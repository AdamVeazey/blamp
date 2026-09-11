#pragma once

#include "MessageQueue.hpp"
#include "InputSource.hpp"

enum class MainSysQEnum {
    BlampKnob_Position,  // int32_t previous, int32_t current
    BlampKnob_Press,     // enum Type : uint8_t { Short, Long }

    Audio_Playback,     // enum Action : uint8_t { Play, Pause, Mute, Unmute }
    Audio_ChangeSource, // enum Source : uint8_t { Aux, RCA, TOSLINK, HDMI_ARC, BT, USB }
    Audio_Volume,       // uint8_t volume_percent; (0-100)
    Audio_TAS3251Err,   // enum Source : uint8_t { nFAULT, nCLIP_OTW }
    Audio_PCM9211Err,   // enum Source : uint8_t { INT0, INT1 }
};

using MainSysQ = MessageQueue<MainSysQEnum, 16, 200>;

// ----------------------------------------------------------------------------
//               BlampKnob messages to Main System Thread
// ----------------------------------------------------------------------------

// Sent from encoder callback
class MainSysQ_BlampKnob_Position : public MainSysQ {
private:
    int32_t previous;
    int32_t current;
public:
    MainSysQ_BlampKnob_Position( int32_t previous, int32_t current ) :
        MainSysQ( Enum::BlampKnob_Position ),
        previous(previous),
        current(current)
    { verifyMessageSize<MainSysQ_BlampKnob_Position>(); }
    int32_t getPrevious() const { return previous; }
    int32_t getCurrent() const { return current; }
    int32_t getDelta() const { return current - previous; }
};

// Sent from Input driver callback
class MainSysQ_BlampKnob_Press : public MainSysQ {
public:
    enum class Type : uint8_t { Short, Long };
private:
    Type type;
public:
    MainSysQ_BlampKnob_Press( Type type ) :
        MainSysQ( Enum::BlampKnob_Press ),
        type(type)
    { verifyMessageSize<MainSysQ_BlampKnob_Press>(); }
    Type getType() const { return type; }
};

// ----------------------------------------------------------------------------
//                  Audio messages to Main System Thread
// ----------------------------------------------------------------------------

// Sent from BT Comms, CLI, HDMI ARC, and USB threads
class MainSysQ_Audio_Playback : public MainSysQ {
public:
    enum class Action : uint8_t { Play, Pause, Mute, Unmute };
private:
    Action action;
public:
    MainSysQ_Audio_Playback( Action action ) :
        MainSysQ( Enum::Audio_Playback ),
        action(action)
    { verifyMessageSize<MainSysQ_Audio_Playback>(); }
    Action getAction() const { return action; }
};

// Sent from CLI
class MainSysQ_Audio_ChangeSource : public MainSysQ {
private:
    InputSource source;
public:
    MainSysQ_Audio_ChangeSource( InputSource source ) :
        MainSysQ( Enum::Audio_ChangeSource ),
        source(source)
    { verifyMessageSize<MainSysQ_Audio_ChangeSource>(); }
    InputSource getInputSource() const { return source; }
};

// Sent from BT Comms, CLI, HDMI ARC, and USB threads
class MainSysQ_Audio_Volume : public MainSysQ {
private:
    uint8_t volume_percent; // 0 - 100
public:
    MainSysQ_Audio_Volume( uint8_t volume_percent ) :
        MainSysQ( Enum::Audio_Volume ),
        volume_percent(volume_percent)
    { verifyMessageSize<MainSysQ_Audio_Volume>(); }
    uint8_t getVolumePercent() const { return volume_percent; }
};


// Sent from GPIO interrupt service routine
class MainSysQ_Audio_TAS3251Err : public MainSysQ {
public:
    enum class Source : uint8_t { nFAULT, nCLIP_OTW, };
private:
    Source source;
public:
    MainSysQ_Audio_TAS3251Err( Source source ) :
        MainSysQ( Enum::Audio_TAS3251Err ),
        source(source)
    { verifyMessageSize<MainSysQ_Audio_TAS3251Err>(); }
    Source getSource() const { return source; }
};

// Sent from GPIO interrupt service routine
class MainSysQ_Audio_PCM9211Err : public MainSysQ {
public:
    enum class Source : uint8_t { INT0, INT1, };
private:
    Source source;
public:
    MainSysQ_Audio_PCM9211Err( Source source ) :
        MainSysQ( Enum::Audio_PCM9211Err ),
        source(source)
    { verifyMessageSize<MainSysQ_Audio_PCM9211Err>(); }
    Source getSource() const { return source; }
};