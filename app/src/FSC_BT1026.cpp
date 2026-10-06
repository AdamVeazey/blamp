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
    LOG_INF("%s", line.asCString());
}

// Put VER in the unsolicited matches list
MODEM_CHAT_MATCHES_DEFINE(matchesUnsol, matchUnsol);
MODEM_CHAT_MATCHES_DEFINE(matchesAbort, matchERROR);

bool BT1026::
runSingleCommand( const char* cmd, const char* expected ) {
    uint32_t timeout_ms = 3000;
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

    return lastResult == MODEM_CHAT_SCRIPT_RESULT_SUCCESS;
}

BT1026::
BT1026( const device* const uart ) :
    uart(uart),
    backend{},
    chat{},
    uartRx{},
    uartTx{},
    chatRx{},
    chatArgv{}
{
    k_mutex_init( &lock );
    k_sem_init( &syncSem, 0, 1 ); // Start empty, max count 1
}

int BT1026::
init() {
    /* Init script commands */
    MODEM_CHAT_SCRIPT_CMDS_DEFINE(
        initCmds,
        MODEM_CHAT_SCRIPT_CMD_RESP("AT", matchOK),
    );

    /* Script configurations */
    MODEM_CHAT_SCRIPT_DEFINE(
        initScript,
        initCmds,
        matchesAbort,
        []( modem_chat* chat, modem_chat_script_result result, void* self ) {
            auto* instance = static_cast<BT1026*>(self);
            instance->lastResult = result;
            k_sem_give(&instance->syncSem); // WAKE UP INIT!
        },
        5 // Total script timeout in seconds
    );
    int err;

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
    err = modem_chat_init(&chat, &chat_config);
    if (err < 0) return err;

    err = modem_chat_attach(&chat, &backend.pipe);
    if (err < 0) return err;

    err = modem_pipe_open(&backend.pipe);
    if (err < 0) return err;

    // Give the hardware a tiny breathing window just in case
    k_msleep(200);

    // Lock the driver so nothing else can talk to it during init
    if (k_mutex_lock(&lock, K_MSEC(1000)) != 0) {
        LOG_ERR("Failed to lock for init");
        return -EBUSY;
    }

    // Reset and run the init script
    k_sem_reset(&syncSem);
    err = modem_chat_script_run(&chat, &initScript);
    if (err < 0) {
        LOG_ERR("Failed to start init script: %d", err);
        k_mutex_unlock(&lock);
        return err;
    }

    // BLOCK HERE until the init script completes (success or timeout)
    if (k_sem_take(&syncSem, K_SECONDS(5)) != 0) {
        LOG_ERR("BT Init Timeout!");
        modem_chat_script_abort(&chat);
        k_mutex_unlock(&lock);
        return -ETIMEDOUT;
    }

    k_mutex_unlock(&lock);

    if (lastResult != MODEM_CHAT_SCRIPT_RESULT_SUCCESS) {
        LOG_ERR("BT Init Script failed with result: %d", lastResult);
        return -EIO;
    }

    return 0;
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

void BT1026::
setBaudrate( uint32_t baudrate ) {
    runSingleCommand( (EDF::String<33>("AT+BAUD=") + baudrate).asCString() );
}

std::optional<I2SCFG> BT1026::
getI2SCFG() {
    if( !runSingleCommand("AT+I2SCFG", "+I2SCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setI2SCFG( I2SCFG cfg ) {
    runSingleCommand( (EDF::String<33>("AT+I2SCFG=") + cfg).asCString() );
}

std::optional<bool> BT1026::
getSPDIFCFG() {
    if( !runSingleCommand("AT+SPDIFCFG", "+SPDIFCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setSPDIFCFG( bool enable ) {
    runSingleCommand( (EDF::String<33>("AT+SPDIFCFG=") + enable).asCString() );
}

std::optional<int32_t> BT1026::
getMicGain() {
    if( !runSingleCommand("AT+MICGAIN", "+MICGAIN") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toInt32_t();
}

void BT1026::
setMicGain( int32_t gain ) {
    runSingleCommand( (EDF::String<33>("AT+MICGAIN=") + gain).asCString() );
}

std::optional<int32_t> BT1026::
getSpkVol() {
    if( !runSingleCommand("AT+SPKVOL", "+SPKVOL") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toInt32_t();
}

void BT1026::
setSpkVol( int32_t volume ) {
    runSingleCommand( (EDF::String<33>("AT+SPKVOL=") + volume).asCString() );
}

void BT1026::
reboot() {
    runSingleCommand("AT+REBOOT");
}

void BT1026::
factoryReset() {
    runSingleCommand("AT+RESTORE");
}

std::optional<bool> BT1026::
getBluetoothIsOn() {
    if( !runSingleCommand("AT+BTEN", "+BTEN") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setBluetooth( bool on ) {
    runSingleCommand( (EDF::String<33>("AT+BTEN=") + on).asCString() );
}

std::optional<Profile> BT1026::
getProfile() {
    if( !runSingleCommand("AT+PROFILE", "+PROFILE") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint16_t();
}

void BT1026::
setProfile( Profile p ) {
    runSingleCommand( (EDF::String<33>("AT+PROFILE=") + p).asCString() );
}

std::optional<uint8_t> BT1026::
getAutoConn() {
    if( !runSingleCommand("AT+AUTOCONN", "+AUTOCONN") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setAutoConn( uint8_t maxAttempts ) {
    runSingleCommand( (EDF::String<33>("AT+AUTOCONN=") + maxAttempts).asCString() );
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

void BT1026::
setName(const EDF::String<32>& name, bool enableMACSuffix) {
    runSingleCommand((
        EDF::String<40>("AT+NAME=") + name + ',' + enableMACSuffix
    ).asCString());
}

std::optional<EDF::String<32>> BT1026::
getBLEName() {
    if( !runSingleCommand("AT+LENAME", "+LENAME") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

void BT1026::
setBLEName(const EDF::String<32>& name, bool enableMACSuffix) {
    runSingleCommand((
        EDF::String<42>("AT+LENAME=") + name + ',' + enableMACSuffix
    ).asCString());
}

std::optional<SecureSimplePairing> BT1026::
getSPP() {
    if( !runSingleCommand("AT+SSP", "+SSP") || response.argc < 3 )
        return std::nullopt;
    return static_cast<SecureSimplePairing>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
setSPP(SecureSimplePairing m) {
    runSingleCommand( (EDF::String<33>("AT+SPP=") + static_cast<uint8_t>(m)).asCString() );
}

std::optional<EDF::String<16>> BT1026::
getPin() {
    if( !runSingleCommand("AT+PIN", "+PIN") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

void BT1026::
setPin(const EDF::String<16>& pin) {
    runSingleCommand( (EDF::String<33>("AT+PIN=") + pin).asCString() );
}

std::optional<EDF::String<7>> BT1026::
getCOD() {
    if( !runSingleCommand("AT+COD", "+COD") || response.argc < 3 )
        return std::nullopt;
    return response.argv[2];
}

void BT1026::
setCOD(const EDF::String<7>& cod) {
    runSingleCommand( (EDF::String<33>("AT+COD=") + cod).asCString() );
}

std::optional<bool> BT1026::
getAdvertising() {
    if( !runSingleCommand("AT+PAIR", "+PAIR") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setAdvertising(bool start) {
    runSingleCommand( (EDF::String<33>("AT+PAIR=") + start).asCString() );
}

void BT1026::
setScanning(bool start) {
    runSingleCommand( (EDF::String<33>("AT+SCAN=") + start).asCString() );
}

void BT1026::
getPairedList() {
    runSingleCommand( "AT+PLIST" );
}

void BT1026::
delPaired(uint8_t index) {
    runSingleCommand( (EDF::String<33>("AT+PLIST=") + index).asCString() );
}

void BT1026::
delAllConnections() {
    runSingleCommand( "AT+DSCA" );
}

std::optional<bool> BT1026::
getThroughputMode() {
    if( !runSingleCommand("AT+TPMODE", "+TPMODE") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setThroughputMode(bool enable) {
    runSingleCommand( (EDF::String<33>("AT+TPMODE=") + enable).asCString() );
}

std::optional<InputCFG> BT1026::
getInputCFG() {
    if( !runSingleCommand("AT+AUXCFG", "+AUXCFG") || response.argc < 3 )
        return std::nullopt;
    return static_cast<InputCFG>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
setInputCFG(InputCFG in) {
    runSingleCommand( (EDF::String<33>("AT+AUXCFG=") + static_cast<uint8_t>(in)).asCString() );
}

std::optional<bool> BT1026::
getPrint() {
    if( !runSingleCommand("AT+PRINT", "+PRINT") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setPrint(bool enable) {
    runSingleCommand( (EDF::String<33>("AT+PRINT=") + enable).asCString() );
}

void BT1026::
setTone(uint8_t tone) {
    if( tone > 94 ) {
        LOG_ERR("Invalid tone! Choose a value between 0-94");
        return;
    }
    runSingleCommand( (EDF::String<33>("AT+TONEPLAY=") + tone).asCString() );
}

std::optional<uint8_t> BT1026::
getMutePIO() {
    if( !runSingleCommand("AT+MUTEPIO", "+MUTEPIO") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setMutePIO(uint8_t pin) {
    if( pin > 63 ) {
        LOG_ERR("Invalid pin! Choose a value between 0-63");
        return;
    }
    runSingleCommand( (EDF::String<33>("AT+MUTEPIO=") + pin).asCString() );
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

void BT1026::
SPPConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+SPPCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    runSingleCommand( cmd.asCString() );
}

void BT1026::
SPPRelease() {
    runSingleCommand( "AT+SPPDISC" );
}

void BT1026::
SPPSend(const EDF::String<493>& data) {
    // NOTE: uartTx[] limits how much can actually be sent here!
    // TODO: deal with that if you actually use this function
    runSingleCommand( (EDF::String<504>("AT+SPPSEND=") + data).asCString() );
}

std::optional<GATTSTATE> BT1026::
getGATTState() {
    if( !runSingleCommand("AT+GATTSTAT", "+GATTSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<GATTSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
GATTRelease() {
    runSingleCommand( "AT+GATTDISC" );
}

void BT1026::
GATTSend(const EDF::String<493>& data) {
    // NOTE: uartTx[] limits how much can actually be sent here!
    // TODO: deal with that if you actually use this function
    runSingleCommand( (EDF::String<505>("AT+GATTSEND=") + data).asCString() );
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

void BT1026::
setHFPRes(uint16_t hz) {
    // 0-48,000
    if( hz > 48'000 ) {
        LOG_ERR("Invalid hz! Choose a value between 0-48000");
        return;
    }
    runSingleCommand( (EDF::String<33>("AT+HFPRES=") + hz).asCString() );
}

void BT1026::
HFPConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+HFPCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    runSingleCommand( cmd.asCString() );
}

void BT1026::
HFPRelease() {
    runSingleCommand( "AT+HFPDISC" );
}

void BT1026::
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
        runSingleCommand( cmd.asCString() );
    }
    else {
        LOG_ERR("Invalid number!");
    }
}

void BT1026::
HFPDTMF(char dtmf) {
    // 0-9,#,* (dial-tone multi-frequency)
    if( EDF::String<32>("1234567890#*").contains(dtmf) ) {
        runSingleCommand( (EDF::String<32>("AT+HFPDTMF=") + dtmf).asCString() );
    }
    else {
        LOG_ERR("Invalid dial-tone multi-frequency character! Choose one: 1234567890#*");
    }
}

void BT1026::
HFPCallPickup() {
    runSingleCommand( "AT+HFPANSW" );
}

void BT1026::
HFPCallHangup() {
    runSingleCommand( "AT+HFPCHUP" );
}

void BT1026::
HFPTransferAudio(bool deviceToModule) {
    // module -> device if false
    runSingleCommand( (EDF::String<33>("AT+HFPADTS=") + deviceToModule).asCString() );
}

void BT1026::
HFPVoiceRecognition(bool start) {
    runSingleCommand( (EDF::String<33>("AT+HFPVR=") + start).asCString() );
}
void BT1026::
setHFPSCO(HFPSCO config) {
    runSingleCommand( (EDF::String<33>("AT+HFPSCO=") + static_cast<uint8_t>(config)).asCString() );
}

void BT1026::
setHFPBattery(uint8_t level) {
    if( level > 9 ) {
        LOG_ERR("Invalid level! Choose a value between 0-9");
        return;
    }
    runSingleCommand( (EDF::String<33>("AT+HFPRES=") + level).asCString() );
}

std::optional<A2DPSTATE> BT1026::
getA2DPState() {
    if( !runSingleCommand("AT+A2DPSTAT", "+A2DPSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<A2DPSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
A2DPConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+A2DPCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    runSingleCommand( cmd.asCString() );
}

void BT1026::
A2DPRelease() {
    runSingleCommand( "AT+A2DPDISC" );
}

std::optional<A2DPCFG> BT1026::
getA2DPCFG() {
    if( !runSingleCommand("AT+A2DPCFG", "+A2DPCFG") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setA2DPCFG(A2DPCFG config) {
    runSingleCommand( (EDF::String<33>("AT+A2DPCFG=") + config).asCString() );
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

void BT1026::
setA2DPConnection(bool establish) {
    runSingleCommand( (EDF::String<33>("AT+A2DPAUDIO=") + establish).asCString() );
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

void BT1026::
setAVRCPCFG(AVRCPCFG config) {
    runSingleCommand( (EDF::String<33>("AT+AVRCPCFG=") + config).asCString() );
}

void BT1026::
playPause() {
    runSingleCommand( "AT+PLAYPAUSE" );
}

void BT1026::
play() {
    runSingleCommand( "AT+PLAY" );
}

void BT1026::
pause() {
    runSingleCommand( "AT+PAUSE" );
}

void BT1026::
stop() {
    runSingleCommand( "AT+STOP" );
}

void BT1026::
forward() {
    runSingleCommand( "AT+FORWARD" );
}

void BT1026::
backward() {
    runSingleCommand( "AT+BACKWARD" );
}

void BT1026::
fastForward(bool pressDown) {
    runSingleCommand( (EDF::String<33>("AT+FFWD=") + pressDown).asCString() );
}

void BT1026::
rewind(bool pressDown) {
    runSingleCommand( (EDF::String<33>("AT+RWD=") + pressDown).asCString() );
}

std::optional<HIDSTATE> BT1026::
getHIDState() {
    if( !runSingleCommand("AT+HIDSTAT", "+HIDSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<HIDSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
HIDConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+HIDCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    runSingleCommand( cmd.asCString() );
}

void BT1026::
HIDRelease() {
    runSingleCommand( "AT+HIDDISC" );
}

std::optional<HIDMode> BT1026::
getHIDMode() {
    if( !runSingleCommand("AT+HIDMODE", "+HIDMODE") || response.argc < 3 )
        return std::nullopt;
    return static_cast<HIDMode>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
setHIDMode(HIDMode mode) {
    runSingleCommand( (EDF::String<33>("AT+HIDMODE=") + static_cast<uint8_t>(mode)).asCString() );
}

std::optional<bool> BT1026::
getHIDAutoRelease() {
    if( !runSingleCommand("AT+HIDREL", "+HIDREL") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint8_t();
}

void BT1026::
setHIDAutoRelease(bool enable) {
    runSingleCommand( (EDF::String<33>("AT+HIDREL=") + enable).asCString() );
}

std::optional<uint32_t> BT1026::
getHIDDelay() {
    if( !runSingleCommand("AT+HIDDLY", "+HIDDLY") || response.argc < 3 )
        return std::nullopt;
    return EDF::String<33>(response.argv[2]).toUint32_t();
}

void BT1026::
setHIDDelay(uint32_t ms) {
    runSingleCommand( (EDF::String<33>("AT+HIDDLY=") + ms).asCString() );
}

void BT1026::
HIDSend(const EDF::String<33>& key) {
    EDF::String<64> cmd("AT+HIDSEND=");
    cmd += key.length();
    cmd += ',';
    cmd += key;
    runSingleCommand( cmd.asCString() );
}

void BT1026::
HIDCmd(HIDCommand cmd) {
    // TODO: deal with sending hex, runSingleCommand uses std::strlen()
    LOG_ERR("HIDCmd() Not implemented!");
}

std::optional<PBSTATE> BT1026::
getPBState() {
    if( !runSingleCommand("AT+PBSTAT", "+PBSTAT") || response.argc < 3 )
        return std::nullopt;
    return static_cast<PBSTATE>(EDF::String<33>(response.argv[2]).toUint8_t());
}

void BT1026::
PBConnect(const EDF::String<13>& mac) {
    EDF::String<33> cmd("AT+PBCONN");
    if( !mac.isEmpty() ) {
        cmd += '=';
        cmd += mac;
    }
    runSingleCommand( cmd.asCString() );
}

void BT1026::
PBRelease() {
    runSingleCommand( "AT+PBDISC" );
}

void BT1026::
PBDownload(PBDown type) {
    runSingleCommand( (EDF::String<33>("AT+PBDOWN=") + static_cast<uint8_t>(type)).asCString() );
}

void BT1026::
PBDownload(PBDown type, uint16_t maxItems) {
    runSingleCommand((
        EDF::String<33>("AT+PBDOWN=") +
        static_cast<uint8_t>(type) +
        ',' +
        maxItems
    ).asCString() );
}

}; /* FSC */