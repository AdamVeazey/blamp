#pragma once

#include "MessageQueue.hpp"
#include "InputSource.hpp"

// Bluetooth Queue message Enumerations
enum class BTQEnum {
    SetPlayback,    // bool play
    SetVolume,      // uint8_t percent (0-100)
};

// Message Queue consumed by ThreadBT
using BTQ = MessageQueue<BTQEnum, 16, 8>;

class BTQ_SetPlayback : public BTQ {
private:
    PlaybackAction action;
public:
    BTQ_SetPlayback( PlaybackAction action ) :
        BTQ( Enum::SetPlayback ),
        action(action)
    { verifyMessageSize<BTQ_SetPlayback>(); }
    PlaybackAction getAction() const { return action; }
};

class BTQ_SetVolume : public BTQ {
private:
    uint8_t percent;
public:
    BTQ_SetVolume( uint8_t percent ) :
        BTQ( Enum::SetVolume ),
        percent(percent)
    { verifyMessageSize<BTQ_SetVolume>(); }
    uint8_t getPercent() const { return percent; }
};