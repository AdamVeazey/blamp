#pragma once

#include <cstdint>

struct ContextAudio {
    int32_t volume = 0;
    bool isPlaying = false;
    bool isMuted = false;
};