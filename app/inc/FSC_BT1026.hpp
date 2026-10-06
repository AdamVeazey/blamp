#pragma once

#include <zephyr/modem/chat.h>
#include <zephyr/modem/backend/uart.h>
#include <optional>
#include <string_view>
#include <EDF/BitField.hpp>
#include <EDF/String.hpp>
#include <EDF/Math.hpp>

namespace FSC {

struct Version {
    EDF::String<32> module;
    EDF::String<32> version;
    EDF::String<32> date;
};

class I2SCFG : public EDF::BitField8 {
    public:
    enum class Frequency {
        FS48000 = 0x00, // 48kHz
        FS44100 = 0x01, // 44.1kHz
    };
    enum class BitDepth {
        B16 = 0x00,
        B24 = 0x01,
        B32 = 0x02,
    };
    using EDF::BitField8::BitField8;

    constexpr bool getEnabled() const { return get(0, 1); }
    constexpr I2SCFG& setEnable( bool enable ) { set(0, 1, enable); return *this; }

    constexpr bool getSlave() const { return get(1, 1); }
    constexpr I2SCFG& setSlave( bool slave ) { set(1, 1, slave); return *this; }

    constexpr Frequency getFrequency() const { return static_cast<Frequency>(get(2, 1)); }
    constexpr I2SCFG& setFrequency( Frequency f ) { set(2, 1, static_cast<uint8_t>(f)); return *this; }

    constexpr bool getRight() const { return get(3, 1); }
    constexpr I2SCFG& setRight( bool enabled ) { set(3, 1, enabled); return *this; }

    constexpr bool getNoDelay() const { return get(4, 1); }
    constexpr I2SCFG& setNoDelay( bool enabled ) { set(4, 1, enabled); return *this; }

    constexpr BitDepth getBitDepth() const { return static_cast<BitDepth>(get(5, 2)); }
    constexpr I2SCFG& setBitDepth( BitDepth depth ) { set(5, 2, static_cast<uint8_t>(depth)); return *this; }
};

class Profile : public EDF::BitField16 {
public:
    using EDF::BitField16::BitField16;

    // Serial Port Profile
    constexpr bool getSPP() const { return get(0, 1); }
    constexpr Profile& setSPP( bool enable ) { set(0, 1, enable); return *this; }

    constexpr bool getGATTServer() const { return get(1, 1); }
    constexpr Profile& setGATTServer( bool enable ) { set(1, 1, enable); return *this; }

    constexpr bool getGATTClient() const { return get(2, 1); }
    constexpr Profile& setGATTClient( bool enable ) { set(2, 1, enable); return *this; }

    constexpr bool getHFP_HF() const { return get(3, 1); }
    constexpr Profile& setHFP_HF( bool enable ) { set(3, 1, enable); return *this; }

    constexpr bool getHFP_AG() const { return get(4, 1); }
    constexpr Profile& setHFP_AG( bool enable ) { set(4, 1, enable); return *this; }

    constexpr bool getA2DPSink() const { return get(5, 1); }
    constexpr Profile& setA2DPSink( bool enable ) { set(5, 1, enable); return *this; }

    constexpr bool getA2DPSource() const { return get(6, 1); }
    constexpr Profile& setA2DPSource( bool enable ) { set(6, 1, enable); return *this; }

    constexpr bool getAVRCPController() const { return get(7, 1); }
    constexpr Profile& setAVRCPController( bool enable ) { set(7, 1, enable); return *this; }

    constexpr bool getAVRCPTarget() const { return get(8, 1); }
    constexpr Profile& setAVRCPTarget( bool enable ) { set(8, 1, enable); return *this; }

    constexpr bool getHIDKeyboard() const { return get(9, 1); }
    constexpr Profile& setHIDKeyboard( bool enable ) { set(9, 1, enable); return *this; }

    constexpr bool getPBAPServer() const { return get(10, 1); }
    constexpr Profile& setPBAPServer( bool enable ) { set(10, 1, enable); return *this; }
};

class DEVSTATE : public EDF::BitField8 {
public:
    using EDF::BitField8::BitField8;

    constexpr bool isPowerOn() const { return get(0, 1); }
    constexpr bool isBR_EDR_Discoverable() const { return get(1, 1); }
    constexpr bool isBLEAdvertising() const { return get(2, 1); }
    constexpr bool isBR_EDR_Scanning() const { return get(3, 1); }
    constexpr bool isBLEScanning() const { return get(4, 1); }
};

enum class SPPSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
};

enum class GATTSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
};

enum class HFPSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
    OutgoingCall    = 0x04,
    IncomingCall    = 0x05,
    ActiveCall      = 0x06,
};

enum class A2DPSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
    Streaming       = 0x04,
};

enum class AVRCPSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
};

enum class HIDSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
};

enum class PBSTATE {
    Unsupported     = 0x00,
    Standby         = 0x01,
    Connecting      = 0x02,
    Connected       = 0x03,
    Downloading     = 0x04,
};

struct STATES {
    DEVSTATE dev;
    SPPSTATE spp;
    GATTSTATE gatt;
    HFPSTATE hfp;
    A2DPSTATE a2dp;
    AVRCPSTATE avrcp;
    HIDSTATE hid;
    PBSTATE pb;
};

enum class SecureSimplePairing {
    LegacyPinCode   = 0x00,
    Auto            = 0x01,
    Display         = 0x02,
};

enum class InputCFG {
    BT      = 0x00,
    LineIn  = 0x01,
    SPDIF   = 0x02,
    I2S     = 0x03,
};

enum class HFPSCO {
    Default     = 0x00, // let system figure it out
    AlwaysToHF  = 0x01, // force audio to HF device
    AlwaysToAG  = 0x02, // force audio to say on audio gateway (phone)
};

class A2DPCFG : public EDF::BitField8 {
public:
    using EDF::BitField8::BitField8;

    constexpr bool getAAC() const { return get(0, 1); }
    constexpr A2DPCFG& setAAC( bool enable ) { set(0, 1, enable); return *this; }

    constexpr bool getAPTX() const { return get(1, 1); }
    constexpr A2DPCFG& setAPTX( bool enable ) { set(1, 1, enable); return *this; }

    constexpr bool getAPTX_LL() const { return get(2, 1); }
    constexpr A2DPCFG& setAPTX_LL( bool enable ) { set(2, 1, enable); return *this; }

    constexpr bool getAPTX_HD() const { return get(3, 1); }
    constexpr A2DPCFG& setAPTX_HD( bool enable ) { set(3, 1, enable); return *this; }

    constexpr bool getAPTX_AD() const { return get(4, 1); }
    constexpr A2DPCFG& setAPTX_AD( bool enable ) { set(4, 1, enable); return *this; }

    constexpr bool getLDAC() const { return get(5, 1); }
    constexpr A2DPCFG& setLDAC( bool enable ) { set(5, 1, enable); return *this; }
};

enum class A2DPDecoder {
    SBC     = 1,
    AAC     = 3,
    APTX    = 5,
    APTX_HD = 7,
    APTX_LL = 8,
    APTX_AD = 9,
    LDAC    = 10,
};
using A2DPEncoder = A2DPDecoder;

class AVRCPCFG : public EDF::BitField8 {
public:
    using EDF::BitField8::BitField8;

    // get track info on track changed
    constexpr bool getTrackInfoOnChange() const { return get(0, 1); }
    constexpr AVRCPCFG& setTrackInfoOnChange( bool enable ) { set(0, 1, enable); return *this; }

    // Get track play progress if value > 0.
    constexpr uint8_t getPlayProgress() const { return get(1, 3); }
    constexpr AVRCPCFG& setPlayProgress(uint8_t v) { set(1, 3, v); return *this; }
};

enum class PBDown {
    PhonebookSIM    = 0, // SIM storage
    PhonebookPhone  = 1, // Phone storage
    ReceivedCallLog = 2,
    DialedCallLog   = 3,
    MissedCallLog   = 4,
    AllCallLog      = 5,
};

enum class HIDMode {
    HexKeyCode      = 0,
    ASCIIKeyCode    = 1, // English
};

enum class HIDCommand : uint16_t {
    PlayPause               = 0x00CD,
    Stop                    = 0x00B7,
    Forward                 = 0x00B5,
    Backward                = 0x00B6,
    FastForward             = 0x00B3,
    Rewind                  = 0x00B4,
    Record                  = 0x00B2,
    VolumeUp                = 0x00E9,
    VolumeDown              = 0x00EA,
    Mute                    = 0x00E2,
    OnScreenKeyboardToggle  = 0x01AE,
};

// Command memory
struct Response {
    EDF::String<32> expected;
    EDF::String<32> argv[10];
    uint16_t argc;
    Response() : argc(0) {}
};

class BT1026 {
private:
    const device* const uart;
    modem_backend_uart backend;
    modem_chat chat;
    uint8_t uartRx[256];
    uint8_t uartTx[256];
    uint8_t chatRx[256];
    uint8_t* chatArgv[32];

    Response response;

    // Concurrency controls
    k_mutex lock;
    k_sem syncSem;
    modem_chat_script_result lastResult;
public:
    void callbackMatchOK( modem_chat* chat, char** argv, uint16_t argc );
    void callbackMatchERROR( modem_chat* chat, char** argv, uint16_t argc );
    void callbackMatchUnsolicited( modem_chat* chat, char** argv, uint16_t argc );
private:
    bool runSingleCommand( const char* cmd, const char* expected = nullptr );
public:
    BT1026( const device* const uart );
    int init();

    std::optional<Version> getVersion();

    std::optional<uint32_t> getBaudrate();
    void setBaudrate( uint32_t baudrate );

    std::optional<I2SCFG> getI2SCFG();
    void setI2SCFG( I2SCFG cfg );

    std::optional<bool> getSPDIFCFG();
    void setSPDIFCFG( bool enable );

    std::optional<int32_t> getMicGain();
    void setMicGain( int32_t gain );

    std::optional<int32_t> getSpkVol();
    void setSpkVol( int32_t volume );

    void reboot();
    void factoryReset();

    std::optional<bool> getBluetoothIsOn();
    void setBluetooth( bool on );

    std::optional<Profile> getProfile();
    void setProfile( Profile p );

    std::optional<uint8_t> getAutoConn();
    void setAutoConn(uint8_t maxAttempts);

    std::optional<STATES> getAllStates();
    std::optional<DEVSTATE> getDevState();
    std::optional<EDF::String<13>> getAddress();
    std::optional<EDF::String<13>> getBLEAddress();

    std::optional<EDF::String<32>> getName();
    void setName(const EDF::String<32>& name, bool enableMACSuffix = true);

    std::optional<EDF::String<32>> getBLEName();
    void setBLEName(const EDF::String<32>& name, bool enableMACSuffix = true);

    std::optional<SecureSimplePairing> getSPP();
    void setSPP(SecureSimplePairing m); // NOTE: Needs reboot

    std::optional<EDF::String<16>> getPin();
    void setPin(const EDF::String<16>& pin);

    std::optional<EDF::String<7>> getCOD(); // Have fun
    void setCOD(const EDF::String<7>& cod); // http://bluetooth-pentest.narod.ru/software/bluetooth_class_of_device-service_generator.html

    std::optional<bool> getAdvertising();
    void setAdvertising(bool start);

    void setScanning(bool start); // a bunch of responses happen

    void getPairedList(); // A bunch of respones happen
    void delPaired(uint8_t index);
    void delAllConnections();

    std::optional<bool> getThroughputMode();
    void setThroughputMode(bool enable);

    std::optional<InputCFG> getInputCFG();
    void setInputCFG(InputCFG in);

    std::optional<bool> getPrint();
    void setPrint(bool enable);

    void setTone(uint8_t tone); // 0-94?

    std::optional<uint8_t> getMutePIO();
    void setMutePIO(uint8_t pin); // 0-63

    std::optional<bool> setMicMute(bool enable);
    std::optional<bool> setSpkMute(bool enable);

    std::optional<SPPSTATE> getSPPState();
    void SPPConnect(const EDF::String<13>& mac = "");
    void SPPRelease();
    void SPPSend(const EDF::String<493>& data); // UTF-8

    std::optional<GATTSTATE> getGATTState();
    void GATTRelease();
    void GATTSend(const EDF::String<493>& data); // UTF-8

    std::optional<HFPSTATE> getHFPState();

    std::optional<uint16_t> getHFPRes();
    void setHFPRes(uint16_t hz); // 0-48,000

    void HFPConnect(const EDF::String<13>& mac = "");

    void HFPRelease();

    void HFPDial(const EDF::String<26>& phoneNumber); // +123 (123) 456-7890
    void HFPDTMF(char dtmf); // 0-9,#,* (dial-tone multi-frequency)
    void HFPCallPickup();
    void HFPCallHangup();
    void HFPTransferAudio(bool deviceToModule); // module -> device if false
    void HFPVoiceRecognition(bool start); // like siri
    void setHFPSCO(HFPSCO config);
    void setHFPBattery(uint8_t level); // 0-9

    std::optional<A2DPSTATE> getA2DPState();
    void A2DPConnect(const EDF::String<13>& mac = "");
    void A2DPRelease();

    std::optional<A2DPCFG> getA2DPCFG();
    void setA2DPCFG(A2DPCFG config);

    std::optional<A2DPDecoder> getA2DPDecoder();
    std::optional<A2DPEncoder> getA2DPEncoder();
    void setA2DPConnection(bool establish);

    std::optional<AVRCPSTATE> getAVRCPState();

    std::optional<AVRCPCFG> getAVRCPCFG();
    void setAVRCPCFG(AVRCPCFG config);

    void playPause();
    void play();
    void pause();
    void stop();
    void forward();
    void backward();
    void fastForward(bool pressDown); // false for pressUp (release button)
    void rewind(bool pressDown); // false for pressUp (release button)

    std::optional<HIDSTATE> getHIDState();
    void HIDConnect(const EDF::String<13>& mac = "");
    void HIDRelease();
    std::optional<HIDMode> getHIDMode();
    void setHIDMode(HIDMode mode);
    std::optional<bool> getHIDAutoRelease();
    void setHIDAutoRelease(bool enable); // false for manual
    std::optional<uint32_t> getHIDDelay();
    void setHIDDelay(uint32_t ms);
    void HIDSend(const EDF::String<33>& key);
    void HIDCmd(HIDCommand cmd);

    std::optional<PBSTATE> getPBState();
    void PBConnect(const EDF::String<13>& mac = "");
    void PBRelease();
    void PBDownload(PBDown type);
    void PBDownload(PBDown type, uint16_t maxItems);
};

}; /* FSC */