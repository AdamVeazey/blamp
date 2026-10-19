#include "ThreadBT.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "BTQ.hpp"
#include "MainSysQ.hpp"
#include "FSC_BT1026.hpp"

LOG_MODULE_REGISTER(ThreadBT_cpp, LOG_LEVEL_INF);

static FSC::BT1026* bt = nullptr;

static void handleSetPlayback( const BTQ_SetPlayback& playback ) {
    if( playback.shouldPlay() ) {
        bool didPlay = bt->play();
        LOG_INF("handleSetPlayback() did play: %u", didPlay);
    }
    else {
        bool didPause = bt->pause();
        LOG_INF("handleSetPlayback() did pause: %u", didPause);
    }
}

static void handleSetVolume( const BTQ_SetVolume& volume ) {
    // TODO: translate this to a real volume I guess??
    bt->setSpkVol( volume.getPercent() );
    LOG_INF("handleSetVolume() percent: %u", volume.getPercent());
}

static void processEvents( const BTQ& message ) {
    using E = BTQ::Enum;
    switch( message.getId() ) {
    case E::SetPlayback:    handleSetPlayback( (const BTQ_SetPlayback&)message );   break;
    case E::SetVolume:      handleSetVolume( (const BTQ_SetVolume&)message );       break;
    }
}

void threadBT(void* btPointer, void* arg2, void* arg3) {
    BTQ::init();
    bt = static_cast<FSC::BT1026*>(btPointer);
    EDF_ASSERTD(btPointer != nullptr, "This thread requires a pointer to an FSC::BT1026 object");

    int ret = bt->init();
    if( ret ) {
        LOG_ERR("Init failed! ret: %d", ret);
        while(true) {}
    }
    // bt->reboot();

    auto v = bt->getVersion().value_or(FSC::Version{});
    LOG_INF("Version - Module: %s, Version: %s, Date: %s", v.module.asCString(), v.version.asCString(), v.date.asCString());

    if( !bt->setName("Blamp", false) ) {
        LOG_ERR("Failed to set name to Blamp!");
    }

    if( !bt->setSPDIFCFG( true ) ) {
        LOG_ERR("Failed to enable S/PDIF output!");
    }

    auto desiredProfile = FSC::Profile()
        .setSPP(true) // Serial Port Profile
        .setA2DPSink(true) // actual BT audio playback profile
        .setAVRCPController(true); // needed for media controls
    if( !bt->setProfile( desiredProfile ) ) {
        LOG_ERR("Failed to set desired profile!");
    }

    auto desiredAVRCPConfig = FSC::AVRCPCFG()
        .setTrackInfoOnChange(true)
        .setPlayProgress(1);
    if( !bt->setAVRCPCFG( desiredAVRCPConfig ) ) {
        LOG_ERR("Failed to set desired AVRCP Config!");
    }
    if( !bt->getDevState() ) {
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