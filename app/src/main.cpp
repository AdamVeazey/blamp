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
    FSC::BT1026 bt( DEVICE_DT_GET(DT_ALIAS(modem)) );
    bt.init();
    // bt.reboot();
    if( !bt.getName().value().equals("Blamp") ) {
        bt.setName("Blamp", false);
        LOG_WRN("Set name!");
    }
    bt.setSPDIFCFG( true );
    LOG_INF("S/PDIF: %i", bt.getSPDIFCFG().value_or(0));
    auto r = bt.getI2SCFG().value();
    LOG_INF("I2S Config: %s, %s, %skHz, %s justified, %s bit delay, bitdepth: %s",
        r.getEnabled() ? "Enabled" : "Disabled",
        r.getSlave() ? "Slave" : "Master",
        r.getFrequency() == FSC::I2SCFG::Frequency::FS44100 ? "44.1" : "48",
        r.getRight() ? "Right" : "Left",
        r.getNoDelay() ? "0" : "1",
        r.getBitDepth() == FSC::I2SCFG::BitDepth::B16 ? "16" :
            r.getBitDepth() == FSC::I2SCFG::BitDepth::B24 ? "24" : "32"
    );
    auto desiredProfile = FSC::Profile()
        .setSPP(true) // Serial Port Profile
        .setA2DPSink(true) // actual BT audio playback profile
        .setAVRCPController(true); // needed for media controls
    auto profile = bt.getProfile().value();
    if( profile != desiredProfile ) {
        bt.setProfile(desiredProfile);
        LOG_WRN("Set profile!");
    }
    LOG_INF("Profile.SPP: %u", profile.getSPP());
    LOG_INF("Profile.GATTServer: %u", profile.getGATTServer());
    LOG_INF("Profile.GATTClient: %u", profile.getGATTClient());
    LOG_INF("Profile.HFP_HF: %u", profile.getHFP_HF());
    LOG_INF("Profile.HFP_AG: %u", profile.getHFP_AG());
    LOG_INF("Profile.A2DPSink: %u", profile.getA2DPSink());
    LOG_INF("Profile.A2DPSource: %u", profile.getA2DPSource());
    LOG_INF("Profile.AVRCPController: %u", profile.getAVRCPController());
    LOG_INF("Profile.AVRCPTarget: %u", profile.getAVRCPTarget());
    LOG_INF("Profile.HIDKeyboard: %u", profile.getHIDKeyboard());
    LOG_INF("Profile.PBAPServer: %u", profile.getPBAPServer());

    auto v = bt.getVersion().value();
    LOG_INF("Version - Module: %s, Version: %s, Date: %s", v.module.asCString(), v.version.asCString(), v.date.asCString());
    LOG_INF("COD: %s", bt.getCOD().value().asCString());
    LOG_INF("Auto connection max attempts: %u", bt.getAutoConn().value());
    LOG_INF("MAC: %s", bt.getAddress().value().asCString());
    LOG_INF("Pin: %s", bt.getPin().value().asCString());
    switch(bt.getInputCFG().value()) {
    case FSC::InputCFG::BT:     LOG_INF("InputCFG: BT");        break;
    case FSC::InputCFG::LineIn: LOG_INF("InputCFG: LineIn");    break;
    case FSC::InputCFG::SPDIF:  LOG_INF("InputCFG: SPDIF");     break;
    case FSC::InputCFG::I2S:    LOG_INF("InputCFG: I2S");       break;
    }

    LOG_INF("MutePin: %u", bt.getMutePIO().value());
    auto a2dpCFG = bt.getA2DPCFG().value();
    LOG_INF("A2DP CFG.AAC: %u", a2dpCFG.getAAC());
    LOG_INF("A2DP CFG.APTX: %u", a2dpCFG.getAPTX());
    LOG_INF("A2DP CFG.APTX-LL: %u", a2dpCFG.getAPTX_LL());
    LOG_INF("A2DP CFG.APTX-HD: %u", a2dpCFG.getAPTX_HD());
    LOG_INF("A2DP CFG.APTX-AD: %u", a2dpCFG.getAPTX_AD());
    LOG_INF("A2DP CFG.LDAC: %u", a2dpCFG.getLDAC());
    auto a2dpDecoder = bt.getA2DPDecoder();
    if( a2dpDecoder.has_value() ) {
        switch(a2dpDecoder.value()) {
        case FSC::A2DPDecoder::SBC:     LOG_INF("A2DP Decoder: SBC");       break;
        case FSC::A2DPDecoder::AAC:     LOG_INF("A2DP Decoder: AAC");       break;
        case FSC::A2DPDecoder::APTX:    LOG_INF("A2DP Decoder: APTX");      break;
        case FSC::A2DPDecoder::APTX_LL: LOG_INF("A2DP Decoder: APTX-LL");   break;
        case FSC::A2DPDecoder::APTX_HD: LOG_INF("A2DP Decoder: APTX-HD");   break;
        case FSC::A2DPDecoder::APTX_AD: LOG_INF("A2DP Decoder: APTX-AD");   break;
        case FSC::A2DPDecoder::LDAC:    LOG_INF("A2DP Decoder: LDAC");      break;
        }
    }
    auto a2dpEncoder = bt.getA2DPEncoder();
    if( a2dpEncoder.has_value() ) {
        switch(a2dpEncoder.value()) {
        case FSC::A2DPEncoder::SBC:     LOG_INF("A2DP Encoder: SBC");       break;
        case FSC::A2DPEncoder::AAC:     LOG_INF("A2DP Encoder: AAC");       break;
        case FSC::A2DPEncoder::APTX:    LOG_INF("A2DP Encoder: APTX");      break;
        case FSC::A2DPEncoder::APTX_LL: LOG_INF("A2DP Encoder: APTX-LL");   break;
        case FSC::A2DPEncoder::APTX_HD: LOG_INF("A2DP Encoder: APTX-HD");   break;
        case FSC::A2DPEncoder::APTX_AD: LOG_INF("A2DP Encoder: APTX-AD");   break;
        case FSC::A2DPEncoder::LDAC:    LOG_INF("A2DP Encoder: LDAC");      break;
        }
    }
    bt.setAVRCPCFG(FSC::AVRCPCFG().setTrackInfoOnChange(true).setPlayProgress(1));
    auto avrcpCFG = bt.getAVRCPCFG().value();
    LOG_INF("AVRCP CFG.trackInfoOnChange: %u", avrcpCFG.getTrackInfoOnChange());
    LOG_INF("AVRCP CFG.playProgressInterval: %u", avrcpCFG.getPlayProgress());

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