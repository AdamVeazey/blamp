#include "FSC_BT1026.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fsc_bt1026, LOG_LEVEL_INF);

namespace FSC {

// General Responses
MODEM_CHAT_MATCH_DEFINE(matchOK, "OK", "", [](modem_chat* chat, char** argv, uint16_t argc, void* self) {
    static_cast<BT1026*>(self)->callbackMatchOK( chat, argv, argc );
});

void BT1026::
callbackMatchOK( modem_chat* chat, char** argv, uint16_t argc ) {
    /* Safely check if a script is actively running */
    if( chat != nullptr &&
        chat->script != nullptr &&
        chat->script->script_chats != nullptr &&
        argc >= 1
    ) {
        /* Grab the request string for the current script step */
        auto currentCmd = chat->script->script_chats[chat->script_chat_it].request;
        LOG_DBG("%s: %s", currentCmd, argv[0]);
    }
    else {
        /* argv[0] contains the matched line */
        for( uint16_t k = 0; k < argc; ++k) {
            LOG_WRN("%s", argv[k]);
        }
    }
}

MODEM_CHAT_MATCH_DEFINE(matchERROR, "ERROR", "", [](modem_chat* chat, char** argv, uint16_t argc, void* self) {
    static_cast<BT1026*>(self)->callbackMatchERROR( chat, argv, argc );
});

void BT1026::
callbackMatchERROR( modem_chat* chat, char** argv, uint16_t argc ) {
    /* Safely check if a script is actively running */
    if( chat != nullptr &&
        chat->script != nullptr &&
        chat->script->script_chats != nullptr &&
        argc >= 1
    ) {
        /* Grab the request string for the current script step */
        auto currentCmd = chat->script->script_chats[chat->script_chat_it].request;
        LOG_ERR("%s: %s", currentCmd, argv[0]);
    }
    else {
        /* argv[0] contains the matched line */
        for( uint16_t k = 0; k < argc; ++k) {
            LOG_ERR("%s", argv[k]);
        }
    }
}

// Unsolicted Responses
MODEM_CHAT_MATCH_DEFINE(matchUnsol, "+", "=,", [](modem_chat* chat, char** argv, uint16_t argc, void* self) {
    static_cast<BT1026*>(self)->callbackMatchUnsolicited( chat, argv, argc );
});

void BT1026::
callbackMatchUnsolicited( modem_chat* chat, char** argv, uint16_t argc ) {
    // argv[0] = "+"
    // argv[1] = "<cmd>"
    // argv[2] = "<value>"
    // argv[n] = "[comma seperated values...]"
    if (argc < 2) return;

    EDF::String<32> cmdName(argv[0]);
    cmdName += argv[1];
    if( response.expected.equals(cmdName) ) {
        // store the rest of the stuff
        response.argc = argc;
        for( uint16_t k = 0; k < response.argc; ++k ) {
            response.argv[k] = argv[k];
        }
    }

    EDF::String<256> line(cmdName);
    if (argc > 2) {
        line += "=";
        line += argv[2];
    }
    for( uint16_t k = 3; k < argc; ++k ) {
        line += ",";
        line += argv[k];
    }
    LOG_DBG("%s", line.asCString());
    asyncResponse(argv, argc);
}

// Put VER in the unsolicited matches list
MODEM_CHAT_MATCHES_DEFINE(matchesUnsol, matchUnsol);
MODEM_CHAT_MATCHES_DEFINE(matchesAbort, matchERROR);

bool BT1026::
runSingleCommand( const char* cmd, const char* expected ) {
    uint32_t timeout_ms = 5000;
    LOG_DBG("runSingleCommand() >%s<", cmd);
    if (k_mutex_lock(&lock, K_MSEC(timeout_ms + 500)) != 0) {
        LOG_ERR("Failed to get BT lock");
        return false;
    }
    response.expected = expected;
    response.argc = 0;
    // dynCmdChat = MODEM_CHAT_SCRIPT_CMD_RESP(cmd, matchOK);
    // FIX: Manually populate fields to use strlen(cmd) instead of pointer sizeof()
    modem_chat_script_chat chatCommandOK = {
        .request = reinterpret_cast<const uint8_t*>(cmd),
        .request_size = static_cast<uint16_t>(std::strlen(cmd)),
        .response_matches = &matchOK,
        .response_matches_size = 1,
        .timeout = 0
    };

    modem_chat_script script = {
        .name = "dyn_cmd",
        .script_chats = &chatCommandOK,
        .script_chats_size = 1,
        .abort_matches = matchesAbort,
        .abort_matches_size = ARRAY_SIZE(matchesAbort),
        .callback = [](modem_chat* chat, modem_chat_script_result res, void* user_data) {
            auto* self = static_cast<BT1026*>(user_data);
            self->lastResult = res;
            k_sem_give( &self->syncSem ); // WAKE UP THE WAITING THREAD!
        },
        .timeout = timeout_ms / 1000, // Zephyr script timeouts are in seconds
    };

    k_sem_reset( &syncSem );
    int err = modem_chat_script_run( &chat, &script );

    if( err < 0 ) {
        LOG_ERR("Failed to start chat script: %d", err);
        k_mutex_unlock( &lock );
        return false;
    }

    // GO TO SLEEP.
    // The calling thread pauses here without wasting CPU cycles.
    // It wakes up when the callback calls k_sem_give(), or if it times out.
    if( k_sem_take( &syncSem, K_MSEC(timeout_ms + 1000) ) != 0 ) {
        LOG_ERR("BT Command Timeout: %s", cmd);
        modem_chat_script_abort( &chat );
        k_mutex_unlock( &lock );
        return false;
    }

    k_mutex_unlock( &lock );
    if( lastResult != MODEM_CHAT_SCRIPT_RESULT_SUCCESS ) {
        LOG_WRN("runSingleCommand() cmd: %s lastResult: %s",
            cmd,
            lastResult == MODEM_CHAT_SCRIPT_RESULT_TIMEOUT ? "Timeout" : "Abort"
        );
    }

    return lastResult == MODEM_CHAT_SCRIPT_RESULT_SUCCESS;
}

void BT1026::
asyncResponse( char** argv, uint16_t argc ) {
    // argv[0] = "+"
    // argv[1] = "<cmd>"
    // argv[2] = "<value>"
    // argv[n] = "[comma seperated values...]"
    if( argc < 2 ) return;
    EDF::String<32> cmd(argv[1]);
    if( cmd.equals("SCAN") ) {
        if( (argc == 3) && (argv[2][0] == 'E')) {
            if( events.onScanResult ) {
                events.onScanResult( -1, 0, "", 0, 0, nullptr );
            }
            LOG_DBG("End of scan list");
            return;
        }
        else if( argc < 7 || !events.onScanResult ) return;
        uint8_t dataLen = EDF::String<32>(argv[6]).toUint8_t();
        events.onScanResult(
            EDF::String<32>(argv[2]).toInt16_t(),
            EDF::String<32>(argv[3]).toUint8_t(),
            argv[4],
            EDF::String<32>(argv[5]).toInt8_t(),
            dataLen,
            (argc >= 8 && dataLen > 0) ? reinterpret_cast<const uint8_t*>(argv[7]) : nullptr
        );
    }
    else if( cmd.equals("PAIRED") ) {
        if( argc < 3 || !events.onPaired ) return;
        events.onPaired( argv[2] );
    }
    else if( cmd.equals("DEVSTAT") ) {
        if( argc < 3 || !events.onStateDevice ) return;
        events.onStateDevice( EDF::String<32>(argv[2]).toUint8_t() );
    }
    else if( cmd.equals("A2DPSTAT") ) {
        if( argc < 3 || !events.onStateA2DP ) return;
        events.onStateA2DP(
            static_cast<A2DPSTATE>(EDF::String<32>(argv[2]).toUint8_t())
        );
    }
    else if( cmd.equals("A2DPDEV") ) {
        if( argc < 3 || !events.onDeviceA2DPConnected ) return;
        if( argc < 4 ) {
            events.onDeviceA2DPConnected( argv[2], nullptr );
            return;
        }
        events.onDeviceA2DPConnected( argv[2], argv[3] );
    }
    else if( cmd.equals("AVRCPSTAT") ) {
        if( argc < 3 || !events.onStateAVRCP ) return;
        events.onStateAVRCP(
            static_cast<AVRCPSTATE>(EDF::String<32>(argv[2]).toUint8_t())
        );
    }
    else if( cmd.equals("PLAYSTAT") ) {
        if( argc < 3 || !events.onStatePlay ) return;
        events.onStatePlay(
            static_cast<PlayState>(EDF::String<32>(argv[2]).toUint8_t())
        );
    }
    else if( cmd.equals("TRACKSTAT") ) {
        if( argc < 5 || !events.onStateTrack ) return;
        events.onStateTrack(
            TrackState{
                .playState = static_cast<PlayState>(EDF::String<32>(argv[2]).toUint8_t()),
                .elapsedTime_ms = EDF::String<33>(argv[3]).toUint32_t(),
                .totalTime_ms = EDF::String<33>(argv[4]).toUint32_t()
            }
        );
    }
    else if( cmd.equals("TRACKINFO") ) {
        static TrackInfo ti;
        if( argc < 3 || !events.onTrackInfo ) return;
        ti.title = argv[2];
        if( argc < 4 ) {
            ti.artist.clear();
            ti.album.clear();
            events.onTrackInfo( ti );
            return;
        }
        ti.artist = argv[3];
        if( argc < 5 ) {
            ti.album.clear();
            events.onTrackInfo( ti );
            return;
        }
        ti.album = argv[4];
        events.onTrackInfo( ti );
    }
}

BT1026::
BT1026( const device* const uart, const Events& events ) :
    uart(uart),
    backend{},
    chat{},
    uartRx{},
    chatRx{},
    uartTx{},
    chatArgv{},
    events{events}
{
    k_mutex_init( &lock );
    k_sem_init( &syncSem, 0, 1 ); // Start empty, max count 1
}

int BT1026::
init() {
    if (!device_is_ready(uart)) {
        LOG_ERR("Modem UART not ready");
        return -ENODEV;
    }

    const struct modem_backend_uart_config backend_config = {
        .uart = uart,
        .receive_buf = uartRx,
        .receive_buf_size = sizeof(uartRx),
        .transmit_buf = uartTx,
        .transmit_buf_size = sizeof(uartTx),
    };
    modem_backend_uart_init(&backend, &backend_config);

    const struct modem_chat_config chat_config = {
        .user_data = this,
        .receive_buf = chatRx,
        .receive_buf_size = sizeof(chatRx),
        .delimiter = (uint8_t*)"\r\n",
        .delimiter_size = 2,
        .filter = nullptr,
        .filter_size = 0,
        .argv = chatArgv,
        .argv_size = ARRAY_SIZE(chatArgv),
        .unsol_matches = matchesUnsol,
        .unsol_matches_size = ARRAY_SIZE(matchesUnsol),
    };

    int err = modem_chat_init(&chat, &chat_config);
    if (err < 0) return err;

    err = modem_chat_attach(&chat, &backend.pipe);
    if (err < 0) return err;

    err = modem_pipe_open(&backend.pipe);
    if (err < 0) return err;

    // Give the hardware a tiny breathing window just in case
    k_msleep(200);

    bool ok = runSingleCommand("AT");
    return !ok; // 0 == success
}

std::optional<Version> BT1026::
getVersion() {
    // argv[0] = "+", argv[1]="VER", argv[2] = "FSC-BT1026E", argv[3] = "V4.9.2", argv[4] = "20230706"
    // We need at least 5 arguments (indices 0 through 4)
    if( !runSingleCommand("AT+VER", "+VER") || response.argc < 5 )
        return std::nullopt;
    return Version{
        .module = response.argv[2],
        .version = response.argv[3],
        .date = response.argv[4]
    };
}

std::optional<uint32_t> BT1026::
getBaudrate() {
    if( !runSingleCommand("AT+BAUD", "+BAUD") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint32_t();
}

bool BT1026::
setBaudrate( uint32_t baudrate ) {
    auto value = getBaudrate();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == baudrate ) return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+BAUD=") + baudrate).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getBaudrate();
    return value.has_value() && value.value() == baudrate;
}

std::optional<I2SCFG> BT1026::
getI2SCFG() {
    if( !runSingleCommand("AT+I2SCFG", "+I2SCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setI2SCFG( I2SCFG cfg ) {
    auto value = getI2SCFG();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == cfg )  return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+I2SCFG=") + cfg).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getI2SCFG();
    return value.has_value() && value.value() == cfg;
}

std::optional<bool> BT1026::
getSPDIFCFG() {
    if( !runSingleCommand("AT+SPDIFCFG", "+SPDIFCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setSPDIFCFG( bool enable ) {
    auto value = getSPDIFCFG();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == enable )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+SPDIFCFG=") + enable).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getSPDIFCFG();
    return value.has_value() && value.value() == enable;
}

std::optional<int32_t> BT1026::
getMicGain() {
    if( !runSingleCommand("AT+MICGAIN", "+MICGAIN") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toInt32_t();
}

bool BT1026::
setMicGain( int32_t gain ) {
    auto value = getMicGain();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == gain ) return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+MICGAIN=") + gain).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getMicGain();
    return value.has_value() && value.value() == gain;
}

std::optional<int32_t> BT1026::
getSpkVol() {
    if( !runSingleCommand("AT+SPKVOL", "+SPKVOL") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toInt32_t();
}

bool BT1026::
setSpkVol( int32_t volume ) {
    auto value = getSpkVol();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == volume )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+SPKVOL=") + volume).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getSpkVol();
    return value.has_value() && value.value() == volume;
}

bool BT1026::
reboot() {
    return runSingleCommand ("AT+REBOOT");
}

bool BT1026::
factoryReset() {
    return runSingleCommand ("AT+RESTORE");
}

std::optional<bool> BT1026::
getBluetoothIsOn() {
    if( !runSingleCommand("AT+BTEN", "+BTEN") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setBluetooth( bool on ) {
    auto value = getBluetoothIsOn();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == on )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+BTEN=") + on).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getBluetoothIsOn();
    return value.has_value() && value.value() == on;
}

std::optional<Profile> BT1026::
getProfile() {
    if( !runSingleCommand("AT+PROFILE", "+PROFILE") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint16_t();
}

bool BT1026::
setProfile( Profile p ) {
    auto value = getProfile();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == p )    return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+PROFILE=") + p).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getProfile();
    return value.has_value() && value.value() == p;
}

std::optional<uint8_t> BT1026::
getAutoConn() {
    if( !runSingleCommand("AT+AUTOCONN", "+AUTOCONN") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setAutoConn( uint8_t maxAttempts ) {
    auto value = getAutoConn();
    if( !value.has_value() )            return false;   // couldn't read value
    if( value.value() == maxAttempts )  return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+AUTOCONN=") + maxAttempts).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getAutoConn();
    return value.has_value() && value.value() == maxAttempts;
}

std::optional<STATES> BT1026::
getAllStates() {
    // "+", "STAT", "DEVSTAT", "SPPSTAT", "GATTSTAT", "HFPSTAT", "A2DPSTAT", "AVRCPSTATE", "HIDSTAT", "PBSTAT"
    if( !runSingleCommand("AT+STAT", "+STAT") || response.argc < 10 )
        return std::nullopt;
    using S = EDF::String<33>;
    return STATES {
        .dev   =                          S(response.argv[2]).toUint8_t(),
        .spp   = static_cast<SPPSTATE>(   S(response.argv[3]).toUint8_t() ),
        .gatt  = static_cast<GATTSTATE>(  S(response.argv[4]).toUint8_t() ),
        .hfp   = static_cast<HFPSTATE>(   S(response.argv[5]).toUint8_t() ),
        .a2dp  = static_cast<A2DPSTATE>(  S(response.argv[6]).toUint8_t() ),
        .avrcp = static_cast<AVRCPSTATE>( S(response.argv[7]).toUint8_t() ),
        .hid   = static_cast<HIDSTATE>(   S(response.argv[8]).toUint8_t() ),
        .pb    = static_cast<PBSTATE>(    S(response.argv[9]).toUint8_t() ),
    };
}

std::optional<DEVSTATE> BT1026::
getDevState() {
    if( !runSingleCommand("AT+DEVSTAT", "+DEVSTAT") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

std::optional<EDF::String<13>> BT1026::
getAddress() {
    if( !runSingleCommand("AT+ADDR", "+ADDR") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

std::optional<EDF::String<13>> BT1026::
getBLEAddress() {
    if( !runSingleCommand("AT+LEADDR", "+LEADDR") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

std::optional<EDF::String<32>> BT1026::
getName() {
    if( !runSingleCommand("AT+NAME", "+NAME") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

bool BT1026::
setName(const EDF::String<32>& name, bool enableMACSuffix) {
    auto value = getName().value_or("");
    if( value.isEmpty() ) return false; // couldn't read value
    auto mac = getAddress().value_or("");
    if( mac.isEmpty() && mac.length() >= 4 ) return false; // couldn't read value
    auto last4Mac = mac.getSubString(mac.end()-4, mac.end());

    if( !enableMACSuffix ) {
        if( value.equals(name) ) return true; // already set to desired value
    }
    else if( value.length() >= 4 ) { // current value is long enough to hold MAC
        auto last4Name = value.getSubString(value.end()-4, value.end());
        auto currentName = value.getSubString(value.begin(), value.end()-4);
        if( currentName.equals(name) && last4Name.equals(last4Mac) ) return true; // already set to desired value
    }

    bool ret = runSingleCommand ((
        EDF::String<40>("AT+NAME=") + name + ',' + enableMACSuffix
    ).asCString());
    if( !ret ) return false; // timeout or some other failure

    value = getName().value_or("");
    if( value.isEmpty() ) return false;
    if( !enableMACSuffix ){
        return value.equals(name);
    }
    else {
        if( value.length() < 4 ) return false; // doesn't have MAC!!
        auto last4Name = value.getSubString(value.end()-4, value.end());
        auto currentName = value.getSubString(value.begin(), value.end()-4);
        return currentName.equals(name) && last4Name.equals(last4Mac);
    }
}

std::optional<EDF::String<32>> BT1026::
getBLEName() {
    if( !runSingleCommand("AT+LENAME", "+LENAME") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

bool BT1026::
setBLEName(const EDF::String<32>& name, bool enableMACSuffix) {
    auto value = getBLEName().value_or("");
    if( value.isEmpty() ) return false; // couldn't read value
    auto mac = getBLEAddress().value_or("");
    if( mac.isEmpty() ) return false; // couldn't read value
    auto last4Mac = mac.getSubString(mac.end()-4, mac.end());

    if( !enableMACSuffix ) {
        if( value.equals(name) ) return true; // already set to desired value
    }
    else if( value.length() >= 4 ) { // current value is long enough to hold MAC
        auto last4Name = value.getSubString(value.end()-4, value.end());
        auto currentName = value.getSubString(value.begin(), value.end()-4);
        if( currentName.equals(name) && last4Name.equals(last4Mac) ) return true; // already set to desired value
    }

    bool ret = runSingleCommand ((
        EDF::String<42>("AT+LENAME=") + name + ',' + enableMACSuffix
    ).asCString());
    if( !ret ) return false; // timeout or some other failure

    value = getBLEName().value_or("");
    if( value.isEmpty() ) return false;
    if( !enableMACSuffix ){
        return value.equals(name);
    }
    else {
        if( value.length() < 4 ) return false; // doesn't have MAC!!
        auto last4Name = value.getSubString(value.end()-4, value.end());
        auto currentName = value.getSubString(value.begin(), value.end()-4);
        return currentName.equals(name) && last4Name.equals(last4Mac);
    }
}

std::optional<SecureSimplePairing> BT1026::
getSPP() {
    if( !runSingleCommand("AT+SSP", "+SSP") || response.argc < 3 )
        return std::nullopt;
    return static_cast<SecureSimplePairing>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
setSPP(SecureSimplePairing m) {
    auto value = getSPP();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == m )    return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+SPP=") + static_cast<uint8_t>(m)).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getSPP();
    return value.has_value() && value.value() == m;
}

std::optional<EDF::String<16>> BT1026::
getPin() {
    if( !runSingleCommand("AT+PIN", "+PIN") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

bool BT1026::
setPin(const EDF::String<16>& pin) {
    auto value = getPin();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value().equals(pin) ) return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+PIN=") + pin).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getPin();
    return value.has_value() && value.value().equals(pin);
}

std::optional<EDF::String<7>> BT1026::
getCOD() {
    if( !runSingleCommand("AT+COD", "+COD") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

bool BT1026::
setCOD(const EDF::String<7>& cod) {
    auto value = getCOD();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value().equals(cod) ) return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+COD=") + cod).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getCOD();
    return value.has_value() && value.value().equals(cod);
}

std::optional<bool> BT1026::
getAdvertising() {
    if( !runSingleCommand("AT+PAIR", "+PAIR") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setAdvertising(bool start) {
    auto value = getAdvertising();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == start )    return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+PAIR=") + start).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getAdvertising();
    return value.has_value() && value.value() == start;
}

bool BT1026::
setScanning(bool start) {
    // TODO: this generates a bunch of respones. Implement dealing with that later
    return runSingleCommand ( (EDF::String<33>("AT+SCAN=") + start).asCString() );
}

bool BT1026::
getPairedList() {
    // TODO: this generates a bunch of respones. Implement dealing with that later
    return runSingleCommand ( "AT+PLIST" );
}

bool BT1026::
delPaired(uint8_t index) {
    return runSingleCommand ( (EDF::String<33>("AT+PLIST=") + index).asCString() );
}

bool BT1026::
delAllConnections() {
    return runSingleCommand ( "AT+DSCA" );
}

std::optional<bool> BT1026::
getThroughputMode() {
    if( !runSingleCommand("AT+TPMODE", "+TPMODE") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setThroughputMode(bool enable) {
    auto value = getThroughputMode();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == enable )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+TPMODE=") + enable).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getThroughputMode();
    return value.has_value() && value.value() == enable;
}

std::optional<InputCFG> BT1026::
getInputCFG() {
    if( !runSingleCommand("AT+AUXCFG", "+AUXCFG") || response.argc < 3 )
        return std::nullopt;
    return static_cast<InputCFG>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
setInputCFG(InputCFG in) {
    auto value = getInputCFG();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == in )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+AUXCFG=") + static_cast<uint8_t>(in)).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getInputCFG();
    return value.has_value() && value.value() == in;
}

std::optional<bool> BT1026::
getPrint() {
    if( !runSingleCommand("AT+PRINT", "+PRINT") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setPrint(bool enable) {
    auto value = getPrint();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == enable )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+PRINT=") + enable).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getPrint();
    return value.has_value() && value.value() == enable;
}

bool BT1026::
setTone(uint8_t tone) {
    if( tone > 94 ) {
        LOG_ERR("Invalid tone! Choose a value between 0-94");
        return false;
    }
    return runSingleCommand ( (EDF::String<33>("AT+TONEPLAY=") + tone).asCString() );
}

std::optional<uint8_t> BT1026::
getMutePIO() {
    if( !runSingleCommand("AT+MUTEPIO", "+MUTEPIO") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setMutePIO(uint8_t pin) {
    if( pin > 63 ) {
        LOG_ERR("Invalid pin! Choose a value between 0-63");
        return false;
    }
    auto value = getMutePIO();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == pin )  return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+MUTEPIO=") + pin).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getMutePIO();
    return value.has_value() && value.value() == pin;
}

std::optional<bool> BT1026::
setMicMute(bool enable) {
    if( !runSingleCommand( (EDF::String<33>("AT+MICMUTE=") + enable).asCString(), "+MICMUTED" ) || response.argc < 3 ) {
        LOG_ERR("resonse.argc: %u", response.argc);
        return std::nullopt;
    }
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

std::optional<bool> BT1026::
setSpkMute(bool enable) {
    if( !runSingleCommand( (EDF::String<33>("AT+SPKMUTE=") + enable).asCString(), "+SPKMUTED" ) || response.argc < 3 ) {
        LOG_ERR("resonse.argc: %u", response.argc);
        return std::nullopt;
    }
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

std::optional<SPPSTATE> BT1026::
getSPPState() {
    if( !runSingleCommand("AT+SPPSTAT", "+SPPSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<SPPSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
SPPConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+SPPCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    return runSingleCommand ( cmd.asCString() );
}

bool BT1026::
SPPRelease() {
    return runSingleCommand ( "AT+SPPDISC" );
}

bool BT1026::
SPPSend(const EDF::String<493>& data) {
    // NOTE: uartTx[] limits how much can actually be sent here!
    // TODO: deal with that if you actually use this function
    return runSingleCommand ( (EDF::String<504>("AT+SPPSEND=") + data).asCString() );
}

std::optional<GATTSTATE> BT1026::
getGATTState() {
    if( !runSingleCommand("AT+GATTSTAT", "+GATTSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<GATTSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
GATTRelease() {
    return runSingleCommand ( "AT+GATTDISC" );
}

bool BT1026::
GATTSend(const EDF::String<493>& data) {
    // NOTE: uartTx[] limits how much can actually be sent here!
    // TODO: deal with that if you actually use this function
    return runSingleCommand ( (EDF::String<505>("AT+GATTSEND=") + data).asCString() );
}

std::optional<HFPSTATE> BT1026::
getHFPState() {
    if( !runSingleCommand("AT+HFPSTAT", "+HFPSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<HFPSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

std::optional<uint16_t> BT1026::
getHFPRes() {
    if( !runSingleCommand("AT+HFPRES", "+HFPRES") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint16_t();
}

bool BT1026::
setHFPRes(uint16_t hz) {
    // 0-48,000
    if( hz > 48'000 ) {
        LOG_ERR("Invalid hz! Choose a value between 0-48000");
        return false;
    }
    auto value = getHFPRes();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == hz )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+HFPRES=") + hz).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getHFPRes();
    return value.has_value() && value.value() == hz;
}

bool BT1026::
HFPConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+HFPCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    return runSingleCommand ( cmd.asCString() );
}

bool BT1026::
HFPRelease() {
    return runSingleCommand ( "AT+HFPDISC" );
}

bool BT1026::
HFPDial(const EDF::String<26>& phoneNumber) {
    // +123 (123) 456-7890
    EDF::String<37> cmd("AT+HFPDIAL=");
    for( const auto& ch : phoneNumber ) {
        if( (ch >= '0' && ch <= '9') || ch == '+' ) {
            cmd += ch;
        }
    }
    if( !cmd.equals("AT+HFPDIAL=") ) {
        // added at least one number...
        return runSingleCommand ( cmd.asCString() );
    }
    else {
        LOG_ERR("Invalid number!");
    }
    return false;
}

bool BT1026::
HFPDTMF(char dtmf) {
    // 0-9,#,* (dial-tone multi-frequency)
    if( EDF::String<32>("1234567890#*").contains(dtmf) ) {
        return runSingleCommand ( (EDF::String<32>("AT+HFPDTMF=") + dtmf).asCString() );
    }
    else {
        LOG_ERR("Invalid dial-tone multi-frequency character! Choose one: 1234567890#*");
    }
    return false;
}

bool BT1026::
HFPCallPickup() {
    return runSingleCommand ( "AT+HFPANSW" );
}

bool BT1026::
HFPCallHangup() {
    return runSingleCommand ( "AT+HFPCHUP" );
}

bool BT1026::
HFPTransferAudio(bool deviceToModule) {
    // module -> device if false
    return runSingleCommand ( (EDF::String<33>("AT+HFPADTS=") + deviceToModule).asCString() );
}

bool BT1026::
HFPVoiceRecognition(bool start) {
    return runSingleCommand ( (EDF::String<33>("AT+HFPVR=") + start).asCString() );
}
bool BT1026::
setHFPSCO(HFPSCO config) {
    return runSingleCommand ( (EDF::String<33>("AT+HFPSCO=") + static_cast<uint8_t>(config)).asCString() );
}

bool BT1026::
setHFPBattery(uint8_t level) {
    if( level > 9 ) {
        LOG_ERR("Invalid level! Choose a value between 0-9");
        return false;
    }
    return runSingleCommand ( (EDF::String<33>("AT+HFPRES=") + level).asCString() );
}

std::optional<A2DPSTATE> BT1026::
getA2DPState() {
    if( !runSingleCommand("AT+A2DPSTAT", "+A2DPSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<A2DPSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
A2DPConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+A2DPCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    return runSingleCommand ( cmd.asCString() );
}

bool BT1026::
A2DPRelease() {
    return runSingleCommand ( "AT+A2DPDISC" );
}

std::optional<A2DPCFG> BT1026::
getA2DPCFG() {
    if( !runSingleCommand("AT+A2DPCFG", "+A2DPCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setA2DPCFG(A2DPCFG config) {
    auto value = getA2DPCFG();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == config )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+A2DPCFG=") + config).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getA2DPCFG();
    return value.has_value() && value.value() == config;
}

std::optional<A2DPDecoder> BT1026::
getA2DPDecoder() {
    if( !runSingleCommand("AT+A2DPDEC", "+A2DPDEC") || response.argc < 3 )
        return std::nullopt;
    return static_cast<A2DPDecoder>(EDF::String<33>(response.argv[2]).toUint8_t());
}

std::optional<A2DPEncoder> BT1026::
getA2DPEncoder() {
    if( !runSingleCommand("AT+A2DPENC", "+A2DPENC") || response.argc < 3 )
        return std::nullopt;
    return static_cast<A2DPDecoder>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
setA2DPConnection(bool establish) {
    return runSingleCommand( (EDF::String<33>("AT+A2DPAUDIO=") + establish).asCString() );
}

std::optional<AVRCPSTATE> BT1026::
getAVRCPState() {
    if( !runSingleCommand("AT+AVRCPSTAT", "+AVRCPSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<AVRCPSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

std::optional<AVRCPCFG> BT1026::
getAVRCPCFG() {
    if( !runSingleCommand("AT+AVRCPCFG", "+AVRCPCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setAVRCPCFG(AVRCPCFG config) {
    auto value = getAVRCPCFG();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == config )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+AVRCPCFG=") + config).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getAVRCPCFG();
    return value.has_value() && value.value() == config;
}

bool BT1026::
playPause() {
    return runSingleCommand( "AT+PLAYPAUSE" );
}

bool BT1026::
play() {
    return runSingleCommand( "AT+PLAY" );
}

bool BT1026::
pause() {
    return runSingleCommand( "AT+PAUSE" );
}

bool BT1026::
stop() {
    return runSingleCommand( "AT+STOP" );
}

bool BT1026::
forward() {
    return runSingleCommand( "AT+FORWARD" );
}

bool BT1026::
backward() {
    return runSingleCommand( "AT+BACKWARD" );
}

bool BT1026::
fastForward(bool pressDown) {
    return runSingleCommand( (EDF::String<33>("AT+FFWD=") + pressDown).asCString() );
}

bool BT1026::
rewind(bool pressDown) {
    return runSingleCommand( (EDF::String<33>("AT+RWD=") + pressDown).asCString() );
}

std::optional<HIDSTATE> BT1026::
getHIDState() {
    if( !runSingleCommand("AT+HIDSTAT", "+HIDSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<HIDSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
HIDConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+HIDCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    return runSingleCommand( cmd.asCString() );
}

bool BT1026::
HIDRelease() {
    return runSingleCommand( "AT+HIDDISC" );
}

std::optional<HIDMode> BT1026::
getHIDMode() {
    if( !runSingleCommand("AT+HIDMODE", "+HIDMODE") || response.argc < 3 )
        return std::nullopt;
    return static_cast<HIDMode>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
setHIDMode(HIDMode mode) {
    auto value = getHIDMode();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == mode ) return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+HIDMODE=") + static_cast<uint8_t>(mode)).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getHIDMode();
    return value.has_value() && value.value() == mode;
}

std::optional<bool> BT1026::
getHIDAutoRelease() {
    if( !runSingleCommand("AT+HIDREL", "+HIDREL") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

bool BT1026::
setHIDAutoRelease(bool enable) {
    auto value = getHIDAutoRelease();
    if( !value.has_value() )        return false;   // couldn't read value
    if( value.value() == enable )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+HIDREL=") + enable).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getHIDAutoRelease();
    return value.has_value() && value.value() == enable;
}

std::optional<uint32_t> BT1026::
getHIDDelay() {
    if( !runSingleCommand("AT+HIDDLY", "+HIDDLY") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint32_t();
}

bool BT1026::
setHIDDelay(uint32_t ms) {
    auto value = getHIDDelay();
    if( !value.has_value() )    return false;   // couldn't read value
    if( value.value() == ms )   return true;    // already set to desired value

    if( !runSingleCommand( (EDF::String<33>("AT+HIDDLY=") + ms).asCString() ) ) {
        return false; // timeout or some other failure
    }
    value = getHIDDelay();
    return value.has_value() && value.value() == ms;
}

bool BT1026::
HIDSend(const EDF::String<33>& key) {
    EDF::String<64> cmd("AT+HIDSEND=");
    cmd += key.length();
    cmd += ',';
    cmd += key;
    return runSingleCommand( cmd.asCString() );
}

bool BT1026::
HIDCmd(HIDCommand cmd) {
    // TODO: deal with sending hex, runSingleCommand uses std::strlen()
    LOG_ERR("HIDCmd() Not implemented!");
    return false;
}

std::optional<PBSTATE> BT1026::
getPBState() {
    if( !runSingleCommand("AT+PBSTAT", "+PBSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<PBSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

bool BT1026::
PBConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+PBCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    return runSingleCommand( cmd.asCString() );
}

bool BT1026::
PBRelease() {
    return runSingleCommand( "AT+PBDISC" );
}

bool BT1026::
PBDownload(PBDown type) {
    return runSingleCommand( (EDF::String<33>("AT+PBDOWN=") + static_cast<uint8_t>(type)).asCString() );
}

bool BT1026::
PBDownload(PBDown type, uint16_t maxItems) {
    return runSingleCommand((
        EDF::String<33>("AT+PBDOWN=") +
        static_cast<uint8_t>(type) +
        ',' +
        maxItems
    ).asCString() );
}

}; /* FSC */