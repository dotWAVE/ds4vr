#pragma once

#include <openvr_driver.h>

#include <cmath>

namespace ds4vr {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct Quat {
    double w = 1.0, x = 0.0, y = 0.0, z = 0.0;
};

inline Quat QuatFromYaw(double yaw_rad)
{
    const double half = yaw_rad * 0.5;
    return { std::cos(half), 0.0, std::sin(half), 0.0 };
}

inline Vec3 QuatRotateVec(const Quat &q, const Vec3 &v)
{
    // q * v * q^-1, expanded for a unit quaternion.
    const double tx = 2.0 * (q.y * v.z - q.z * v.y);
    const double ty = 2.0 * (q.z * v.x - q.x * v.z);
    const double tz = 2.0 * (q.x * v.y - q.y * v.x);
    return {
        v.x + q.w * tx + (q.y * tz - q.z * ty),
        v.y + q.w * ty + (q.z * tx - q.x * tz),
        v.z + q.w * tz + (q.x * ty - q.y * tx),
    };
}

inline Quat QuatConj(const Quat &q)
{
    return { q.w, -q.x, -q.y, -q.z };
}

inline Quat QuatFromFloats(const float f[4])
{
    return { static_cast<double>(f[0]), static_cast<double>(f[1]),
             static_cast<double>(f[2]), static_cast<double>(f[3]) };
}

inline Quat QuatMul(const Quat &a, const Quat &b)
{
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
    };
}

inline double Vec3Length(const Vec3 &v)
{
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

inline Vec3 Vec3Normalize(const Vec3 &v)
{
    const double len = Vec3Length(v);
    if (len < 1e-12) return {0.0, 0.0, -1.0};
    return { v.x/len, v.y/len, v.z/len };
}

inline Vec3 Vec3Scale(const Vec3 &v, double s)
{
    return { v.x*s, v.y*s, v.z*s };
}

inline Vec3 Vec3Add(const Vec3 &a, const Vec3 &b)
{
    return { a.x+b.x, a.y+b.y, a.z+b.z };
}

inline Vec3 Vec3Sub(const Vec3 &a, const Vec3 &b)
{
    return { a.x-b.x, a.y-b.y, a.z-b.z };
}

inline Quat QuatSlerp(const Quat &a, const Quat &b, double t)
{
    double dot = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
    Quat b2 = b;
    if (dot < 0.0) { dot = -dot; b2 = {-b.w, -b.x, -b.y, -b.z}; }
    if (dot > 0.9995) {
        Quat r = { a.w + t*(b2.w-a.w), a.x + t*(b2.x-a.x),
                   a.y + t*(b2.y-a.y), a.z + t*(b2.z-a.z) };
        double n = 1.0 / std::sqrt(r.w*r.w + r.x*r.x + r.y*r.y + r.z*r.z);
        return { r.w*n, r.x*n, r.y*n, r.z*n };
    }
    double theta = std::acos(dot);
    double s0 = std::sin((1.0-t)*theta) / std::sin(theta);
    double s1 = std::sin(t*theta) / std::sin(theta);
    return { s0*a.w + s1*b2.w, s0*a.x + s1*b2.x,
             s0*a.y + s1*b2.y, s0*a.z + s1*b2.z };
}

inline Vec3 Vec3Lerp(const Vec3 &a, const Vec3 &b, double t)
{
    return { a.x + t*(b.x-a.x), a.y + t*(b.y-a.y), a.z + t*(b.z-a.z) };
}

inline vr::HmdQuaternion_t ToHmd(const Quat &q)
{
    return { q.w, q.x, q.y, q.z };
}

// Extract position from the HMD's 3×4 device-to-absolute matrix.
inline Vec3 PosFromMatrix(const vr::HmdMatrix34_t &m)
{
    return { m.m[0][3], m.m[1][3], m.m[2][3] };
}

// Extract yaw (rotation about +Y) from a 3×4 matrix. The matrix is
// right-handed with +Y up and -Z forward. Yaw = atan2(forward.x, -forward.z)
// where forward is the third column negated.
inline double YawFromMatrix(const vr::HmdMatrix34_t &m)
{
    return std::atan2(m.m[0][2], m.m[2][2]);
}

// HMD pitch: how far the user is looking up (+) or down (-).
// Forward = -Z column of the rotation matrix.
inline double PitchFromMatrix(const vr::HmdMatrix34_t &m)
{
    return std::asin(std::max(-1.0, std::min(1.0,
        -static_cast<double>(m.m[1][2]))));
}

} // namespace ds4vr
