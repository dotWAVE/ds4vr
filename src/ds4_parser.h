#pragma once

#include <cstddef>
#include <cstdint>

#include "input_state.h"

namespace ds4vr {

// Decodes a single DS4 USB input report (Report ID 0x01, 64 bytes — spec §10).
// Returns true on a recognised report; leaves `out` untouched on rejection.
//
// The first byte of `data` is the report ID. Phase 1 handles only USB (0x01);
// Bluetooth (ID 0x11) lives behind the future HAL.
bool ParseDs4UsbReport(const uint8_t *data, std::size_t len, InputState &out);

} // namespace ds4vr
