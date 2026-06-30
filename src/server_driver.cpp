#include "server_driver.h"

#include <algorithm>
#include <cstdint>

#include "config.h"
#include "hid_device.h"
#include "touch_device.h"

namespace ds4vr {

ServerDriver::ServerDriver()  = default;
ServerDriver::~ServerDriver() = default;

vr::EVRInitError ServerDriver::Init(vr::IVRDriverContext *driver_context)
{
    VR_INIT_SERVER_DRIVER_CONTEXT(driver_context);

    vr::VRDriverLog()->Log("ds4vr: ServerDriver::Init starting\n");

    LoadConfig(GetIniPath());

    hid_ = std::make_unique<HidDevice>();
    hid_->Start();

    left_  = std::make_unique<TouchDevice>(vr::TrackedControllerRole_LeftHand);
    right_ = std::make_unique<TouchDevice>(vr::TrackedControllerRole_RightHand);

    const bool added_left = vr::VRServerDriverHost()->TrackedDeviceAdded(
        left_->GetSerialNumber(),
        vr::TrackedDeviceClass_Controller,
        left_.get());

    const bool added_right = vr::VRServerDriverHost()->TrackedDeviceAdded(
        right_->GetSerialNumber(),
        vr::TrackedDeviceClass_Controller,
        right_.get());

    vr::VRDriverLog()->Log(added_left
        ? "ds4vr: TrackedDeviceAdded(left) ok\n"
        : "ds4vr: TrackedDeviceAdded(left) FAILED\n");
    vr::VRDriverLog()->Log(added_right
        ? "ds4vr: TrackedDeviceAdded(right) ok\n"
        : "ds4vr: TrackedDeviceAdded(right) FAILED\n");

    return vr::VRInitError_None;
}

void ServerDriver::Cleanup()
{
    if (vr::VRDriverLog()) {
        vr::VRDriverLog()->Log("ds4vr: ServerDriver::Cleanup\n");
    }
    if (hid_) hid_->Stop();
    left_.reset();
    right_.reset();
    hid_.reset();
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

void ServerDriver::RunFrame()
{
    if (!hid_) return;
    const InputState snapshot = hid_->GetSnapshot();

    // --- Engagement FSM ---
    const Quat q_phys = QuatFromFloats(snapshot.imu_quat);
    fsm_.Update(snapshot.l1, snapshot.r1, q_phys);

    // --- Touch device updates ---
    if (left_)  left_->RunFrame(snapshot, fsm_.Left());
    if (right_) right_->RunFrame(snapshot, fsm_.Right());

    // --- Haptic events → rumble (§8) ---
    vr::VREvent_t ev;
    while (vr::VRServerDriverHost()->PollNextEvent(&ev, sizeof(ev))) {
        if (ev.eventType == vr::VREvent_Input_HapticVibration) {
            const float amp = ev.data.hapticVibration.fAmplitude;
            const uint8_t val = static_cast<uint8_t>(
                std::min(255.0f, std::max(0.0f, amp * 255.0f)));

            const auto &cfg = Cfg();
            const uint32_t left_id  = left_  ? left_->GetObjectId()  : UINT32_MAX;
            const uint32_t right_id = right_ ? right_->GetObjectId() : UINT32_MAX;

            if (ev.trackedDeviceIndex == left_id) {
                if (cfg.haptic_left_large)
                    rumble_large_ = val;
                else
                    rumble_small_ = val;
            } else if (ev.trackedDeviceIndex == right_id) {
                if (cfg.haptic_right_large)
                    rumble_large_ = val;
                else
                    rumble_small_ = val;
            }
        }
    }
    hid_->SetRumble(rumble_large_, rumble_small_);
}

} // namespace ds4vr
