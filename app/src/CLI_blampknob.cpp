#include <zephyr/shell/shell.h>
#include <EDF/String.hpp>
#include "MainSysQ.hpp"
#include "QuadratureEncoder.hpp"

// ----------------------------------------------------
// Sub-command: knob pos [ticks]
// ----------------------------------------------------
static int cmd_knob_pos(const struct shell *sh, size_t argc, char **argv) {
    // If no argument is passed, just "rotate right by 4 ticks" as a quick default
    int32_t ticks = 4;
    if (argc > 1) {
        EDF::String<33> arg(argv[1]);
        if( arg.equals("right") )     ticks = 4;
        else if( arg.equals("left") ) ticks = -4;
        else                          ticks = arg.toInt32_t();
    }
    int32_t prev = MockQuadratureEncoder::getValue();
    MockQuadratureEncoder::mockRotate(ticks);
    int32_t current = MockQuadratureEncoder::getValue();
    MainSysQ_BlampKnob_Position(prev, current).send();
    return 0;
}

// ----------------------------------------------------
// Sub-command: knob press [short/long]
// ----------------------------------------------------
static int cmd_knob_press(const struct shell *sh, size_t argc, char **argv) {
    bool mockPressShort = true;
    if( argc > 1 && EDF::String<16>(argv[1]).equals("long") ) {
        mockPressShort = false;
    }
    using T = MainSysQ_BlampKnob_Press::Type;
    MainSysQ_BlampKnob_Press(mockPressShort ? T::Short : T::Long).send();
    return 0;
}

// ----------------------------------------------------
// Define the Sub-command Look-up Table
// ----------------------------------------------------
SHELL_STATIC_SUBCMD_SET_CREATE(sub_knob,
    SHELL_CMD_ARG(pos, NULL,
                  "Rotate knob. Usage: 'blampknob pos [ticks / left / right]'. Defaults to 1 tick.",
                  cmd_knob_pos, 1, 1
    ),
    SHELL_CMD_ARG(press, NULL,
                  "Press button. Usage: 'blampknob press [short / long]'. Defaults to short.",
                  cmd_knob_press, 1, 1
    ),
    SHELL_SUBCMD_SET_END
);

// Register the root master command
SHELL_CMD_REGISTER(blampknob, &sub_knob, "Mock BlampKnob encoder/switch inputs", NULL);

