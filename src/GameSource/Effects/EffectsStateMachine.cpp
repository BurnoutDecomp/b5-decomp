#include "GameSource/Effects/EffectsStateMachine.h"
#include "GameSource/Effects/EffectsModule.h"          // BrnEffects::CarState (+0x10 mfExhaustSmokeBlendDelta, +0x4E mPad4E)
#include "GameSource/Effects/ParticleEffectHelper.h"   // RaceCarParticleEffectHelper::RaceCarState()
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
        // Propagate the race-car's force flag into the car state. The X360 reads the byte at
        // RaceCarState+0x64 directly (lbz r11,0x64(r11)); RaceCarState() returns a pointer to the
        // (incomplete-in-scope) RaceCarState, so read the raw byte to avoid fabricating a member
        // name (offset 0x64 lies inside maWheels[0]).
        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState = lHelper.RaceCarState();
        if (*(reinterpret_cast<const u8*>(lpRaceCarState) + 0x64) != 0)
        {
            *(reinterpret_cast<u8*>(&lCarState) + 0x4E) = 1;   // CarState +0x4E
        }

        // Count the state timer down; note whether it expired this frame.
        bool lbStateTimerExpired = false;
        if (mTime > 0.0f)
        {
            mTime -= lCarState.mfExhaustSmokeBlendDelta;   // CarState +0x10 (frame delta)
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
}
