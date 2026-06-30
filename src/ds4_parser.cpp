#include "ds4_parser.h"

namespace ds4vr {

namespace {

// 8-axis hat: 0=N, 1=NE, ... 7=NW, 8=released.
void DecodeHat(uint8_t hat, InputState &out)
{
    out.dpad_up    = (hat == 7 || hat == 0 || hat == 1);
    out.dpad_right = (hat == 1 || hat == 2 || hat == 3);
    out.dpad_down  = (hat == 3 || hat == 4 || hat == 5);
    out.dpad_left  = (hat == 5 || hat == 6 || hat == 7);
}

// raw uint8 stick axis (0..255, ~128 centred) → [-1, 1]. Y is flipped so
// pushing the stick up returns +1 (matches OpenXR/SteamVR joystick convention).
float StickNorm(uint8_t raw) { return (static_cast<float>(raw) - 128.0f) / 127.5f; }

int16_t ReadI16LE(const uint8_t *p) {
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

// BMI055 defaults: ±2000 dps gyro, ±4 g accel (spec §10).
constexpr float kGyroScale  = (2000.0f / 32768.0f) * (3.14159265f / 180.0f); // → rad/s
constexpr float kAccelScale = 4.0f / 32768.0f;                                // → g

} // namespace

bool ParseDs4UsbReport(const uint8_t *data, std::size_t len, InputState &out)
{
    // USB report ID 0x01, 64 bytes. Need at least 25 for full IMU.
    if (data == nullptr || len < 25 || data[0] != 0x01) {
        return false;
    }

    InputState s;

    s.left_stick_x  =  StickNorm(data[1]);
    s.left_stick_y  = -StickNorm(data[2]);
    s.right_stick_x =  StickNorm(data[3]);
    s.right_stick_y = -StickNorm(data[4]);

    // Byte 5: low nibble = D-pad hat; high nibble = face buttons.
    DecodeHat(static_cast<uint8_t>(data[5] & 0x0F), s);
    s.square   = (data[5] & 0x10) != 0;
    s.cross    = (data[5] & 0x20) != 0;
    s.circle   = (data[5] & 0x40) != 0;
    s.triangle = (data[5] & 0x80) != 0;

    // Byte 6: bumpers, trigger digital bits, share/options, stick clicks.
    s.l1      = (data[6] & 0x01) != 0;
    s.r1      = (data[6] & 0x02) != 0;
    // bits 2/3 are L2/R2 digital — redundant with the analog values; ignore.
    s.share   = (data[6] & 0x10) != 0;
    s.options = (data[6] & 0x20) != 0;
    s.l3      = (data[6] & 0x40) != 0;
    s.r3      = (data[6] & 0x80) != 0;

    // Byte 7: PS button + touchpad click (upper bits are a report counter).
    s.ps             = (data[7] & 0x01) != 0;
    s.touchpad_click = (data[7] & 0x02) != 0;

    // Bytes 8/9: analog triggers 0..255.
    s.l2 = static_cast<float>(data[8]) / 255.0f;
    s.r2 = static_cast<float>(data[9]) / 255.0f;

    // Bytes 13–18: gyro X/Y/Z (int16 LE), bytes 19–24: accel X/Y/Z (int16 LE).
    s.gyro[0] = static_cast<float>(ReadI16LE(&data[13])) * kGyroScale;
    s.gyro[1] = static_cast<float>(ReadI16LE(&data[15])) * kGyroScale;
    s.gyro[2] = static_cast<float>(ReadI16LE(&data[17])) * kGyroScale;

    s.accel[0] = static_cast<float>(ReadI16LE(&data[19])) * kAccelScale;
    s.accel[1] = static_cast<float>(ReadI16LE(&data[21])) * kAccelScale;
    s.accel[2] = static_cast<float>(ReadI16LE(&data[23])) * kAccelScale;

    s.connected = true;
    out = s;
    return true;
}

} // namespace ds4vr
