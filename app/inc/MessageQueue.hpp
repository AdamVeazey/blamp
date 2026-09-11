#pragma once

#include <zephyr/kernel.h>

/*
 * ARCHITECTURE NOTE: Intentional Stack Over-read
 *
 * To maintain clean, decentralized C++ syntax without relying on massive global
 * unions, this queue implementation intentionally allows Zephyr's underlying
 * memcpy() to read past the memory footprint of smaller derived classes up
 * to MAX_MSG_SIZE.
 *
 * RISKS & MITIGATIONS:
 * 1. Size Overflow: A derived class MUST NOT exceed MAX_MSG_SIZE, or Zephyr
 *    will slice the payload.
 *    (Mitigation: Add `static_assert(sizeof(*this) <= MAX_MSG_SIZE);` to your
 *    derived class constructors).
 *
 * 2. MPU HardFaults: Because memcpy() reads past the object, it copies adjacent
 *    stack garbage. If this read were to cross the thread's hardware stack boundary,
 *    it would trigger a MemManage/HardFault.
 *    (Mitigation: ARM stacks grow downwards. Because these temporary objects are
 *    instantiated deep inside active function frames—with caller data and return
 *    addresses sitting "above" them in memory—this upward over-read will merely
 *    copy harmless stack data and will practically never hit the MPU guard page).
 */

template<typename E, size_t MAX_MSG_SIZE, size_t MAX_NUM_MSGS>
class MessageQueue {
public:
    using Enum = E;
    using Message = uint8_t[MAX_MSG_SIZE];
    enum class ErrSend {
        Sent = 0,           // Message sent.
        QFull = -ENOMSG,    // -ENOMSG Returned without waiting or queue purged.
        Timeout = -EAGAIN,  // -EAGAIN Waiting period timed out.
        Uninitialized,      // Need to call init()
    };
    enum class ErrReceive {
        Received = 0,           // Message received
        ReceivedWrongMessage,   // Unexpected message received
        QEmpty = -ENOMSG,       // -ENOMSG Returned without waiting or queue purged.
        Timeout = -EAGAIN,      // -EAGAIN Waiting period timed out.
        Uninitialized,          // Need to call init()
    };
protected:
    template<typename Derived>
    constexpr void verifyMessageSize() {
        static_assert( sizeof(Derived) <= MAX_MSG_SIZE,
            "Derived class size is too large! "
            "Make Derived smaller OR increase MAX_MSG_SIZE"
        );
    }
private:
    inline static struct k_msgq queue;
    inline static char __aligned(4) buffer[MAX_MSG_SIZE * MAX_NUM_MSGS];
    inline static bool initialized = false;
    E id;
public:
    inline static void init() {
        if(!initialized) {
            k_msgq_init(&queue, buffer, MAX_MSG_SIZE, MAX_NUM_MSGS);
            initialized = true;
        }
    }
    MessageQueue( E id ) : id(id) {}
    inline const E getId() const { return id; }
    inline ErrSend send( k_timeout_t timeout = K_NO_WAIT ) {
        if( !initialized ) return ErrSend::Uninitialized;
        return static_cast<ErrSend>(k_msgq_put( &queue, this, timeout ));
    }
    inline static ErrReceive receive( Message& messageOut, k_timeout_t timeout = K_FOREVER ) {
        if( !initialized ) return ErrReceive::Uninitialized;
        return static_cast<ErrReceive>(k_msgq_get( &queue, &messageOut, timeout ));
    }
    inline static bool isEmpty() {
        return k_msgq_num_used_get( &queue ) == 0;
    }
};

/*
You can then make an Enum like

enum class MainSysEnum : uint8_t {
    Play,
    Pause,
    Mute
    Volume, // int8_t adjust
};

using MainSysQ = MessageQueue<MainSysEnum, 2, 10>;

class MainSysQ_Volume : public MainSysQ {
private:
    int8_t adjust;
public:
    MainSysQ_Volume(int8_t adjust) :
        MainSysQ(MainSys_play),
        adjust(adjust)
    { verifyMessageSize<MainSysQ_Volume>(); };
    inline const getAdjust() const { return adjust; }
};

and you can use it simply like

... in a function
MainSysQ_Volume(-1).send();

the receiving side looks like

void handleVolume(const MainSysQ_Volume& volume) {
    board.volume += volume.getAdjust();
}

// ... somewhere else

using Q = MainSysQ;
Q::Message m;
if( Q::receive(m) == Q::ErrReceive::Received ) {
    switch( ((const Q&)m.data()).getId() ) {
    case Q::Enum::Volume: handleVolume( (const MainSysQ_Volume&)m ); break;
    }
}

*/