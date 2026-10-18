#include "ThreadBT.hpp"

#include <zephyr/kernel.h>
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
    }
});

static void handleSetPlayback( const BTQ_SetPlayback& playback ) {
    if( playback.shouldPlay() ) {
        bool didPlay = bt.play();
        LOG_INF("handleSetPlayback() did play: %u", didPlay);
    }
    else {
        bool didPause = bt.pause();
        LOG_INF("handleSetPlayback() did pause: %u", didPause);
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

void threadBT(void* arg1, void* arg2, void* arg3) {
    BTQ::init();

    bt.init();
    // bt.reboot();
    if( !bt.getName().value_or("").equals("Blamp") ) {
        bt.setName("Blamp", false);
        LOG_WRN("Set name!");
    }
    bt.setSPDIFCFG( true );
    LOG_INF("S/PDIF: %i", bt.getSPDIFCFG().value_or(0));
    auto desiredProfile = FSC::Profile()
        .setSPP(true) // Serial Port Profile
        .setA2DPSink(true) // actual BT audio playback profile
        .setAVRCPController(true); // needed for media controls
    auto profile = bt.getProfile().value_or(desiredProfile);
    if( profile != desiredProfile ) {
        bt.setProfile(desiredProfile);
        LOG_WRN("Set profile!");
    }

    auto v = bt.getVersion().value_or(FSC::Version{});
    LOG_INF("Version - Module: %s, Version: %s, Date: %s", v.module.asCString(), v.version.asCString(), v.date.asCString());

    auto desiredAVRCPConfig = FSC::AVRCPCFG()
        .setTrackInfoOnChange(true)
        .setPlayProgress(1);
    auto avrcpCFG = bt.getAVRCPCFG();
    if( avrcpCFG.has_value() ) {
        if( desiredAVRCPConfig != *avrcpCFG )
            bt.setAVRCPCFG( desiredAVRCPConfig );
        LOG_INF("AVRCP CFG.trackInfoOnChange: %u", (*avrcpCFG).getTrackInfoOnChange());
        LOG_INF("AVRCP CFG.playProgressInterval: %u", (*avrcpCFG).getPlayProgress());
    }
    bt.getDevState();

    while( true ) {
        BTQ::Message m;
        while (BTQ::receive( m, K_FOREVER ) == BTQ::ErrReceive::Received ) {
            processEvents( reinterpret_cast<const BTQ&>(*m) );
        }
    }
}