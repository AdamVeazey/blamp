#pragma once

#include <cstdint>
#include "InputSource.hpp"

struct ContextConnections {
    bool bluetoothConnected = false;
    bool hdmiArcActive = false;
    bool auxConnected = false;
    bool usbConnected = false;
    InputSource audioSource = InputSource::UNKNOWN;
};