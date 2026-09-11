#include "Blamp.hpp"

#include <zephyr/logging/log.h>
#include "DisplayUI.hpp"

LOG_MODULE_REGISTER(blamp_audio, LOG_LEVEL_INF);

void Blamp::
handleAudio_Playback( const MainSysQ_Audio_Playback& action ) {
    using A = MainSysQ_Audio_Playback::Action;
    auto a = action.getAction();
    switch( a ) {
    case A::Mute:
        // send TAS3251 command, maybe toggle GPIO? however it works
        return;
    case A::Unmute:
        // send TAS3251 command, maybe toggle GPIO? however it works
        return;
    case A::Play:  // intentionally left empty
    case A::Pause: // intentionally left empty
    default:
        break;
    }

    bool success = false;

    switch(connections.audioSource) {
    case InputSource::AUX_IN:
    case InputSource::RCA_DIGITAL_COAX:
    case InputSource::OPTICAL:
        LOG_WRN("No way to Play/Pause this InputSource: %s", toString(connections.audioSource));
        return;
    case InputSource::HDMI_ARC:
        if( connections.hdmiArcActive ) {
            // send CEC command (I think?) to play/pause
            // hdmi.mediaControl( a );
            success = true;
        }
        break;
    case InputSource::USB:
        if( connections.usbConnected ) {
            // send USB control back to source to play/pause
            // usb.mediaControl( a );
            success = true;
        }
        break;
    case InputSource::BLUETOOTH:
        if( connections.bluetoothConnected ) {
            // send AT command to module to play/pause
            // bt.mediaControl( a );
            success = true;
        }
        break;
    case InputSource::UNKNOWN:
    default:
        LOG_ERR("Unknown InputSource!");
        return;
    }

    if( success ) {
        audio.isPlaying = (a == A::Play);
        if( state == DisplayUI::MenuState::Home )
            display.updateHomeAudioPlaying( audio.isPlaying );
    }
    else {
        LOG_WRN("Playback ignored: %s disconnected or inactive.", toString(connections.audioSource));
    }
}

void Blamp::
handleAudio_ChangeSource( const MainSysQ_Audio_ChangeSource& source ) {
    auto s = source.getInputSource();
    switch(s){
    case InputSource::AUX_IN:
        if( connections.auxConnected ) connections.audioSource = s;
        break;
    case InputSource::RCA_DIGITAL_COAX:
    case InputSource::OPTICAL:
        connections.audioSource = s;
        LOG_DBG("No way to know if %s is connected!", toString(s));
        break;
    case InputSource::HDMI_ARC:
        if( connections.hdmiArcActive ) connections.audioSource = s;
        break;
    case InputSource::USB:
        if( connections.usbConnected ) connections.audioSource = s;
        break;
    case InputSource::BLUETOOTH:
        if( connections.bluetoothConnected ) connections.audioSource = s;
        break;
    case InputSource::UNKNOWN:
    default:
        LOG_ERR("Unknown InputSource!");
        return;
    }
    if( connections.audioSource == s ) {
        // PCM9211 I2C write to change source
        LOG_INF("InputSource changed to %s", toString(connections.audioSource));
        if( !canPlayPause( connections.audioSource ) ) {
            // can't control play/pause, assume always playing?
            audio.isPlaying = true;
        }
        if( state == DisplayUI::MenuState::Home ) {
            display.updateHomeAudioPlaying( audio.isPlaying );
            display.updateHomeInputSource( connections.audioSource );
        }
    }
    else {
        LOG_ERR("InputSource unable to change to %s. Check connection", toString(s));
    }
}

void Blamp::
handleAudio_Volume( const MainSysQ_Audio_Volume& volume ) {
    // TAS3251 I2C write to set volume
    audio.volume = volume.getVolumePercent();
    if( state == DisplayUI::MenuState::Home ) {
        display.updateHomeVolume( audio.volume );
    }
}

void Blamp::
handleAudio_TAS3251Err( const MainSysQ_Audio_TAS3251Err& err ) {
    // handle TAS3251 error
    using Source = MainSysQ_Audio_TAS3251Err::Source;
    Source s = err.getSource();
    switch(s) {
    case Source::nCLIP_OTW: break;
    case Source::nFAULT:    break;
    default:                break;
    }
}

void Blamp::
handleAudio_PCM9211Err( const MainSysQ_Audio_PCM9211Err& err ) {
    // handle PCM9211 error
    using Source = MainSysQ_Audio_PCM9211Err::Source;
    Source s = err.getSource();
    switch(s) {
    case Source::INT0:  break;
    case Source::INT1:  break;
    default:            break;
    }
}
