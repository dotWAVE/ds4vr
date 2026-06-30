#pragma once

#include <cstdint>
#include <string>

namespace ds4vr {

enum class ButtonTarget : uint8_t {
    Unbound,
    FaceUpper,   // Y (left hand) or B (right hand)
    FaceLower,   // X (left hand) or A (right hand)
    Grip,
    System,
};

struct Config {
    // [sticks]
    float stick_deadzone   = 0.08f;
    float response_curve   = 1.0f;

    // [trigger]
    float trigger_click_threshold = 0.85f;

    // [imu]
    float imu_beta = 0.04f;

    // [engage]
    float t_dtap_ms = 250.0f;

    // [armmodel]
    float shoulder_left[3]  = { -0.18f, -0.24f, -0.05f };
    float shoulder_right[3] = {  0.18f, -0.24f, -0.05f };
    float forearm_nominal   = 0.26f;
    float elbow_lift_max    = 0.12f;
    float pitch_lo_deg      = -20.0f;
    float pitch_hi_deg      =  70.0f;
    float pose_blend_ms         = 100.0f;
    float hmd_pitch_influence   = 0.35f;  // how much HMD pitch steers arms at max reach

    // [reach]
    float reach_rest       = 0.90f;  // reach used for the resting display pose
    float rest_outward_deg = 30.0f;  // outward lateral yaw per hand at rest
    float reach_default  = 0.45f;
    float reach_min      = 0.20f;
    float reach_max      = 0.70f;
    float reach_rate     = 0.60f;
    float reach_deadzone = 0.12f;
    bool  reach_invert   = false;

    // [mapping]
    bool grip_toggle = true;   // true = grip button toggles; false = hold

    // [mapping.left]  — which Touch component each DS4 button drives
    ButtonTarget left_dpad_up    = ButtonTarget::FaceUpper;  // Y
    ButtonTarget left_dpad_left  = ButtonTarget::FaceLower;  // X
    ButtonTarget left_dpad_down  = ButtonTarget::Unbound;
    ButtonTarget left_dpad_right = ButtonTarget::Grip;

    // [mapping.right]
    ButtonTarget right_cross    = ButtonTarget::FaceLower;   // A
    ButtonTarget right_circle   = ButtonTarget::FaceUpper;   // B
    ButtonTarget right_square   = ButtonTarget::Grip;
    ButtonTarget right_triangle = ButtonTarget::Unbound;

    // [haptics] — which rumble motor each hand drives
    bool haptic_left_large  = true;   // true = large motor
    bool haptic_right_large = false;  // false = small motor
};

// Singleton. Valid after LoadConfig() is called (or defaults).
const Config &Cfg();

// Parse ds4vr.ini into the global config. Returns false if file not found
// (defaults remain). Logs parsed values.
bool LoadConfig(const std::string &path);

// Compute the ini path from the DLL location (bin/win64/ → ../../ds4vr.ini).
std::string GetIniPath();

} // namespace ds4vr
