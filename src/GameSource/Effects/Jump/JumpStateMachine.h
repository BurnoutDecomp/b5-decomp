#ifndef GAMESOURCE_EFFECTS_JUMP_JUMPSTATEMACHINE_H
#define GAMESOURCE_EFFECTS_JUMP_JUMPSTATEMACHINE_H

#include "types.hpp"
#include "GameSource/Effects/EffectsStateMachine.h"   // BrnEffects::EffectsStateMachine (base), EffectsState, CarState

// ============================================================================
// GameSource/Effects/Jump/JumpStateMachine.h
//
// BrnEffects::JumpStateMachine -- the per-race-car effects state machine that
// drives the jump / landing (wheel sparks / debris / vapour) particle effects.
// It derives from BrnEffects::EffectsStateMachine and overrides the state
// virtuals (OnDetermineNextState / OnChangeState / OnTick).
//
// SOURCE-OF-TRUTH: declaration SHAPE (virtual/const, vtable order, method set and
// the mFadeTime / mbWheelsOnGround[4] member set) from the DecFIGS DWARF
// (GameSource/Effects/Jump/JumpStateMachine.h), gated on the ARTIST X360 ledger.
// SetVapourBlend behaviour verified against the ARTIST asm @ 0x82288A58; the
// remaining bodies (OnDetermineNextState, OnChangeState 0x82299510,
// FireWheelSparks 0x82299670, FireWheelDebris 0x822939D8, OnTick) are authored by
// sibling wave agents in this same TU -- GROW this header additively, do NOT fork.
//
// LAYOUT (base EffectsStateMachine occupies +0x00..+0x0B = vptr + mState + mTime):
//   +0x0C  mFadeTime            (DWARF JumpStateMachine.h:46)
//   +0x10  mbWheelsOnGround[4]  (DWARF JumpStateMachine.h:48)
// SetVapourBlend does not read `this`, so these offsets are shape-only for it.
// ============================================================================

namespace BrnEffects
{
    // Forward declarations of the types this state machine is handed by reference.
    class ParticleEffectHelper;
    class RaceCarParticleEffectHelper;
    struct CarState;

    // ------------------------------------------------------------------------
    // BrnEffects::JumpStateMachine
    // ------------------------------------------------------------------------
    class JumpStateMachine : public EffectsStateMachine
    {
    protected:
        // ---- EffectsStateMachine virtual overrides --------------------------
        // OnDetermineNextState (DWARF JumpStateMachine.cpp:47). The jump-effects
        // state transition table.
        virtual EffectsState OnDetermineNextState(CarState& lCarState,
                                                  bool lbStateTimerExpired,
                                                  EffectsState leCurrentState,
                                                  RaceCarParticleEffectHelper& lHelper);

        // OnChangeState @ 0x82299510 (DWARF JumpStateMachine.cpp:250). Entry action
        // for each jump state.
        virtual void OnChangeState(EffectsState leNewState,
                                   RaceCarParticleEffectHelper& lHelper,
                                   CarState& lCarState);

        // OnTick (DWARF JumpStateMachine.cpp:465).
        virtual void OnTick(CarState& lCarState, RaceCarParticleEffectHelper& lHelper);

    private:
        // FireWheelSparks @ 0x82299670 (DWARF JumpStateMachine.cpp:308). Fires the
        // landing wheel-spark effect from the front-wheel contact points.
        void FireWheelSparks(CarState& lCarState, RaceCarParticleEffectHelper& lHelper) const;

        // FireWheelDebris @ 0x822939D8 (DWARF JumpStateMachine.cpp:354). Spawns the
        // landing debris burst scaled by the race car velocity.
        void FireWheelDebris(CarState& lCarState, RaceCarParticleEffectHelper& lHelper) const;

        // SetVapourBlend @ 0x82288A58 (DWARF JumpStateMachine.cpp:411). Computes and
        // applies the jump-vapour LION effect's state blend from the car's upward
        // velocity alignment and the debug "Jumping" vapour delay/ramp times, honouring
        // the debug force-state-blend override. Implementation-detail rodata literals
        // (0.707/0.8535 dir thresholds, 20.0 min speed, 0x114 handle offset) live as
        // file-scope statics in the .cpp, not as class members.
        void SetVapourBlend(f32 lfFadeTime, RaceCarParticleEffectHelper& lHelper) const;

        // ---- data members (offsets relative to the JumpStateMachine object) --
        f32  mFadeTime;             // +0x0C  (DWARF JumpStateMachine.h:46)
        bool mbWheelsOnGround[4];   // +0x10  (DWARF JumpStateMachine.h:48)
    };
}

#endif // GAMESOURCE_EFFECTS_JUMP_JUMPSTATEMACHINE_H
