#include <algorithm>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <lvgl.h>

#include "Blamp.hpp"
#include "QuadratureEncoder.hpp"
#include "FSC_BT1026.hpp"

LOG_MODULE_REGISTER(main_cpp, LOG_LEVEL_INF);


static struct k_timer qdec_timer;
static struct k_work qdec_work;

static void qdec_work_handler(struct k_work *work) {
    static const QuadratureEncoder qdec( DEVICE_DT_GET(DT_NODELABEL(qdec)) );
    if (!qdec.isReady()) {
        LOG_ERR("qdec is not ready!");
        return;
    }

    static int32_t prev = 0;
    int32_t current = qdec.getValue();
    if (current != prev) {
        MainSysQ_BlampKnob_Position(prev, current).send();
        prev = current;
    }
}

static void qdec_timer_handler(struct k_timer *timer) {
    // Offload execution to thread context (System Workqueue)
    k_work_submit(&qdec_work);
}

void qdec_init(void) {
    k_work_init(&qdec_work, qdec_work_handler);
    k_timer_init(&qdec_timer, qdec_timer_handler, NULL);

    // Poll every 10 ms
    k_timer_start(&qdec_timer, K_MSEC(10), K_MSEC(10));
}

static void input_cb(struct input_event *evt) {
    // The driver sends evt->value = 1 for press, 0 for release.
    // We only care about the moment the press action activates (value == 1).
    if( evt->value != 1 ) return;

    using T = MainSysQ_BlampKnob_Press::Type;
    if( evt->code == INPUT_KEY_ENTER ) {
        // DT defined ENTER as short press
        MainSysQ_BlampKnob_Press(T::Short).send();
    }
    else if( evt->code == INPUT_KEY_MENU ) {
        // DT defined MENU as long press
        MainSysQ_BlampKnob_Press(T::Long).send();
    }
}

// Register to listen to the input events globally
INPUT_CALLBACK_DEFINE( nullptr, input_cb );

int main(void) {
    Blamp::init();
    qdec_init();
    FSC_BT1026 bt( DEVICE_DT_GET(DT_ALIAS(modem)) );
    bt.init();

    LOG_INF("S/PDIF: %i", bt.getSPDIFCFG().value_or(0));
    bt.setSPDIFCFG( false );
    LOG_INF("S/PDIF: %i", bt.getSPDIFCFG().value_or(0));
    bt.setSPDIFCFG( true );
    LOG_INF("S/PDIF: %i", bt.getSPDIFCFG().value_or(0));
    auto r = bt.getI2SCFG().value();
    LOG_INF("I2S Config: %s, %s, %skHz, %s justified, %s bit delay, bitdepth: %u",
        r.enabled ? "Enabled" : "Disabled",
        r.slave ? "Slave" : "Master",
        r.fs44100 ? "44.1" : "48",
        r.right ? "Right" : "Left",
        r.noDelay ? "0" : "1",
        r.bitDepth
    );
    auto p = bt.getProfile().value();
    LOG_INF("Profile: %u", p);


    auto v = bt.getVersion().value();
    LOG_INF("Version - Module: %s, Version: %s, Date: %s", v.module, v.version, v.date);


    while (1) {
        // 1. Ask LVGL how long until its next task (e.g., inactivity timer, animation)
        uint32_t lvgl_delay = lv_task_handler();
        uint32_t timeout_ms = std::clamp( static_cast<int>(lvgl_delay), 1, 1000 );

        // 2. Block on the queue.
        // WAKES INSTANTLY if a message (Knob, Audio Fault, I2C event) arrives!
        // Otherwise times out after timeout_ms so LVGL can run.
        MainSysQ::Message m;
        if( MainSysQ::receive(m, K_MSEC(timeout_ms)) == MainSysQ::ErrReceive::Received ) {

            // Process the message that woke us up
            Blamp::processEvents( reinterpret_cast<const MainSysQ&>(*m) );

            // Drain any other queued messages that arrived in the same burst
            while (MainSysQ::receive( m, K_NO_WAIT ) == MainSysQ::ErrReceive::Received ) {
                Blamp::processEvents( reinterpret_cast<const MainSysQ&>(*m) );
            }
        }
    }
}