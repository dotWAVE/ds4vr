#pragma once

#include <cstddef>
#include <cstdint>

#include "input_state.h"

namespace ds4vr {

// Decodes a single DualSense USB input report (Report ID 0x01, 64 bytes).
// Returns true on a recognised report; leaves `out` untouched on rejection.
bool ParseDualSenseUsbReport(const uint8_t *data, std::size_t len, InputState &out);

} // namespace ds4vr
