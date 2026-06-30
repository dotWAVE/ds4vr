#pragma once

#include <openvr_driver.h>

#include <chrono>
#include <string>

#include "engagement.h"
#include "input_state.h"
#include "math_types.h"

namespace ds4vr {

class TouchDevice final : public vr::ITrackedDeviceServerDriver {
public:
    explicit TouchDevice(vr::ETrackedControllerRole role);

    // vr::ITrackedDeviceServerDriver
    vr::EVRInitError Activate(uint32_t object_id) override;
    void             Deactivate() override;
    void             EnterStandby() override {}
    void *           GetComponent(const char *component_name_and_version) override;
    void             DebugRequest(const char *request,
                                  char *response_buffer,
                                  uint32_t response_buffer_size) override;
    vr::DriverPose_t GetPose() override;

    void RunFrame(const InputState &snapshot, const HandPoseState &hand);

    const char *GetSerialNumber() const { return serial_.c_str(); }
    uint32_t    GetObjectId()    const { return object_id_; }

private:
    void CreateInputComponents(vr::PropertyContainerHandle_t props);
    void UpdateInputs(const InputState &s, bool is_aiming, bool rotation_only);
    void UpdatePose(const InputState &s, const HandPoseState &hand, float dt, bool rotation_only);

    // Arm model: orientation + reach → world-space hand position.
    void ComputeArmModel(const Quat &q_ctrl_world, double reach,
                         const Vec3 &hmd_pos, double hmd_yaw,
                         double hmd_pitch);

    vr::ETrackedControllerRole role_;
    std::string serial_;
    std::string registered_type_;
    uint32_t    object_id_ = vr::k_unTrackedDeviceIndexInvalid;

    // Current computed pose.
    Vec3 hand_pos_{};
    Quat hand_rot_{};

    // Reach state (§7.5.1) — persists across frames.
    float reach_ = 0.45f;  // reach_default from §9 [reach]

    // Blend state for arm-reset transition (§7.6).
    bool was_at_rest_     = true;
    bool blending_        = false;
    float blend_t_        = 0.0f;
    Vec3 blend_from_pos_{};
    Quat blend_from_rot_{};

    // Grip toggle state.
    bool prev_grip_raw_    = false;
    bool grip_toggle_state_= false;

    // Position offset (decouples hand_pos_ from arm-model tracking while the
    // rotation-only modifier is held, and absorbs the resulting drift after
    // release so position never jumps).
    Vec3  pos_offset_     {};  // persistent; added on top of arm-model position
    Vec3  ro_frozen_pos_  {};  // hand_pos_ captured at rotation-only entry; held during the hold

    // Wrist rotation offset (decoupled from arm direction via rotation_only modifier).
    Quat  q_wrist_            {1.0, 0.0, 0.0, 0.0};
    bool  prev_rotation_only_ = false;
    Quat  q_ctrl_ro_entry_    {1.0, 0.0, 0.0, 0.0};
    Quat  q_wrist_ro_entry_   {1.0, 0.0, 0.0, 0.0};
    // Stick double-click timing (resets wrist offset while aiming).
    double stick_last_press_  = -1000.0;
    double stick_prev_press_  = -1000.0;
    bool   stick_was_pressed_ = false;

    // Frame timing.
    std::chrono::steady_clock::time_point last_frame_time_{};

    struct {
        vr::VRInputComponentHandle_t system_click   = 0;
        vr::VRInputComponentHandle_t trigger_value  = 0;
        vr::VRInputComponentHandle_t trigger_click  = 0;
        vr::VRInputComponentHandle_t trigger_touch  = 0;
        vr::VRInputComponentHandle_t grip_value     = 0;
        vr::VRInputComponentHandle_t grip_click     = 0;
        vr::VRInputComponentHandle_t joystick_x     = 0;
        vr::VRInputComponentHandle_t joystick_y     = 0;
        vr::VRInputComponentHandle_t joystick_click = 0;
        vr::VRInputComponentHandle_t joystick_touch = 0;
        vr::VRInputComponentHandle_t thumbrest_touch= 0;
        vr::VRInputComponentHandle_t x_click = 0;
        vr::VRInputComponentHandle_t x_touch = 0;
        vr::VRInputComponentHandle_t y_click = 0;
        vr::VRInputComponentHandle_t y_touch = 0;
        vr::VRInputComponentHandle_t a_click = 0;
        vr::VRInputComponentHandle_t a_touch = 0;
        vr::VRInputComponentHandle_t b_click = 0;
        vr::VRInputComponentHandle_t b_touch = 0;
        vr::VRInputComponentHandle_t haptic  = 0;
    } in_;
};

} // namespace ds4vr
