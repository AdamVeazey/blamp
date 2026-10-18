#pragma once

#include "MessageQueue.hpp"

// Bluetooth Queue message Enumerations
enum class BTQEnum {
    SetPlayback,    // bool play
    SetVolume,      // uint8_t percent (0-100)
};

// Message Queue consumed by ThreadBT
using BTQ = MessageQueue<BTQEnum, 16, 8>;

class BTQ_SetPlayback : public BTQ {
private:
    bool play;
public:
    BTQ_SetPlayback( bool play ) :
        BTQ( Enum::SetPlayback ),
        play(play)
    { verifyMessageSize<BTQ_SetPlayback>(); }
    bool shouldPlay() const { return play; }
};

class BTQ_SetVolume : public BTQ {
private:
    uint8_t percent;
public:
    BTQ_SetVolume( uint8_t percent ) :
        BTQ( Enum::SetPlayback ),
        percent(percent)
    { verifyMessageSize<BTQ_SetVolume>(); }
    uint8_t getPercent() const { return percent; }
};