#include "Blamp.hpp"


void Blamp::
init() {
    MainSysQ::init();
    display.init();
    display.showHomeScreen( audio.isPlaying, audio.volume, connections.audioSource );
    state = DisplayUI::MenuState::Home;
}

void Blamp::
processEvents( const MainSysQ& message ) {
    using E = MainSysQ::Enum;
    switch( message.getId() ) {
    case E::BlampKnob_Position: handleBlampknob_Position( (const MainSysQ_BlampKnob_Position&)message );    break;
    case E::BlampKnob_Press:    handleBlampknob_Press( (const MainSysQ_BlampKnob_Press&)message );          break;

    case E::Audio_Playback:     handleAudio_Playback( (const MainSysQ_Audio_Playback&)message );            break;
    case E::Audio_ChangeSource: handleAudio_ChangeSource( (const MainSysQ_Audio_ChangeSource&)message );    break;
    case E::Audio_Volume:       handleAudio_Volume( (const MainSysQ_Audio_Volume&)message );                break;
    case E::Audio_TAS3251Err:   handleAudio_TAS3251Err( (const MainSysQ_Audio_TAS3251Err&)message );        break;
    case E::Audio_PCM9211Err:   handleAudio_PCM9211Err( (const MainSysQ_Audio_PCM9211Err&)message );        break;
    default:                                                                                                break;
    }
}
