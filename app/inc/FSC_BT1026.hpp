#pragma once

#include <zephyr/modem/chat.h>
#include <zephyr/modem/backend/uart.h>
#include <optional>
#include <string_view>
#include <charconv>
#include <cstring>


struct ResponseVersion {
    char module[32];
    char version[32];
    char date[32];
};

struct I2SCFG{
    bool enabled;
    bool slave;
    bool fs44100;
    bool right;
    bool noDelay;
    uint8_t bitDepth;
    I2SCFG( uint8_t bitfield ) {
        enabled = (bitfield >> 0) & 1;
        slave = (bitfield >> 1) & 1;
        fs44100 = (bitfield >> 2) & 1; // 48k otherwise
        right = (bitfield >> 3) & 1;  // left otherwise
        noDelay = (bitfield >> 4) & 1; // 1 bit delay otherwise
        bitDepth = 0; // invalid by default;
        switch( (bitfield >> 5) & 3 ){
        case 0: bitDepth = 16; break;
        case 1: bitDepth = 24; break;
        case 2: bitDepth = 32; break;
        default:
            break;
        }
    }
    uint32_t asUint32_t() const {
        return (enabled << 0) |
               (slave << 1) |
               (fs44100 << 2) |
               (right << 3) |
               (noDelay << 4) |
               (bitDepth << 5);
    }
};

class FSC_BT1026 {
private:
    const device* const uart;
    modem_backend_uart backend;
    modem_chat chat;
    uint8_t uartRx[256];
    uint8_t uartTx[256];
    uint8_t chatRx[256];
    uint8_t* chatArgv[32];

    // Command memory
    modem_chat_script_chat dynCmdChat;
    modem_chat_script dynCmdScript;
    char cmdBuffer[64]; // Holds the formatted string (e.g., "AT+SPKVOL=10")
    struct {
        char argv[6][32]; // Support up to 6 arguments, max 32 chars each
        uint16_t argc = 0;
    } lastResponse;

    // Concurrency controls
    k_mutex lock;
    k_sem syncSem;
    modem_chat_script_result lastResult;
public:
    void callbackMatchOK( modem_chat* chat, char** argv, uint16_t argc );
    void callbackMatchERROR( modem_chat* chat, char** argv, uint16_t argc );
    void callbackMatchUnsolicited( modem_chat* chat, char** argv, uint16_t argc );
private:
    bool runSingleCommand( const char* cmd, uint32_t timeout_ms = 3000 );
    void setNumber( const char* cmd, uint32_t val );

    template<typename T>
    std::optional<T> getNumber( const char* cmd );
    std::optional<bool> getBool( const char* cmd );
public:
    FSC_BT1026( const device* const uart );
    int init();

    std::optional<ResponseVersion> getVersion();

    std::optional<uint32_t> getBaudrate();
    void setBaudrate( uint32_t baudrate );

    std::optional<I2SCFG> getI2SCFG();
    void setI2SCFG( I2SCFG cfg );

    std::optional<bool> getSPDIFCFG();
    void setSPDIFCFG( bool enable );

    std::optional<uint32_t> getProfile() { return getNumber<uint32_t>("AT+PROFILE"); }
};

template<typename T>
std::optional<T> FSC_BT1026::
getNumber( const char* cmd ) {
    if (!runSingleCommand(cmd) || lastResponse.argc < 3) {
        return std::nullopt;
    }
    T val{};
    auto [ptr, ec] = std::from_chars(
        lastResponse.argv[2],
        lastResponse.argv[2] + std::strlen(lastResponse.argv[2]),
        val
    );
    return (ec == std::errc()) ? std::optional(val) : std::nullopt;
}