#ifndef GAMESOURCE_EFFECTS_EFFECTSSTATEMACHINE_H
#define GAMESOURCE_EFFECTS_EFFECTSSTATEMACHINE_H

#include "types.hpp"

// ============================================================================
// GameSource/Effects/EffectsStateMachine.h
//
// BrnEffects::EffectsStateMachine -- the shared base for the per-race-car effects
// state machines (BoostStateMachine, JumpStateMachine, ...). It owns the current
// EffectsState + a state timer, runs the generic Tick (determine-next-state ->
// change-state -> on-tick) loop, and defers the per-machine behaviour to four
// protected virtuals.
//
// SOURCE-OF-TRUTH: declaration SHAPE (the EffectsState enum, the vptr + mState +
// mTime member layout, and the OnDetermineNextState / OnChangeState / OnTick /
// OnConstruct vtable order) from the DecFIGS DWARF
// (GameSource/Effects/EffectsStateMachine.h), gated on the ARTIST ledger. The
// member offsets are pinned by the BoostStateMachine override bodies: OnConstruct
// (0x82280620) stores mState at +0x04 and mTime at +0x08, so the base occupies
// +0x00 (vptr) .. +0x0B. Bodies for the non-virtual methods live in
// EffectsStateMachine.cpp (its own TU) and are declared-only here so derived TUs
// compile against the shape -- GROW additively, do NOT fork the type.
// ============================================================================

namespace BrnEffects
{
    // Forward declarations of the types the state machine threads through.
    struct CarState;
    class RaceCarParticleEffectHelper;

    // EffectsState (DWARF EffectsStateMachine.h:38). The full effects-state set;
    // BoostStateMachine uses states 0..6, the jump machine the higher ones.
    enum EffectsState
    {
        EffectsStateAllOff               = 0,
        EffectsStateBoosting             = 1,
        EffectsStateExhaustSmokeOn       = 2,
        EffectsStateExhaustSmokeStopping = 3,
        EffectsStateBoostStopping        = 4,
        EffectsStateExhaustPopRestart    = 5,
        EffectsStateExhaustPop           = 6,
        EffectsStateJumping              = 7,
        EffectsStateJumpingMovingUp      = 8,
        EffectsStateJumpingMovingDown    = 9,
        EffectsStateLanded               = 10,
        EffectsStateFiringSparks         = 11,
        EffectsStateLandedWaiting        = 12,
        EffectsStateJumpingFinishing     = 13,
        EffectsStateWheelNoEffect        = 14,
        EffectsStateWheelSmoking         = 15,
        EffectsStateWheelBurnout         = 16,
        EffectsState_MAX                 = 17,
    };

    class EffectsStateMachine
    {
    public:
        // CurrentState (DWARF EffectsStateMachine.h:68). Own-TU body.
        EffectsState CurrentState() const { return mState; }

        // Construct / Tick / SetState (DWARF EffectsStateMachine.cpp). Own-TU bodies;
        // declared here for the shape only.
        void Construct();
        void Tick(CarState& lCarState, RaceCarParticleEffectHelper& lHelper);
        void SetState(EffectsState leState);
        void SetState(EffectsState leState, RaceCarParticleEffectHelper& lHelper, CarState& lCarState);

    protected:
        // SetStateTimer (DWARF EffectsStateMachine.cpp:25). Own-TU body.
        void SetStateTimer(f32 lfTime);

        // The four per-machine virtuals (vtable order per the DWARF). Derived
        // machines override these; the base bodies live in EffectsStateMachine.cpp.
        virtual EffectsState OnDetermineNextState(CarState& lCarState,
                                                  bool lbStateTimerExpired,
                                                  EffectsState leCurrentState,
                                                  RaceCarParticleEffectHelper& lHelper);
        virtual void OnChangeState(EffectsState leNewState,
                                   RaceCarParticleEffectHelper& lHelper,
                                   CarState& lCarState);
        virtual void OnTick(CarState& lCarState, RaceCarParticleEffectHelper& lHelper);
        virtual void OnConstruct();

        // ---- data members --------------------------------------------------
        EffectsState mState;   // +0x04  current effects state
        f32          mTime;    // +0x08  state timer (seconds in current state)
    };
}

#endif // GAMESOURCE_EFFECTS_EFFECTSSTATEMACHINE_H
