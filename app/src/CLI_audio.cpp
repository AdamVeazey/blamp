#include <zephyr/shell/shell.h>
#include <EDF/String.hpp>
#include "MainSysQ.hpp"

static int cmd_audio_playback(const struct shell *sh, size_t argc, char **argv) {
    EDF::String<16> arg(argv[0]);
    if( arg.equals("play") )     MainSysQ_Audio_Playback(PlaybackAction::Play).send();
    if( arg.equals("pause") )    MainSysQ_Audio_Playback(PlaybackAction::Pause).send();
    if( arg.equals("forward") )  MainSysQ_Audio_Playback(PlaybackAction::Forward).send();
    if( arg.equals("backward") ) MainSysQ_Audio_Playback(PlaybackAction::Backward).send();
    // intentionally skipped error handling, CLI already deals with it for me
    return 0;
}

static int cmd_audio_input(const struct shell *sh, size_t argc, char **argv) {
    EDF::String<16> arg(argv[0]);
    if( arg.equals("aux") )            MainSysQ_Audio_ChangeSource(InputSource::AUX_IN).send();
    else if( arg.equals("coax") )      MainSysQ_Audio_ChangeSource(InputSource::RCA_DIGITAL_COAX).send();
    else if( arg.equals("optical") )   MainSysQ_Audio_ChangeSource(InputSource::OPTICAL).send();
    else if( arg.equals("hdmi") )      MainSysQ_Audio_ChangeSource(InputSource::HDMI_ARC).send();
    else if( arg.equals("usb") )       MainSysQ_Audio_ChangeSource(InputSource::USB).send();
    else if( arg.equals("bluetooth") ) MainSysQ_Audio_ChangeSource(InputSource::BLUETOOTH).send();
    // intentionally skipped error handling, CLI already deals with it for me
    return 0;
}

// ----------------------------------------------------
// Define the Sub-command Look-up Table
// ----------------------------------------------------
SHELL_STATIC_SUBCMD_SET_CREATE(sub_input,
    SHELL_CMD(aux,       NULL, "Selects InputSource::AUX_IN", cmd_audio_input),
    SHELL_CMD(coax,      NULL, "Selects InputSource::RCA_DIGITAL_COAX", cmd_audio_input),
    SHELL_CMD(optical,   NULL, "Selects InputSource::OPTICAL", cmd_audio_input),
    SHELL_CMD(hdmi,      NULL, "Selects InputSource::HDMI_ARC", cmd_audio_input),
    SHELL_CMD(usb,       NULL, "Selects InputSource::USB", cmd_audio_input),
    SHELL_CMD(bluetooth, NULL, "Selects InputSource::Bluetooth", cmd_audio_input),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_audio,
    SHELL_CMD(play,  NULL, "Play", cmd_audio_playback),
    SHELL_CMD(pause, NULL, "Pause", cmd_audio_playback),
    SHELL_CMD(forward, NULL, "Forward", cmd_audio_playback),
    SHELL_CMD(backward, NULL, "Backward", cmd_audio_playback),
    SHELL_CMD(input, &sub_input, "Select input source: AUX, COAX, OPTICAL, HDMI, USB, Bluetooth", NULL),
    SHELL_SUBCMD_SET_END
);

// Register the root master command
SHELL_CMD_REGISTER(audio, &sub_audio, "Audio controls", NULL);
