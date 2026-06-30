#include "dualsense_parser.h"

namespace ds4vr {

namespace {

// Hat decoding is identical to DS4: 0=N, 1=NE, ... 7=NW, 8=released.
void DecodeHat(uint8_t hat, InputState &out)
{
    out.dpad_up    = (hat == 7 || hat == 0 || hat == 1);
    out.dpad_right = (hat == 1 || hat == 2 || hat == 3);
    out.dpad_down  = (hat == 3 || hat == 4 || hat == 5);
    out.dpad_left  = (hat == 5 || hat == 6 || hat == 7);
}

float StickNorm(uint8_t raw) { return (static_cast<float>(raw) - 128.0f) / 127.5f; }

int16_t ReadI16LE(const uint8_t *p) {
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

// Same scale factors as DS4 (similar IMU ranges — spec says "calibrate at implementation").
constexpr float kGyroScale  = (2000.0f / 32768.0f) * (3.14159265f / 180.0f);
constexpr float kAccelScale = 4.0f / 32768.0f;

} // namespace

bool ParseDualSenseUsbReport(const uint8_t *data, std::size_t len, InputState &out)
{
    // DualSense USB: Report ID 0x01, 64 bytes.
    //
    //   0:     Report ID (0x01)
    //   1–4:   Sticks (LX, LY, RX, RY)
    //   5–6:   Triggers (L2, R2)
    //   7:     Sequence counter
    //   8–10:  Buttons (3 bytes)
    //   11:    Reserved
    //   12–15: Timestamp (uint32 LE)
    //   16–21: Gyro X/Y/Z (3× int16 LE)
    //   22–27: Accel X/Y/Z (3× int16 LE)

    if (data == nullptr || len < 28 || data[0] != 0x01) {
        return false;
    }

    InputState s;

    s.left_stick_x  =  StickNorm(data[1]);
    s.left_stick_y  = -StickNorm(data[2]);
    s.right_stick_x =  StickNorm(data[3]);
    s.right_stick_y = -StickNorm(data[4]);

    s.l2 = static_cast<float>(data[5]) / 255.0f;
    s.r2 = static_cast<float>(data[6]) / 255.0f;

    // Byte 8: hat + face buttons (same bit layout as DS4 byte 5).
    DecodeHat(static_cast<uint8_t>(data[8] & 0x0F), s);
    s.square   = (data[8] & 0x10) != 0;
    s.cross    = (data[8] & 0x20) != 0;
    s.circle   = (data[8] & 0x40) != 0;
    s.triangle = (data[8] & 0x80) != 0;

    // Byte 9: bumpers, share/options, stick clicks (same bit layout as DS4 byte 6).
    s.l1      = (data[9] & 0x01) != 0;
    s.r1      = (data[9] & 0x02) != 0;
    s.share   = (data[9] & 0x10) != 0;  // "Create" on DualSense
    s.options = (data[9] & 0x20) != 0;
    s.l3      = (data[9] & 0x40) != 0;
    s.r3      = (data[9] & 0x80) != 0;

    // Byte 10: PS, touchpad click, mute.
    s.ps             = (data[10] & 0x01) != 0;
    s.touchpad_click = (data[10] & 0x02) != 0;
    // Mute button (bit 2) — no mapping in spec; ignored.

    // Bytes 16–21: gyro X/Y/Z (int16 LE), bytes 22–27: accel X/Y/Z (int16 LE).
    s.gyro[0] = static_cast<float>(ReadI16LE(&data[16])) * kGyroScale;
    s.gyro[1] = static_cast<float>(ReadI16LE(&data[18])) * kGyroScale;
    s.gyro[2] = static_cast<float>(ReadI16LE(&data[20])) * kGyroScale;

    s.accel[0] = static_cast<float>(ReadI16LE(&data[22])) * kAccelScale;
    s.accel[1] = static_cast<float>(ReadI16LE(&data[24])) * kAccelScale;
    s.accel[2] = static_cast<float>(ReadI16LE(&data[26])) * kAccelScale;

    s.connected = true;
    out = s;
    return true;
}

} // namespace ds4vr
