#pragma once

#include <cstdint>

namespace ds4vr {

// Snapshot of one DS4 report, normalised into engine-friendly units.
// Produced by ds4_parser, published by HidDevice, consumed by TouchDevice.
//
// Phase 1: buttons + sticks + analog triggers. IMU/touchpad-touch fields are
// reserved here so phase 3 can populate them without re-plumbing.
struct InputState {
    // Sticks: raw normalised [-1, 1], no deadzone applied. Y is flipped so
    // "up on the stick" == +1.
    float left_stick_x  = 0.0f;
    float left_stick_y  = 0.0f;
    float right_stick_x = 0.0f;
    float right_stick_y = 0.0f;

    // Analog triggers: 0..1.
    float l2 = 0.0f;
    float r2 = 0.0f;

    // Shoulder bumpers — reserved per spec §6 / §7.4 (engagement). NOT routed
    // to any /input/* component here; surfaced only so the engagement FSM in
    // phase 4 can read them.
    bool l1 = false;
    bool r1 = false;

    // Stick clicks.
    bool l3 = false;
    bool r3 = false;

    // Face / center.
    bool square   = false;
    bool cross    = false;
    bool circle   = false;
    bool triangle = false;
    bool share    = false;
    bool options  = false;
    bool ps       = false;
    bool touchpad_click = false;

    // D-pad (hat decoded into four booleans).
    bool dpad_up    = false;
    bool dpad_right = false;
    bool dpad_down  = false;
    bool dpad_left  = false;

    // Set true on any successful parse; HidDevice resets to default on
    // disconnect so consumers see "all released, sticks centred".
    bool connected = false;

    // IMU data in physical units (populated by parser, phase 3+).
    float gyro[3]  = {0, 0, 0};   // rad/s  (X=pitch, Y=yaw, Z=roll in sensor frame)
    float accel[3] = {0, 0, 0};   // g      (X, Y, Z in sensor frame)

    // Fused controller orientation from the Madgwick filter (set by HidDevice
    // after each IMU update, NOT by the parser). Quaternion (w, x, y, z).
    float imu_quat[4] = {1, 0, 0, 0};
};

} // namespace ds4vr
