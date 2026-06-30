#pragma once

#include <openvr_driver.h>

#include <memory>

#include "engagement.h"

namespace ds4vr {

class TouchDevice;
class HidDevice;

class ServerDriver final : public vr::IServerTrackedDeviceProvider {
public:
    ServerDriver();
    ~ServerDriver();

    vr::EVRInitError Init(vr::IVRDriverContext *driver_context) override;
    void             Cleanup() override;
    const char *const *GetInterfaceVersions() override { return vr::k_InterfaceVersions; }
    void             RunFrame() override;
    bool             ShouldBlockStandbyMode() override { return false; }
    void             EnterStandby() override {}
    void             LeaveStandby() override {}

private:
    std::unique_ptr<TouchDevice> left_;
    std::unique_ptr<TouchDevice> right_;
    std::unique_ptr<HidDevice>   hid_;
    EngagementFSM                fsm_;
    uint8_t                      rumble_large_ = 0;
    uint8_t                      rumble_small_ = 0;
};

} // namespace ds4vr
