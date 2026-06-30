#include "touch_device.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config.h"

namespace ds4vr {

namespace {

constexpr vr::HmdQuaternion_t kQuatIdentity{1.0, 0.0, 0.0, 0.0};
constexpr double kPi = 3.14159265358979323846;

// Elbow rest offset relative to shoulder (not yet in ini — local tunable).
constexpr Vec3 kElbowRestOffset{0.0, -0.22, -0.05};

// q_neutral: hands point straight down (pitch -90°) for waist-level resting pose.
constexpr Quat kNeutral{0.7071, -0.7071, 0.0, 0.0};

double Clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

void ApplyRadialDeadzone(float &x, float &y, float dz)
{
    const float m = std::hypotf(x, y);
    if (m < dz) { x = 0.0f; y = 0.0f; return; }
    const float clamped = std::min(m, 1.0f);
    const float scale   = ((clamped - dz) / (1.0f - dz)) / m;
    x *= scale;
    y *= scale;
}

float DeadzoneScalar(float v, float dz)
{
    if (std::fabsf(v) < dz) return 0.0f;
    float sign = v > 0.0f ? 1.0f : -1.0f;
    return sign * (std::fabsf(v) - dz) / (1.0f - dz);
}

void UBool(vr::VRInputComponentHandle_t h, bool v)
{
    if (h) vr::VRDriverInput()->UpdateBooleanComponent(h, v, 0.0);
}

void UScalar(vr::VRInputComponentHandle_t h, float v)
{
    if (h) vr::VRDriverInput()->UpdateScalarComponent(h, static_cast<double>(v), 0.0);
}

bool GetHmdPose(Vec3 &pos, double &yaw, double &pitch)
{
    vr::TrackedDevicePose_t poses[1];
    vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0.0f, poses, 1);
    if (!poses[0].bPoseIsValid) return false;
    pos   = PosFromMatrix(poses[0].mDeviceToAbsoluteTracking);
    yaw   = YawFromMatrix(poses[0].mDeviceToAbsoluteTracking);
    pitch = PitchFromMatrix(poses[0].mDeviceToAbsoluteTracking);
    return true;
}

} // namespace

// ---- Construction / Activation ----

TouchDevice::TouchDevice(vr::ETrackedControllerRole role)
    : role_(role)
    , serial_(role == vr::TrackedControllerRole_LeftHand ? "ds4vr-left" : "ds4vr-right")
    , registered_type_(role == vr::TrackedControllerRole_LeftHand
                           ? "ds4vr/ds4vr_left" : "ds4vr/ds4vr_right")
{}

vr::EVRInitError TouchDevice::Activate(uint32_t object_id)
{
    object_id_ = object_id;
    {
        const char *rn = role_ == vr::TrackedControllerRole_LeftHand ? "left" : "right";
        char buf[128];
        std::snprintf(buf, sizeof(buf), "ds4vr: TouchDevice::Activate(%s) object_id=%u\n", rn, object_id_);
        vr::VRDriverLog()->Log(buf);
    }

    auto props = vr::VRProperties()->TrackedDeviceToPropertyContainer(object_id_);
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ControllerType_String, "oculus_touch");
    vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, role_);
    vr::VRProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{ds4vr}/input/touch_profile.json");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String,
        role_ == vr::TrackedControllerRole_LeftHand ? "oculus_cv1_controller_left" : "oculus_cv1_controller_right");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ManufacturerName_String, "ds4vr");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "DS4 Emulated Touch");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_SerialNumber_String, serial_.c_str());
    vr::VRProperties()->SetStringProperty(props, vr::Prop_RegisteredDeviceType_String, registered_type_.c_str());
    vr::VRProperties()->SetBoolProperty(props, vr::Prop_DeviceIsWireless_Bool, false);

    CreateInputComponents(props);
    last_frame_time_ = std::chrono::steady_clock::now();
    reach_ = Cfg().reach_default;
    return vr::VRInitError_None;
}

void TouchDevice::CreateInputComponents(vr::PropertyContainerHandle_t props)
{
    auto B = [&](const char *p, vr::VRInputComponentHandle_t *h) {
        vr::VRDriverInput()->CreateBooleanComponent(props, p, h); };
    auto S1 = [&](const char *p, vr::VRInputComponentHandle_t *h) {
        vr::VRDriverInput()->CreateScalarComponent(props, p, h,
            vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided); };
    auto S2 = [&](const char *p, vr::VRInputComponentHandle_t *h) {
        vr::VRDriverInput()->CreateScalarComponent(props, p, h,
            vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided); };

    B ("/input/system/click",   &in_.system_click);
    S1("/input/trigger/value",  &in_.trigger_value);
    B ("/input/trigger/click",  &in_.trigger_click);
    B ("/input/trigger/touch",  &in_.trigger_touch);
    S1("/input/grip/value",     &in_.grip_value);
    B ("/input/grip/click",     &in_.grip_click);
    S2("/input/joystick/x",     &in_.joystick_x);
    S2("/input/joystick/y",     &in_.joystick_y);
    B ("/input/joystick/click", &in_.joystick_click);
    B ("/input/joystick/touch", &in_.joystick_touch);
    B ("/input/thumbrest/touch",&in_.thumbrest_touch);

    if (role_ == vr::TrackedControllerRole_LeftHand) {
        B("/input/x/click", &in_.x_click); B("/input/x/touch", &in_.x_touch);
        B("/input/y/click", &in_.y_click); B("/input/y/touch", &in_.y_touch);
    } else {
        B("/input/a/click", &in_.a_click); B("/input/a/touch", &in_.a_touch);
        B("/input/b/click", &in_.b_click); B("/input/b/touch", &in_.b_touch);
    }
    vr::VRDriverInput()->CreateHapticComponent(props, "/output/haptic", &in_.haptic);
}

void TouchDevice::Deactivate() { object_id_ = vr::k_unTrackedDeviceIndexInvalid; }
void *TouchDevice::GetComponent(const char *) { return nullptr; }
void TouchDevice::DebugRequest(const char *, char *b, uint32_t sz) { if (b && sz) b[0] = '\0'; }

vr::DriverPose_t TouchDevice::GetPose()
{
    vr::DriverPose_t p{};
    p.poseIsValid = true;  p.deviceIsConnected = true;
    p.result = vr::TrackingResult_Running_OK;
    p.qWorldFromDriverRotation = kQuatIdentity;
    p.qDriverFromHeadRotation  = kQuatIdentity;
    p.vecPosition[0] = hand_pos_.x;
    p.vecPosition[1] = hand_pos_.y;
    p.vecPosition[2] = hand_pos_.z;
    p.qRotation      = ToHmd(hand_rot_);
    return p;
}

// ---- Arm model (§7.5) ----

void TouchDevice::ComputeArmModel(const Quat &q_ctrl, double reach,
                                  const Vec3 &hmd_pos, double hmd_yaw,
                                  double hmd_pitch)
{
    const auto &c = Cfg();
    const bool left = (role_ == vr::TrackedControllerRole_LeftHand);
    const Quat r_yaw = QuatFromYaw(hmd_yaw);

    const float *sh = left ? c.shoulder_left : c.shoulder_right;
    const Vec3 shoulder = Vec3Add(hmd_pos, QuatRotateVec(r_yaw, {sh[0], sh[1], sh[2]}));

    const Vec3 fwd = QuatRotateVec(q_ctrl, {0.0, 0.0, -1.0});
    const double ctrl_pitch = std::asin(std::max(-1.0, std::min(1.0, fwd.y)));

    const double lo = c.pitch_lo_deg * kPi / 180.0;
    const double hi = c.pitch_hi_deg * kPi / 180.0;
    const double lift = Clamp01((ctrl_pitch - lo) / (hi - lo));

    Vec3 elbow = Vec3Add(shoulder, QuatRotateVec(r_yaw, kElbowRestOffset));
    elbow.y += lift * c.elbow_lift_max;

    const Vec3 aim_pt = Vec3Add(elbow, Vec3Scale(fwd, c.forearm_nominal));

    const double reach_t = Clamp01(
        (reach - c.reach_min) / (c.reach_max - c.reach_min));
    Vec3 dir = Vec3Normalize(Vec3Sub(aim_pt, shoulder));

    // --- HMD pitch influence (fix #2): tilt the arm direction toward where
    // the user is looking, scaled by reach distance. At rest (close) the
    // arms ignore head pitch; at full extension they follow it partially.
    const double pitch_tilt = reach_t * hmd_pitch * c.hmd_pitch_influence;
    dir.y += std::sin(pitch_tilt);
    dir = Vec3Normalize(dir);

    hand_pos_ = Vec3Add(hmd_pos, Vec3Scale(dir, reach));
    hand_rot_ = q_ctrl;
}

// ---- Configurable button mapping ----

void TouchDevice::UpdateInputs(const InputState &s, bool is_aiming, bool rotation_only)
{
    const auto &c = Cfg();
    const bool left = (role_ == vr::TrackedControllerRole_LeftHand);

    // --- Sticks ---
    float jx = left ? s.left_stick_x  : s.right_stick_x;
    float jy = left ? s.left_stick_y  : s.right_stick_y;
    ApplyRadialDeadzone(jx, jy, c.stick_deadzone);
    if (is_aiming) { jx = 0.0f; jy = 0.0f; }

    UScalar(in_.joystick_x, jx);
    UScalar(in_.joystick_y, jy);
    UBool  (in_.joystick_click, (left ? s.l3 : s.r3) && !rotation_only);
    UBool  (in_.joystick_touch, std::hypotf(jx, jy) > 0.10f);

    // --- Trigger ---
    const float trig = left ? s.l2 : s.r2;
    UScalar(in_.trigger_value, trig);
    UBool  (in_.trigger_click, trig >= c.trigger_click_threshold);
    UBool  (in_.trigger_touch, trig > 0.05f);

    // --- Configurable face/dpad buttons ---
    // Each DS4 button maps to a ButtonTarget via the ini.
    struct BtnSrc { bool pressed; ButtonTarget target; };

    BtnSrc sources[4];
    if (left) {
        sources[0] = { s.dpad_up,    c.left_dpad_up    };
        sources[1] = { s.dpad_left,  c.left_dpad_left  };
        sources[2] = { s.dpad_down,  c.left_dpad_down  };
        sources[3] = { s.dpad_right, c.left_dpad_right };
    } else {
        sources[0] = { s.cross,    c.right_cross    };
        sources[1] = { s.circle,   c.right_circle   };
        sources[2] = { s.square,   c.right_square   };
        sources[3] = { s.triangle, c.right_triangle };
    }

    bool face_upper = false, face_lower = false, grip = false, system_mapped = false;
    for (const auto &btn : sources) {
        if (!btn.pressed) continue;
        switch (btn.target) {
            case ButtonTarget::FaceUpper:  face_upper = true; break;
            case ButtonTarget::FaceLower:  face_lower = true; break;
            case ButtonTarget::Grip:       grip = true;       break;
            case ButtonTarget::System:     system_mapped = true; break;
            case ButtonTarget::Unbound:    break;
        }
    }

    // System: merged from mapped buttons + Share/Options default.
    const bool sys_default = left ? s.share : s.options;
    UBool(in_.system_click, sys_default || system_mapped);

    // Grip toggle: flip state on rising edge; hold mode passes through directly.
    if (c.grip_toggle) {
        if (grip && !prev_grip_raw_)
            grip_toggle_state_ = !grip_toggle_state_;
        prev_grip_raw_ = grip;
        grip = grip_toggle_state_;
    } else {
        prev_grip_raw_     = grip;
        grip_toggle_state_ = false;
    }

    UBool  (in_.grip_click, grip);
    UScalar(in_.grip_value, grip ? 1.0f : 0.0f);

    if (left) {
        UBool(in_.y_click, face_upper); UBool(in_.y_touch, face_upper);
        UBool(in_.x_click, face_lower); UBool(in_.x_touch, face_lower);
    } else {
        UBool(in_.b_click, face_upper); UBool(in_.b_touch, face_upper);
        UBool(in_.a_click, face_lower); UBool(in_.a_touch, face_lower);
    }
    UBool(in_.thumbrest_touch, false);
}

// ---- Pose: reach + arm model + blend ----

void TouchDevice::UpdatePose(const InputState &s, const HandPoseState &hand, float dt, bool rotation_only)
{
    const auto &c = Cfg();
    Vec3 hmd_pos;  double hmd_yaw = 0.0;  double hmd_pitch = 0.0;
    if (!GetHmdPose(hmd_pos, hmd_yaw, hmd_pitch)) { hmd_pos = {0,1.6,0}; }

    const bool left = (role_ == vr::TrackedControllerRole_LeftHand);
    const Quat r_yaw = QuatFromYaw(hmd_yaw);

    // --- Reach integration (§7.5.1) ---
    if (hand.is_aiming && !rotation_only) {
        float sy = left ? s.left_stick_y : s.right_stick_y;
        sy = DeadzoneScalar(sy, c.reach_deadzone);
        if (c.reach_invert) sy = -sy;
        reach_ += c.reach_rate * sy * dt;
        reach_  = std::max(c.reach_min, std::min(c.reach_max, reach_));
    }

    // --- q_ctrl in world space ---
    Quat q_ctrl;
    double reach;
    if (hand.at_rest) {
        const double half_out = c.rest_outward_deg * kPi / 360.0;
        const Quat q_out = { std::cos(half_out), 0.0,
                             (left ? 1.0 : -1.0) * std::sin(half_out), 0.0 };
        q_ctrl = QuatMul(r_yaw, QuatMul(kNeutral, q_out));
        reach  = c.reach_rest;
    } else {
        q_ctrl = hand.orientation;
        reach  = reach_;
    }

    // Capture pre-update pose for blend source.
    Vec3 prev_pos = hand_pos_;
    Quat prev_rot = hand_rot_;

    // Arm model always runs on the live/raw orientation; hand_pos_/hand_rot_
    // hold the "natural" tracked pose afterward (hand_rot_ == q_ctrl).
    ComputeArmModel(q_ctrl, reach, hmd_pos, hmd_yaw, hmd_pitch);

    if (rotation_only) {
        if (!prev_rotation_only_) {
            // Entering: pin position here; remember entry orientation for
            // wrist accumulation below.
            ro_frozen_pos_    = prev_pos;
            q_ctrl_ro_entry_  = q_ctrl;
            q_wrist_ro_entry_ = q_wrist_;
        }
        const Quat q_delta = QuatMul(QuatConj(q_ctrl_ro_entry_), q_ctrl);
        q_wrist_ = QuatMul(q_wrist_ro_entry_, q_delta);
        hand_pos_ = ro_frozen_pos_;  // pinned for the whole hold
    } else {
        if (prev_rotation_only_) {
            // Just released: bake the held position into a persistent offset
            // so tracking continues from here with zero discontinuity.
            pos_offset_ = Vec3Sub(ro_frozen_pos_, hand_pos_);
        }
        hand_pos_ = Vec3Add(hand_pos_, pos_offset_);
    }
    hand_rot_ = QuatMul(q_ctrl, q_wrist_);
    prev_rotation_only_ = rotation_only;

    Vec3 target_pos = hand_pos_;
    Quat target_rot = hand_rot_;

    // --- Blend on arm-reset transition (§7.6) ---
    if (hand.at_rest && !was_at_rest_) {
        blending_       = true;
        blend_t_        = 0.0f;
        blend_from_pos_ = prev_pos;
        blend_from_rot_ = prev_rot;
        reach_          = c.reach_default;
        pos_offset_     = {};                    // full reset: no residual arm drift
        q_wrist_        = Quat{1.0, 0.0, 0.0, 0.0};  // full reset: no residual wrist twist
    }
    if (blending_) {
        blend_t_ += dt / (c.pose_blend_ms * 0.001f);
        if (blend_t_ >= 1.0f) { blending_ = false; blend_t_ = 1.0f; }
        double t = blend_t_;
        t = t * t * (3.0 - 2.0 * t);
        hand_pos_ = Vec3Lerp(blend_from_pos_, target_pos, t);
        hand_rot_ = QuatSlerp(blend_from_rot_, target_rot, t);
    }
    if (!hand.at_rest) blending_ = false;
    was_at_rest_ = hand.at_rest;
}

void TouchDevice::RunFrame(const InputState &snapshot, const HandPoseState &hand)
{
    if (object_id_ == vr::k_unTrackedDeviceIndexInvalid) return;

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.011f;

    const bool left = (role_ == vr::TrackedControllerRole_LeftHand);
    const bool stick = left ? snapshot.l3 : snapshot.r3;
    const bool rotation_only = hand.is_aiming && stick;

    // Stick double-click while aiming resets the wrist offset to identity.
    if (hand.is_aiming && stick && !stick_was_pressed_) {
        const double t = std::chrono::duration<double>(now.time_since_epoch()).count();
        const double gap = stick_last_press_ < 0.0 ? -1.0 : t - stick_last_press_;
        if (gap > 0.0 && gap < Cfg().t_dtap_ms * 0.001)
            q_wrist_ = Quat{1.0, 0.0, 0.0, 0.0};
        stick_prev_press_ = stick_last_press_;
        stick_last_press_ = t;
    }
    stick_was_pressed_ = stick && hand.is_aiming;

    UpdateInputs(snapshot, hand.is_aiming, rotation_only);
    UpdatePose(snapshot, hand, dt, rotation_only);
    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(object_id_, GetPose(), sizeof(vr::DriverPose_t));
}

} // namespace ds4vr
