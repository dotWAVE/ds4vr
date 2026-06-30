#include "hid_device.h"

#include <openvr_driver.h>
#include <hidapi.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "ds4_parser.h"
#include "dualsense_parser.h"
#include "imu_fusion.h"

namespace ds4vr {

namespace {

constexpr uint16_t kSonyVid = 0x054C;

struct PidEntry {
    uint16_t       pid;
    ControllerType type;
};

constexpr std::array<PidEntry, 5> kPids = {{
    { 0x0CE6, ControllerType::DualSense },   // DualSense
    { 0x0DF2, ControllerType::DualSense },   // DualSense Edge
    { 0x05C4, ControllerType::DS4 },          // DS4 1st gen
    { 0x09CC, ControllerType::DS4 },          // DS4 2nd gen
    { 0x0BA0, ControllerType::DS4 },          // Sony Wireless USB Adaptor
}};

constexpr int  kReadTimeoutMs   = 100;
constexpr auto kReopenBackoff   = std::chrono::milliseconds(750);

const char *TypeName(ControllerType t) {
    switch (t) {
        case ControllerType::DS4:       return "DS4";
        case ControllerType::DualSense: return "DualSense";
        default:                        return "Unknown";
    }
}

void Log(const char *msg) {
    if (vr::VRDriverLog()) {
        vr::VRDriverLog()->Log(msg);
    }
}

} // namespace

HidDevice::HidDevice() = default;

HidDevice::~HidDevice() { Stop(); }

void HidDevice::Start()
{
    if (running_.exchange(true)) return;

    if (hid_init() != 0) {
        Log("ds4vr: hid_init failed\n");
        running_.store(false);
        return;
    }
    thread_ = std::thread(&HidDevice::ReadLoop, this);
}

void HidDevice::Stop()
{
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    hid_exit();
}

InputState HidDevice::GetSnapshot() const
{
    std::lock_guard<std::mutex> lk(mu_);
    return latest_;
}

HidDevice::OpenResult HidDevice::TryOpen() const
{
    for (const auto &entry : kPids) {
        if (hid_device_ *h = hid_open(kSonyVid, entry.pid, nullptr)) {
            char buf[96];
            std::snprintf(buf, sizeof(buf),
                          "ds4vr: opened %s vid=0x%04x pid=0x%04x\n",
                          TypeName(entry.type), kSonyVid, entry.pid);
            Log(buf);
            return { h, entry.type };
        }
    }
    return {};
}

void HidDevice::ReadLoop()
{
    Log("ds4vr: HID read thread starting\n");

    std::array<uint8_t, 64> buf{};

    while (running_.load(std::memory_order_relaxed)) {

        if (handle_ == nullptr) {
            auto result = TryOpen();
            handle_     = result.handle;
            controller_ = result.type;
            if (handle_ == nullptr) {
                std::this_thread::sleep_for(kReopenBackoff);
                continue;
            }
            hid_set_nonblocking(handle_, 0);
            connected_.store(true, std::memory_order_relaxed);
            fusion_ = std::make_unique<ImuFusion>(Cfg().imu_beta);
            sent_large_ = 0; sent_small_ = 0;
            last_imu_time_ = std::chrono::steady_clock::now();
        }

        const int n = hid_read_timeout(handle_, buf.data(),
                                        static_cast<size_t>(buf.size()),
                                        kReadTimeoutMs);

        if (n < 0) {
            Log("ds4vr: hid_read_timeout returned <0; assuming disconnect\n");
            hid_close(handle_);
            handle_     = nullptr;
            controller_ = ControllerType::Unknown;
            fusion_.reset();
            connected_.store(false, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lk(mu_);
                latest_ = InputState{};
            }
            std::this_thread::sleep_for(kReopenBackoff);
            continue;
        }

        if (n == 0) {
            SendRumbleIfNeeded();
            continue;
        }

        InputState parsed;
        bool ok = false;
        switch (controller_) {
            case ControllerType::DS4:
                ok = ParseDs4UsbReport(buf.data(), static_cast<std::size_t>(n), parsed);
                break;
            case ControllerType::DualSense:
                ok = ParseDualSenseUsbReport(buf.data(), static_cast<std::size_t>(n), parsed);
                break;
            default:
                break;
        }

        if (ok && fusion_) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - last_imu_time_).count();
            last_imu_time_ = now;

            // Clamp dt to avoid integration blowup on the first sample or
            // after a long stall (e.g. HID timeout gap).
            if (dt <= 0.0f || dt > 0.1f) dt = 0.004f;  // assume ~250 Hz

            fusion_->Update(parsed.gyro, parsed.accel, dt);
            fusion_->GetQuat(parsed.imu_quat);

            std::lock_guard<std::mutex> lk(mu_);
            latest_ = parsed;
        }

        SendRumbleIfNeeded();
    }

    if (handle_ != nullptr) {
        hid_close(handle_);
        handle_ = nullptr;
    }
    fusion_.reset();
    controller_ = ControllerType::Unknown;
    connected_.store(false, std::memory_order_relaxed);
    Log("ds4vr: HID read thread exiting\n");
}

void HidDevice::SetRumble(uint8_t large, uint8_t small)
{
    rumble_large_.store(large, std::memory_order_relaxed);
    rumble_small_.store(small, std::memory_order_relaxed);
}

void HidDevice::SendRumbleIfNeeded()
{
    if (handle_ == nullptr) return;

    const uint8_t lg = rumble_large_.load(std::memory_order_relaxed);
    const uint8_t sm = rumble_small_.load(std::memory_order_relaxed);
    if (lg == sent_large_ && sm == sent_small_) return;

    sent_large_ = lg;
    sent_small_ = sm;

    if (controller_ == ControllerType::DS4) {
        // DS4 USB output report: ID 0x05, 32 bytes.
        // Byte 4 = small (right) motor, byte 5 = large (left) motor.
        uint8_t report[32]{};
        report[0] = 0x05;
        report[1] = 0xFF;  // enable rumble + LED
        report[4] = sm;
        report[5] = lg;
        hid_write(handle_, report, sizeof(report));
    } else if (controller_ == ControllerType::DualSense) {
        // DualSense USB output report: ID 0x02, 48 bytes.
        // Byte 1 flags = 0x03 (enable rumble), byte 3 = small, byte 4 = large.
        uint8_t report[48]{};
        report[0] = 0x02;
        report[1] = 0x03;  // motor enable flags
        report[3] = sm;
        report[4] = lg;
        hid_write(handle_, report, sizeof(report));
    }
}

} // namespace ds4vr
