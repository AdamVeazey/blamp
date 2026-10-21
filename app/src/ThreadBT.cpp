#include "ThreadBT.hpp"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "BTQ.hpp"
#include "MainSysQ.hpp"
#include "FSC_BT1026.hpp"

LOG_MODULE_REGISTER(ThreadBT_cpp, LOG_LEVEL_INF);

// 'local' global BT device, should save a lot of thread stack space
static FSC::BT1026 bt( DEVICE_DT_GET(DT_ALIAS(modem)), FSC::Events{
    // happen on kworker thread, don't do any blocking stuff!!
    .onStateDevice = [](FSC::DEVSTATE state) {
        // only send this event if the connected state changed
        MainSysQ_BT_Connected( !state.isBR_EDR_Discoverable() ).send();
    },
    .onStatePlay = [](FSC::PlayState state) {
        switch(state) {
        case FSC::PlayState::Stopped:
        case FSC::PlayState::Paused:
            MainSysQ_BT_IsPlaying( false ).send();
            break;
        case FSC::PlayState::Playing:
            MainSysQ_BT_IsPlaying( true ).send();
            break;
        case FSC::PlayState::FastForward:   break;
        case FSC::PlayState::FastRewinding: break;
        }
    },
    .onStateTrack = [](FSC::TrackState state) {
        const char* ps = "Unknown";
        switch(state.playState){
        case FSC::PlayState::Stopped:       ps = "Stopped";         break;
        case FSC::PlayState::Playing:       ps = "Playing";         break;
        case FSC::PlayState::Paused:        ps = "Paused";          break;
        case FSC::PlayState::FastForward:   ps = "Fast Forward";    break;
        case FSC::PlayState::FastRewinding: ps = "Fast Rewinding";  break;
        }
        LOG_INF("PlayState: %s elapsed %d ms / total %d ms", ps, state.elapsedTime_ms, state.totalTime_ms);
    },
    .onTrackInfo = [](const FSC::TrackInfo& info) {
        if( info.artist.isEmpty() && info.album.isEmpty() ) {
            LOG_INF("%s", info.title.asCString());
        }
    }
});
static const gpio_dt_spec BT_RESET = GPIO_DT_SPEC_GET(DT_ALIAS(bt_reset), gpios);
static const gpio_dt_spec BT_SYS_CTRL = GPIO_DT_SPEC_GET(DT_ALIAS(bt_sys_ctrl), gpios);

static void handleSetPlayback( const BTQ_SetPlayback& playback ) {
    auto action = playback.getAction();
    if( action == PlaybackAction::Play ) {
        bool didPlay = bt.play();
        LOG_INF("handleSetPlayback() bt.play(): %u", didPlay);
    }
    else if( action == PlaybackAction::Pause ) {
        bool didPause = bt.pause();
        LOG_INF("handleSetPlayback() bt.pause(): %u", didPause);
    }
    else if( action == PlaybackAction::Forward ) {
        bool didForward = bt.forward();
        LOG_INF("handleSetPlayback() bt.forward(): %u", didForward);
    }
    else if( action == PlaybackAction::Backward ) {
        bool didBackward = bt.backward();
        LOG_INF("handleSetPlayback() bt.backward(): %u", didBackward);
    }
}

static void handleSetVolume( const BTQ_SetVolume& volume ) {
    // TODO: translate this to a real volume I guess??
    bt.setSpkVol( volume.getPercent() );
    LOG_INF("handleSetVolume() percent: %u", volume.getPercent());
}

static void processEvents( const BTQ& message ) {
    using E = BTQ::Enum;
    switch( message.getId() ) {
    case E::SetPlayback:    handleSetPlayback( (const BTQ_SetPlayback&)message );   break;
    case E::SetVolume:      handleSetVolume( (const BTQ_SetVolume&)message );       break;
    }
}

static void resetPowerUp() {
    LOG_INF("Powering cycling / resetting BT module...");
    // Ensure clean baseline (BT_SYS_CTRL low, BT_RESET asserted low)
    gpio_pin_set_dt(&BT_SYS_CTRL, false);
    gpio_pin_set_dt(&BT_RESET, true);
    k_msleep(50); // Let power rails drop/stabilize

    gpio_pin_set_dt(&BT_SYS_CTRL, true); // power-up of internal regulators

    k_msleep(30); // Wait >20 ms for SYS_CTRL power-up

    gpio_pin_set_dt(&BT_RESET, false); // Let software run on the module
}

void threadBT(void* arg1, void* arg2, void* arg3) {
    BTQ::init();

    if (!gpio_is_ready_dt(&BT_RESET) || !gpio_is_ready_dt(&BT_SYS_CTRL)) {
        return;
    }

    gpio_pin_configure_dt(&BT_RESET, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&BT_SYS_CTRL, GPIO_OUTPUT_INACTIVE);

    resetPowerUp();

    // Give the module time to boot its firmware before talking over UART
    k_msleep(100);

    int ret = bt.init();
    if( ret ) {
        LOG_ERR("Init failed! ret: %d", ret);
        while(true) {}
    }
    // bt.reboot();

    auto v = bt.getVersion().value_or(FSC::Version{});
    LOG_INF("Version - Module: %s, Version: %s, Date: %s", v.module.asCString(), v.version.asCString(), v.date.asCString());

    if( !bt.setName("Blamp", false) ) {
        LOG_ERR("Failed to set name to Blamp!");
    }

    if( !bt.setSPDIFCFG( true ) ) {
        LOG_ERR("Failed to enable S/PDIF output!");
    }

    auto desiredProfile = FSC::Profile()
        .setSPP(true) // Serial Port Profile
        .setA2DPSink(true) // actual BT audio playback profile
        .setAVRCPController(true); // needed for media controls
    if( !bt.setProfile( desiredProfile ) ) {
        LOG_ERR("Failed to set desired profile!");
    }

    auto desiredAVRCPConfig = FSC::AVRCPCFG()
        .setTrackInfoOnChange(true)
        .setPlayProgress(1);
    if( !bt.setAVRCPCFG( desiredAVRCPConfig ) ) {
        LOG_ERR("Failed to set desired AVRCP Config!");
    }
    if( !bt.getDevState() ) {
        LOG_WRN("Failed to get initial device state");
    }
    else {
        LOG_INF("Device state is now known");
    }

    while( true ) {
        BTQ::Message m;
        while (BTQ::receive( m, K_FOREVER ) == BTQ::ErrReceive::Received ) {
            processEvents( reinterpret_cast<const BTQ&>(*m) );
        }
    }
}