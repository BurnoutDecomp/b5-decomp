#pragma once

// BrnAI::AIAggression -- the rival-AI aggression behaviour controller. It drives the
// state machine that decides when an AI racer overtakes, slams, drops back, veers, etc.
// against its target/player car, and caches the world-space target position used by the
// racing-line generator.
//
// Layout (member order/types/offsets) recovered from the DecFIGS DWARF
// (BrnAIAggression.h). Member offsets are confirmed against the X360 build
// (mTargetPos @ +0x30, mbTargetPosValid @ +0x44, observed in
// AIAggression::GetTargetPos @0x827656D0). The cross-referenced AIDriver/AICar
// types live in TUs not yet reconstructed, so they are forward-declared and only
// referenced by pointer here -- this home owns the AIAggression layout, not those
// types. Only GetTargetPos is defined by this TU; the remaining members exist solely
// to place mTargetPos/mbTargetPosValid at their true offsets.

#include "BrnCommonTypes.h"   // Vector3 (rw::math::vpu::Vector3, 16-byte SIMD)

namespace CgsNumeric { class Random; }

namespace BrnAI
{
    // DWARF BrnAIAggression.h:31 -- speed-matching mode used while shadowing a target car.
    enum ESpeedMatch
    {
        ESpeedMatch_Disabled      = 0,
        ESpeedMatch_Enabled       = 1,
        ESpeedMatch_Slower        = 2,
        ESpeedMatch_SlowToClip    = 3,
        ESpeedMatch_OvertakeFast  = 4,
        ESpeedMatch_OvertakeSlowly = 5,
        ESpeedMatch_Count         = 6,
    };

    // DWARF BrnAIAggression.h:43 -- the aggression state machine's current state.
    enum EAIAggressionState
    {
        E_AI_AGGRESSION_STATE_OUT_OF_RANGE      = 0,
        E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM  = 1,
        E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM = 2,
        E_AI_AGGRESSION_STATE_ATTACK_SLAM       = 3,
        E_AI_AGGRESSION_STATE_WAIT              = 4,
        E_AI_AGGRESSION_STATE_VEER              = 5,
        E_AI_AGGRESSION_STATE_PASSIVE           = 6,
        E_AI_AGGRESSION_STATE_FALL_PAST         = 7,
        E_AI_AGGRESSION_STATE_BE_FODDER         = 8,
        E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND   = 9,
        E_AI_AGGRESSION_STATE_OVERTAKE_FAST     = 10,
        E_AI_AGGRESSION_STATE_OVERTAKE_SLOWLY   = 11,
        E_AI_AGGRESSION_STATE_SPURT_FORWARD     = 12,
        E_AI_AGGRESSION_STATE_VEER_EXTREME      = 13,
        E_AI_AGGRESSION_STATE_HANG_AROUND_AHEAD = 14,
        E_AI_AGGRESSION_STATE_COUNT             = 15,
    };

    // DWARF BrnAIAggression.h:64 -- post-attack disengage behaviour.
    enum EStopAttack
    {
        E_AGGRESSION_STEERAWAY      = 0,
        E_AGGRESSION_DELAYEDATTACK  = 1,
        E_AGGRESSION_ATTACKAGAIN    = 2,
    };

    // Forward-declared collaborators (referenced by pointer only; homed by their own TUs).
    class AIDriver;
    class AICar;

    // DWARF BrnAIAggression.h:84.
    struct AIAggression
    {
        // Returns the cached world-space target position by value (X360
        // AIAggression::GetTargetPos @0x827656D0). Asserts mbTargetPosValid first, then
        // copies mTargetPos into the (ABI-hidden) return slot -- the X360 build does a
        // single 16-byte lvx128/stvx128 vector move from this+0x30. Called by
        // AIDriver::GenerateRacingLine.
        Vector3 GetTargetPos();

    private:
        // mRandom is a STATIC member in the DWARF (extern/shared across instances), so it
        // contributes nothing to the per-instance layout below.
        static CgsNumeric::Random mRandom;                  // DWARF :370 (static)

        EAIAggressionState meAggressionState;               // +0x00
        AIDriver*          mpDriver;                         // +0x04
        AICar*             mpCar;                            // +0x08
        const AICar*       mpTargetCar;                      // +0x0C
        const AICar*       mpPlayerCar;                      // +0x10
        f32                mfStateTime;                      // +0x14
        AICar*             mpPrevSpeedMatchTarget;           // +0x18
        f32                mfLastSpeed;                      // +0x1C
        f32                mfLastSpeedTarget;                // +0x20
        Vector3            mTargetPos;                       // +0x30 (16-byte aligned)
        f32                mfTargetSeparationAlong;          // +0x40
        bool               mbTargetPosValid;                 // +0x44
        f32                mFixedPassingSpeed;               // +0x48
        f32                mfRecentHitTimer;                 // +0x4C
        f32                mfContinuousContactTimer;         // +0x50
        bool               mbWantToBoost;                    // +0x54
        ESpeedMatch        meSpeedMatchType;                 // +0x58
        f32                mfRelativePositionAhead;          // +0x5C
        bool               mbIsSuitableForAggression;        // +0x60
        f32                mfNonSpeedMatchedSpeed;           // +0x64
        f32                mfHangingAroundTimer;             // +0x68
    };
}
