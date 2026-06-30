#pragma once

#include <cstdint>

namespace ds4vr {

// Gyro bias estimator: averages gyro readings during detected still periods.
// "Still" = |accel| close to 1 g and |gyro| small. Runs continuously;
// exponential moving average with a slow alpha so it converges over seconds.
class GyroBias {
public:
    void Update(const float gyro[3], const float accel[3]);
    void Apply(float gyro[3]) const;

private:
    float bias_[3]       = {0, 0, 0};
    bool  initialised_   = false;
    static constexpr float kAlpha       = 0.001f;  // EMA smoothing
    static constexpr float kGyroThresh  = 0.05f;   // rad/s — must be nearly motionless
    static constexpr float kAccelLo     = 0.9f;    // g magnitude window for "at rest"
    static constexpr float kAccelHi     = 1.1f;
};

// Madgwick AHRS filter (IMU-only, no magnetometer). Corrects pitch/roll via
// gravity; yaw is gyro-only and drifts — by design (spec §2/§7.3).
// Coordinate convention: Y-up (matches SteamVR standing space).
class MadgwickFilter {
public:
    explicit MadgwickFilter(float beta = 0.04f) : beta_(beta) {}

    // gyro in rad/s, accel in g, dt in seconds.
    void Update(float gx, float gy, float gz,
                float ax, float ay, float az,
                float dt);

    // Current fused orientation quaternion (w, x, y, z).
    void GetQuat(float out[4]) const;

    void SetBeta(float b) { beta_ = b; }

private:
    float q_[4] = {1, 0, 0, 0};
    float beta_;
};

// Combined pipeline: bias removal → Madgwick fusion.
// Owns the filter state; call Update() per HID report.
class ImuFusion {
public:
    explicit ImuFusion(float beta = 0.04f) : filter_(beta) {}

    // Pass parsed gyro (rad/s) and accel (g). dt = seconds since last call.
    void Update(const float gyro[3], const float accel[3], float dt);

    // Fused orientation (w, x, y, z).
    void GetQuat(float out[4]) const { filter_.GetQuat(out); }

private:
    GyroBias       bias_;
    MadgwickFilter filter_;
};

} // namespace ds4vr
