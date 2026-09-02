#include "GameSource/Effects/EffectsStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"          // BrnEffects::CarState (+0x10 mDt, +0x4E mbJumping)
#include "GameSource/Effects/BrnEffectsDebugComponent.h" // EffectsDebugComponent::JumpParams().IsForceJumping()
#include "GameSource/Effects/ParticleEffectHelper.h"   // RaceCarParticleEffectHelper::DebugComponent()
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT

// BrnEffects::EffectsStateMachine out-of-line bodies, reconstructed from BURNOUT_X360_ARTIST.XEX.
// The generic effects state-machine spine: OnConstruct seeds the state timer; Tick runs the
// determine-next-state -> change-state -> on-tick loop, deferring per-machine behaviour to the
// four protected virtuals. Members mState(+0x04)/mTime(+0x08) are declared in the header.

namespace BrnEffects
{
    // X360 0x822798D0 -- seed the state timer to 0 (stfs flt_82001CC0 == 0.0f to this+0x08).
    void EffectsStateMachine::OnConstruct()
    {
        mTime = 0.0f;   // +0x08
    }

    // X360 0x82280468 -- one tick of the effects state machine.
    void EffectsStateMachine::Tick(CarState& lCarState, RaceCarParticleEffectHelper& lHelper)
    {
        // ⚠ CORRECTED 2026-09-02 (tyre-mark wave). The old body read
        // `*(RaceCarState* + 0x64)` and stored through `*(u8*)(&lCarState + 0x4E)`, calling it
        // "the race-car's force flag". It is the DEBUG COMPONENT's force-jump toggle, and it
        // was reading the WRONG OBJECT: the asm is
        //     lwz  r11, 0x10(r28)   ; r28 == the helper -> helper+0x10 == mpDebugComponent
        //     lbz  r11, 0x64(r11)   ; EffectsDebugComponent+0x64 == mJumpParams.mbForceJumping
        //     stb  1,   0x4E(r29)   ; CarState+0x4E == mbJumping
        // helper+0x10 is mpDebugComponent (base mpParticleModule +0x00, mpActiveRaceCar +0x04,
        // mpEffectsModule +0x08, mpRaceCarState +0x0C, mpDebugComponent +0x10), and
        // RaceCarState+0x64 is padding inside maWheels[0] -- so the old read was garbage.
        // Both ends are named members now; the raw-offset hacks are gone.
        if (lHelper.DebugComponent()->JumpParams().IsForceJumping())
        {
            lCarState.mbJumping = true;   // CarState +0x4E
        }

        // Count the state timer down; note whether it expired this frame.
        bool lbStateTimerExpired = false;
        if (mTime > 0.0f)
        {
            mTime -= lCarState.GetDt();   // asm `lfs f13, 0x10(r29)` == EffectsModuleParams::mDt
            lbStateTimerExpired = (mTime <= 0.0f);
        }

        // Ask the derived machine for the next state.
        const EffectsState leNextState =
            OnDetermineNextState(lCarState, lbStateTimerExpired, mState, lHelper);
        CGS_ASSERT(leNextState < EffectsState_MAX, "Invalid State in EffectsStateMachine");

        // On a state change: reset the timer (before the entry hook, matching the X360 store
        // order), run the entry hook, then commit the new state.
        if (leNextState != mState)
        {
            mTime = 0.0f;   // +0x08
            OnChangeState(leNextState, lHelper, lCarState);
            mState = leNextState;   // +0x04
        }

        // Per-frame hook.
        OnTick(lCarState, lHelper);
    }

    // =====================================================================================
    // The three base virtuals (DWARF EffectsStateMachine.h:76/:80/:83).
    //
    // NONE of them has an X360 export -- the ARTIST build carries only OnConstruct @0x822798D0
    // and Tick @0x82280468 for this class. The base slots are therefore empty defaults that
    // ICF folded away (every derived machine overrides all three; Tick reaches them only
    // through the vtable). Empty is what the console does, so empty is what is written here --
    // with the one exception the compiler forces: OnDetermineNextState must return a state,
    // and "the state we are already in" is the console's own no-transition value (Tick's
    // `leNextState != mState` test then does nothing).
    // =====================================================================================
    EffectsState EffectsStateMachine::OnDetermineNextState(CarState& /*lCarState*/,
                                                          bool /*lbStateTimerExpired*/,
                                                          EffectsState leCurrentState,
                                                          RaceCarParticleEffectHelper& /*lHelper*/)
    {
        return leCurrentState;
    }

    void EffectsStateMachine::OnChangeState(EffectsState /*leNewState*/,
                                            RaceCarParticleEffectHelper& /*lHelper*/,
                                            CarState& /*lCarState*/)
    {
    }

    void EffectsStateMachine::OnTick(CarState& /*lCarState*/,
                                     RaceCarParticleEffectHelper& /*lHelper*/)
    {
    }
}
