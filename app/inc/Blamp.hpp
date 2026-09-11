#pragma once

#include "MainSysQ.hpp"
#include "ContextAudio.hpp"
#include "ContextConnections.hpp"
#include "DisplayUI.hpp"


/**
 * Main system coordinator
 */
class Blamp {
private:
    inline static ContextAudio audio;
    inline static ContextConnections connections;
    inline static DisplayUI display;
    inline static DisplayUI::MenuState state;
private:
    // MainSysQ::Enum::Blampknob_*
    static void handleBlampknob_Position( const MainSysQ_BlampKnob_Position& position );
    static void handleBlampknob_Press( const MainSysQ_BlampKnob_Press& press );

    // MainSysQ::Enum::Audio_*
    static void handleAudio_Playback( const MainSysQ_Audio_Playback& action );
    static void handleAudio_ChangeSource( const MainSysQ_Audio_ChangeSource& source );
    static void handleAudio_Volume( const MainSysQ_Audio_Volume& volume );
    static void handleAudio_TAS3251Err( const MainSysQ_Audio_TAS3251Err& err );
    static void handleAudio_PCM9211Err( const MainSysQ_Audio_PCM9211Err& err );
public:
    static void init();
    static void processEvents( const MainSysQ& message );
};