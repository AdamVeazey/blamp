#include <cstdlib> // For atoi
#include <zephyr/shell/shell.h>

#include "MainSysQ.hpp"
#include "QuadratureEncoder.hpp"

// ----------------------------------------------------
// Sub-command: knob pos [ticks]
// ----------------------------------------------------
static int cmd_knob_pos(const struct shell *sh, size_t argc, char **argv) {
    // If no argument is passed, just "rotate right by 1 tick" as a quick default
    int32_t ticks = 1;
    if (argc > 1) {
        // Check for quick directional words
        if (strcmp(argv[1], "right") == 0) {
            ticks = 1;
        } else if (strcmp(argv[1], "left") == 0) {
            ticks = -1;
        } else {
            // Otherwise parse the integer (handles negative signs like -5 perfectly)
            ticks = atoi(argv[1]);
        }
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
    if (argc > 1 && strcmp(argv[1], "long") == 0) {
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

