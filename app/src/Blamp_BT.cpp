#include "Blamp.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(blamp_bt, LOG_LEVEL_INF);

void Blamp::
handleBT_IsPlaying( const MainSysQ_BT_IsPlaying& bt ) {
    if( connections.audioSource != InputSource::BLUETOOTH ) {
        LOG_WRN("BT is %splaying audio, but not current input source",
            bt.isPlaying() ? "" : "not "
        );
        return;
    }
    if( audio.isPlaying == bt.isPlaying() ) return; // no actual state change
    audio.isPlaying = bt.isPlaying();
    if( state == DisplayUI::MenuState::Home )
        display.updateHomeAudioPlaying( audio.isPlaying );
    LOG_INF("Bluetooth is %splaying audio", audio.isPlaying ? "" : "not ");
}

void Blamp::
handleBT_Connected( const MainSysQ_BT_Connected& bt ) {
    if( connections.bluetoothConnected != bt.isConnected() ) {
        connections.bluetoothConnected = bt.isConnected();
        LOG_INF("Bluetooth is %sconnected!",
            connections.bluetoothConnected ? "" : "not "
        );
    }
}