#pragma once

#include "math_types.h"

namespace ds4vr {

// Per-hand output from the engagement FSM.
struct HandPoseState {
    bool at_rest    = true;
    bool is_aiming  = false;  // true = this hand's bumper is sole-held right now
    Quat orientation{};       // meaningful when !at_rest
};

// Engagement state machine. Each bumper independently controls its hand.
// Holding one or both bumpers aims those hands; releasing freezes the pose.
// Double-tap resets a hand to rest. Both held = both aim simultaneously.
class EngagementFSM {
public:
    void Update(bool l1, bool r1, const Quat &q_phys);

    HandPoseState Left()  const { return output_[0]; }
    HandPoseState Right() const { return output_[1]; }

private:
    static constexpr int kLeft  = 0;
    static constexpr int kRight = 1;

    struct BumperState {
        bool   was_pressed     = false;
        double last_press_time = -1000.0;
        double prev_press_time = -1000.0;
    };

    struct PerHand {
        Quat q_base{};    // virtual base for current/next aiming session
        Quat q_zero{};    // physical reference captured at (re)zero
        Quat q_held{};    // frozen virtual orientation (identity when at_rest)
        bool at_rest = true;
    };

    void ResetHand(int hand);
    void BeginAiming(int hand, const Quat &q_phys);
    Quat ComputeVirtual(int hand, const Quat &q_phys) const;

    BumperState   bumpers_[2];
    PerHand       per_hand_[2];
    HandPoseState output_[2];

    bool was_aiming_[2] = {};  // whether each hand was actively aiming last frame
};

} // namespace ds4vr
