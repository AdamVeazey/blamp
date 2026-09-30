#include "FSC_BT1026.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fsc_bt1026, LOG_LEVEL_INF);

// General Responses
MODEM_CHAT_MATCH_DEFINE(matchOK, "OK", "", [](modem_chat* chat, char** argv, uint16_t argc, void* self) {
    static_cast<FSC_BT1026*>(self)->callbackMatchOK( chat, argv, argc );
});

void FSC_BT1026::
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
    static_cast<FSC_BT1026*>(self)->callbackMatchERROR( chat, argv, argc );
});

void FSC_BT1026::
callbackMatchERROR( modem_chat* chat, char** argv, uint16_t argc ) {
    // do the same thing as OK callback
    callbackMatchOK( chat, argv, argc );
}

// Unsolicted Responses
MODEM_CHAT_MATCH_DEFINE(matchUnsol, "+", "=,", [](modem_chat* chat, char** argv, uint16_t argc, void* self) {
    static_cast<FSC_BT1026*>(self)->callbackMatchUnsolicited( chat, argv, argc );
});

void FSC_BT1026::
callbackMatchUnsolicited( modem_chat* chat, char** argv, uint16_t argc ) {
    // argv[0] = "+"
    // argv[1] = "<cmd>"
    // argv[2] = "<value>"
    // argv[n] = "[comma seperated values...]"
    if (argc < 2) return;

    lastResponse.argc = std::min(argc, (uint16_t)6);
    for (uint16_t i = 0; i < lastResponse.argc; ++i) {
        if (argv[i] != nullptr) {
            snprintf(lastResponse.argv[i], sizeof(lastResponse.argv[i]), "%s", argv[i]);
        } else {
            lastResponse.argv[i][0] = '\0';
        }
    }

    char logBuf[256]; // 256 is safe since chatRx ring buffer is only 256
    char* ptr = logBuf;
    size_t space = sizeof(logBuf);
    // Write argv[0] ("+") and argv[1] ("CMD")
    int w = snprintf(ptr, space, "%s%s", argv[0], argv[1]);
    ptr += w; space -= w;
    // Write the first argument with an '='
    if (argc > 2) {
        w = snprintf(ptr, space, "=%s", argv[2]);
        ptr += w; space -= w;
    }
    // Write any remaining arguments separated by ','
    for( uint16_t k = 3; k < argc; ++k ) {
        w = snprintf(ptr, space, ",%s", argv[k]);
        ptr += w; space -= w;
    }
    LOG_INF("%s", logBuf);
}

// Put VER in the unsolicited matches list
MODEM_CHAT_MATCHES_DEFINE(
    matchesUnsol,
    matchUnsol
);
MODEM_CHAT_MATCHES_DEFINE(matchesAbort, matchERROR);

bool FSC_BT1026::
runSingleCommand( const char* cmd, uint32_t timeout_ms ) {
    if (k_mutex_lock(&lock, K_MSEC(timeout_ms)) != 0) {
        LOG_ERR("Failed to get BT lock");
        return false;
    }

    // dynCmdChat = MODEM_CHAT_SCRIPT_CMD_RESP(cmd, matchOK);
    // FIX: Manually populate fields to use strlen(cmd) instead of pointer sizeof()
    dynCmdChat.request = reinterpret_cast<const uint8_t*>(cmd);
    dynCmdChat.request_size = std::strlen(cmd);
    dynCmdChat.response_matches = &matchOK;
    dynCmdChat.response_matches_size = 1;
    dynCmdChat.timeout = 0;

    dynCmdScript = {
        .name = "dyn_cmd",
        .script_chats = &dynCmdChat,
        .script_chats_size = 1,
        .abort_matches = matchesAbort,
        .abort_matches_size = ARRAY_SIZE(matchesAbort),
        .callback = [](modem_chat* chat, modem_chat_script_result res, void* self) {
            auto* instance = static_cast<FSC_BT1026*>(self);
            instance->lastResult = res;
            k_sem_give( &instance->syncSem ); // WAKE UP THE WAITING THREAD!
        },
        .timeout = timeout_ms / 1000, // Zephyr script timeouts are in seconds
    };

    k_sem_reset( &syncSem );
    int err = modem_chat_script_run( &chat, &dynCmdScript );

    if( err < 0 ) {
        LOG_ERR("Failed to start chat script: %d", err);
        k_mutex_unlock( &lock );
        return false;
    }

    // GO TO SLEEP.
    // The calling thread pauses here without wasting CPU cycles.
    // It wakes up when the callback calls k_sem_give(), or if it times out.
    if( k_sem_take( &syncSem, K_MSEC(timeout_ms) ) != 0 ) {
        LOG_ERR("BT Command Timeout: %s", cmd);
        modem_chat_script_abort( &chat );
        k_mutex_unlock( &lock );
        return false;
    }

    k_mutex_unlock( &lock );

    return (lastResult == MODEM_CHAT_SCRIPT_RESULT_SUCCESS);
}

void FSC_BT1026::
setNumber( const char* cmd, uint32_t val ) {
    snprintf(cmdBuffer, sizeof(cmdBuffer), "%s=%u", cmd, val);
    runSingleCommand(cmdBuffer);
}

std::optional<bool> FSC_BT1026::
getBool( const char* cmd ) {
    if (!runSingleCommand( cmd ) || lastResponse.argc < 3) return std::nullopt;
    return lastResponse.argv[2][0] == '1';
}

FSC_BT1026::
FSC_BT1026( const device* const uart ) :
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

int FSC_BT1026::init() {
    /* Init script commands */
    MODEM_CHAT_SCRIPT_CMDS_DEFINE(
        initCmds,
        MODEM_CHAT_SCRIPT_CMD_RESP("AT", matchOK),
        MODEM_CHAT_SCRIPT_CMD_RESP("AT+MICGAIN", matchOK),
        MODEM_CHAT_SCRIPT_CMD_RESP("AT+SPKVOL", matchOK),
        MODEM_CHAT_SCRIPT_CMD_RESP("AT+AUTOCONN", matchOK),
        MODEM_CHAT_SCRIPT_CMD_RESP("AT+STAT", matchOK),
    );

    /* Script configurations */
    MODEM_CHAT_SCRIPT_DEFINE(
        initScript,
        initCmds,
        matchesAbort,
        []( modem_chat* chat, modem_chat_script_result result, void* self ) {
            auto* instance = static_cast<FSC_BT1026*>(self);
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

std::optional<ResponseVersion> FSC_BT1026::
getVersion() {
    if( !runSingleCommand("AT+VER") ) {
        LOG_ERR("AT+VER command failed or timed out");
        return std::nullopt;
    }
    // argv[0] = "+", argv[1] = "VER", argv[2] = "FSC-BT1026E", argv[3] = "V4.9.2", argv[4] = "20230706"
    // We need at least 5 arguments (indices 0 through 4)
    if( lastResponse.argc < 5 ) {
        LOG_ERR("Incomplete AT+VER response. Expected 5 args, got %d", lastResponse.argc);
        return std::nullopt;
    }
    ResponseVersion r{};
    snprintf(r.module, sizeof(r.module), "%s", lastResponse.argv[2]);
    snprintf(r.version, sizeof(r.version), "%s", lastResponse.argv[3]);
    snprintf(r.date, sizeof(r.date), "%s", lastResponse.argv[4]);
    return r;
}

std::optional<uint32_t> FSC_BT1026::
getBaudrate() {
    return getNumber<uint32_t>("AT+BAUD");
}

void FSC_BT1026::
setBaudrate( uint32_t baudrate ) {
    setNumber("AT+BAUD", baudrate);
}

std::optional<I2SCFG> FSC_BT1026::
getI2SCFG() {
    return getNumber<uint8_t>("AT+I2SCFG");
}

void FSC_BT1026::
setI2SCFG( I2SCFG cfg ) {
    setNumber("AT+I2SCFG", cfg.asUint32_t());
}

std::optional<bool> FSC_BT1026::
getSPDIFCFG() {
    return getBool("AT+SPDIFCFG");
}

void FSC_BT1026::
setSPDIFCFG( bool enable ) {
    setNumber("AT+SPDIFCFG", enable);
}