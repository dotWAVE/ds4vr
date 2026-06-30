#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include "input_state.h"

struct hid_device_;  // forward decl from hidapi

namespace ds4vr {

class ImuFusion;  // forward decl

enum class ControllerType : uint8_t {
    Unknown,
    DS4,
    DualSense,
};

// Owns the controller HID handle and its read thread. Supports DS4 and
// DualSense over USB; dispatches to the appropriate parser based on which
// device opened. Bluetooth is deferred.
//
// Lifecycle:
//   ServerDriver::Init    → Start()
//   ServerDriver::RunFrame→ GetSnapshot()
//   ServerDriver::Cleanup → Stop()
//
// The read thread loops forever while running_ is set: opens the device on
// demand, reads with a 100 ms timeout, parses, publishes. On read error it
// closes the handle and retries, satisfying spec §13's "unplug/replug
// mid-session, recover without restarting SteamVR".
class HidDevice {
public:
    HidDevice();
    ~HidDevice();

    HidDevice(const HidDevice &)            = delete;
    HidDevice &operator=(const HidDevice &) = delete;

    void Start();
    void Stop();

    InputState GetSnapshot() const;
    bool       IsConnected() const { return connected_.load(std::memory_order_relaxed); }

    // Set desired rumble motor strengths (0–255). Thread-safe; the read
    // thread coalesces and writes the output report periodically.
    void SetRumble(uint8_t large_motor, uint8_t small_motor);

private:
    struct OpenResult {
        hid_device_   *handle = nullptr;
        ControllerType type   = ControllerType::Unknown;
    };

    void        ReadLoop();
    OpenResult  TryOpen() const;

    mutable std::mutex mu_;
    InputState         latest_;

    std::atomic<bool> running_  {false};
    std::atomic<bool> connected_{false};

    std::thread      thread_;
    hid_device_     *handle_     = nullptr;  // touched only on the read thread
    ControllerType   controller_ = ControllerType::Unknown;

    // IMU fusion — owned by the read thread.
    std::unique_ptr<ImuFusion> fusion_;
    std::chrono::steady_clock::time_point last_imu_time_{};

    // Rumble — written by main thread via SetRumble, drained by read thread.
    std::atomic<uint8_t> rumble_large_{0};
    std::atomic<uint8_t> rumble_small_{0};
    uint8_t              sent_large_ = 0;  // last values written to device
    uint8_t              sent_small_ = 0;
    void                 SendRumbleIfNeeded();
};

} // namespace ds4vr
