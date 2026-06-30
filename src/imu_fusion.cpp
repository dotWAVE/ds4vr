#include "imu_fusion.h"

#include <cmath>

namespace ds4vr {

// --- GyroBias ---

void GyroBias::Update(const float gyro[3], const float accel[3])
{
    const float accel_mag = std::sqrtf(
        accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
    const float gyro_mag = std::sqrtf(
        gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2]);

    if (accel_mag < kAccelLo || accel_mag > kAccelHi || gyro_mag > kGyroThresh) {
        return;
    }

    if (!initialised_) {
        bias_[0] = gyro[0];
        bias_[1] = gyro[1];
        bias_[2] = gyro[2];
        initialised_ = true;
        return;
    }

    for (int i = 0; i < 3; ++i) {
        bias_[i] += kAlpha * (gyro[i] - bias_[i]);
    }
}

void GyroBias::Apply(float gyro[3]) const
{
    if (!initialised_) return;
    gyro[0] -= bias_[0];
    gyro[1] -= bias_[1];
    gyro[2] -= bias_[2];
}

// --- MadgwickFilter (Y-up) ---
//
// Reference direction: accelerometer at rest reads "up" = (0, +1, 0).
// Estimated up direction in sensor frame via rotation matrix R(q):
//
//   h = R(q) * (0, 1, 0)
//     = ( 2(xy - wz),  1 - 2(x² + z²),  2(yz + wx) )
//
// Objective f = h - a_measured, Jacobian J = ∂f/∂q, gradient step = J^T·f.

void MadgwickFilter::Update(float gx, float gy, float gz,
                            float ax, float ay, float az,
                            float dt)
{
    float q0 = q_[0], q1 = q_[1], q2 = q_[2], q3 = q_[3];

    // Gyro-based quaternion rate of change: q̇ = ½ q ⊗ ω
    float qDot0 = 0.5f * (-q1*gx - q2*gy - q3*gz);
    float qDot1 = 0.5f * ( q0*gx + q2*gz - q3*gy);
    float qDot2 = 0.5f * ( q0*gy - q1*gz + q3*gx);
    float qDot3 = 0.5f * ( q0*gz + q1*gy - q2*gx);

    // Accelerometer correction (pitch & roll only — yaw has no reference).
    float norm = std::sqrtf(ax*ax + ay*ay + az*az);
    if (norm > 0.001f) {
        float inv = 1.0f / norm;
        ax *= inv; ay *= inv; az *= inv;

        // Estimated "up" in sensor frame.
        float hx = 2.0f*(q1*q2 - q0*q3);
        float hy = 1.0f - 2.0f*(q1*q1 + q3*q3);
        float hz = 2.0f*(q2*q3 + q0*q1);

        // Objective.
        float f1 = hx - ax;
        float f2 = hy - ay;
        float f3 = hz - az;

        // Gradient = J^T · f.
        float s0 = -2.0f*q3*f1               + 2.0f*q1*f3;
        float s1 =  2.0f*q2*f1 - 4.0f*q1*f2 + 2.0f*q0*f3;
        float s2 =  2.0f*q1*f1               + 2.0f*q3*f3;
        float s3 = -2.0f*q0*f1 - 4.0f*q3*f2 + 2.0f*q2*f3;

        norm = std::sqrtf(s0*s0 + s1*s1 + s2*s2 + s3*s3);
        if (norm > 0.001f) {
            inv = 1.0f / norm;
            qDot0 -= beta_ * s0 * inv;
            qDot1 -= beta_ * s1 * inv;
            qDot2 -= beta_ * s2 * inv;
            qDot3 -= beta_ * s3 * inv;
        }
    }

    // Integrate.
    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;

    // Normalise.
    norm = 1.0f / std::sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q_[0] = q0 * norm;
    q_[1] = q1 * norm;
    q_[2] = q2 * norm;
    q_[3] = q3 * norm;
}

void MadgwickFilter::GetQuat(float out[4]) const
{
    out[0] = q_[0];
    out[1] = q_[1];
    out[2] = q_[2];
    out[3] = q_[3];
}

// --- ImuFusion ---

void ImuFusion::Update(const float gyro[3], const float accel[3], float dt)
{
    float g[3] = { gyro[0], gyro[1], gyro[2] };

    bias_.Update(g, accel);
    bias_.Apply(g);

    filter_.Update(g[0], g[1], g[2], accel[0], accel[1], accel[2], dt);
}

} // namespace ds4vr
