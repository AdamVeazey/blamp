#include "Blamp.hpp"

#include <algorithm>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(blamp_blampknob, LOG_LEVEL_INF);

void Blamp::
handleBlampknob_Position( const MainSysQ_BlampKnob_Position& position ) {
    LOG_INF("prev: %i current: %i", position.getPrevious(), position.getCurrent());
    display.resetInactivityTimer();
    auto delta = position.getDelta();
    switch( state ) {
    case DisplayUI::MenuState::Home:
    {
        constexpr int32_t TICKS_PER_DETENT = 4;        // 4 STM32 hardware counts per click
        constexpr int32_t VOL_PERCENT_PER_DETENT = 2; // 2% volume per physical click

        int32_t detents = delta / TICKS_PER_DETENT;
        if( detents != 0 ) {
            int32_t requestedVol = static_cast<int32_t>(audio.volume) + (detents * VOL_PERCENT_PER_DETENT);
            uint8_t targetVolume = static_cast<uint8_t>(std::clamp( requestedVol, 0, 100 ));

            if( targetVolume != audio.volume ) {
                handleAudio_Volume( targetVolume );
            }
        }
        break;
    }
    case DisplayUI::MenuState::SubmenuSelect:
    case DisplayUI::MenuState::SourceSelect:
        if( delta > 0 ) display.scrollMenuDown();
        else            display.scrollMenuUp();
        break;
    case DisplayUI::MenuState::BTPairing:
        // Do nothing
        break;
    }
}

void Blamp::
handleBlampknob_Press( const MainSysQ_BlampKnob_Press& press ) {
    bool isShort = (press.getType() == MainSysQ_BlampKnob_Press::Type::Short);
    const char* pressType = isShort ? "short" : "long";
    LOG_INF("%s press", pressType);
    display.resetInactivityTimer();
    switch(state) {
    case DisplayUI::MenuState::Home:
        if( isShort ) {
            // short press
            using A = MainSysQ_Audio_Playback::Action;
            A requestedAction = audio.isPlaying ? A::Pause : A::Play;
            handleAudio_Playback( requestedAction );
        }
        else {
            // long press
            state = DisplayUI::MenuState::SubmenuSelect;
            display.showSubmenuSelect();
        }
        break;
    case DisplayUI::MenuState::SubmenuSelect:
        if( isShort ) {
            // short press
            switch( display.getSubmenuSelected() ){
            case DisplayUI::Submenu::InputSourceSelection:
                state = DisplayUI::MenuState::SourceSelect;
                display.showSourceSelect( connections.audioSource );
                break;
            case DisplayUI::Submenu::BluetoothPairing:
                state = DisplayUI::MenuState::BTPairing;
                display.showBTPairing();
                break;
            }
        }
        else {
            // long press
            state = DisplayUI::MenuState::Home;
            display.showHomeScreen( audio.isPlaying, audio.volume, connections.audioSource );
        }
        break;
    case DisplayUI::MenuState::SourceSelect:
        if( isShort ) {
            // short press
            InputSource selected = display.getInputSourceSelected();
            handleAudio_ChangeSource( selected );
        }
        // Go home in both cases
        state = DisplayUI::MenuState::Home;
        display.showHomeScreen( audio.isPlaying, audio.volume, connections.audioSource );
        break;
    case DisplayUI::MenuState::BTPairing:
        // Go home in both cases
        state = DisplayUI::MenuState::Home;
        display.showHomeScreen( audio.isPlaying, audio.volume, connections.audioSource );
        break;
    }
}
