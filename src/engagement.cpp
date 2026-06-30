#include "engagement.h"

#include <chrono>

#include "config.h"

namespace ds4vr {

namespace {

double MonoSec()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

void EngagementFSM::ResetHand(int hand)
{
    per_hand_[hand].q_held  = Quat{};   // identity = neutral → resting pose
    per_hand_[hand].at_rest = true;
}

void EngagementFSM::BeginAiming(int hand, const Quat &q_phys)
{
    per_hand_[hand].q_base  = per_hand_[hand].q_held;
    per_hand_[hand].q_zero  = q_phys;
    per_hand_[hand].at_rest = false;
}

Quat EngagementFSM::ComputeVirtual(int hand, const Quat &q_phys) const
{
    // q_virtual = q_base ⊗ (q_zero⁻¹ ⊗ q_phys)   — spec §7.3
    const Quat delta = QuatMul(QuatConj(per_hand_[hand].q_zero), q_phys);
    return QuatMul(per_hand_[hand].q_base, delta);
}

void EngagementFSM::Update(bool l1, bool r1, const Quat &q_phys)
{
    const double now = MonoSec();
    const bool pressed[2] = { l1, r1 };
    bool dtap[2] = { false, false };

    // ---- Edge detection & double-tap ----
    for (int i = 0; i < 2; i++) {
        if (pressed[i] && !bumpers_[i].was_pressed) {
            // Rising edge: record press time, shift history.
            bumpers_[i].prev_press_time = bumpers_[i].last_press_time;
            bumpers_[i].last_press_time = now;
        }
        if (!pressed[i] && bumpers_[i].was_pressed) {
            // Falling edge: check for double-tap.
            // "Two presses within T_dtap, second released quickly" (§7.4).
            const double t_dtap = Cfg().t_dtap_ms * 0.001;
            const double hold = now - bumpers_[i].last_press_time;
            const double gap  = bumpers_[i].last_press_time - bumpers_[i].prev_press_time;
            if (hold < t_dtap && gap > 0.0 && gap < t_dtap) {
                dtap[i] = true;
                // Clear history so subsequent taps start fresh.
                bumpers_[i].prev_press_time = -1000.0;
            }
        }
        bumpers_[i].was_pressed = pressed[i];
    }

    // ---- Per-hand aiming ----
    // Each bumper independently drives its hand. BeginAiming fires on the
    // frame a hand newly becomes pressed (rising edge or rejoining after clutch).
    for (int i = 0; i < 2; i++) {
        if (pressed[i]) {
            if (!was_aiming_[i])
                BeginAiming(i, q_phys);
            per_hand_[i].q_held  = ComputeVirtual(i, q_phys);
            per_hand_[i].at_rest = false;
            was_aiming_[i] = true;
        } else {
            was_aiming_[i] = false;
            // Released: hand keeps its last q_held (pose frozen until next press).
        }
    }

    // ---- Double-tap resets ----
    if (dtap[kLeft])  ResetHand(kLeft);
    if (dtap[kRight]) ResetHand(kRight);

    // ---- Publish ----
    for (int i = 0; i < 2; i++) {
        output_[i].at_rest     = per_hand_[i].at_rest;
        output_[i].is_aiming   = pressed[i];
        output_[i].orientation = per_hand_[i].q_held;
    }
}

} // namespace ds4vr
