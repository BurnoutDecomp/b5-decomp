#ifndef GAMESOURCE_EFFECTS_BOOST_BOOSTSTATEMACHINE_H
#define GAMESOURCE_EFFECTS_BOOST_BOOSTSTATEMACHINE_H

#include "types.hpp"
#include "GameSource/Effects/EffectsStateMachine.h"   // BrnEffects::EffectsStateMachine (base), EffectsState

// ============================================================================
// GameSource/Effects/Boost/BoostStateMachine.h
//
// BrnEffects::BoostStateMachine -- the per-race-car effects state machine that
// drives the boost / exhaust-smoke / exhaust-pop particle effects. It derives
// from BrnEffects::EffectsStateMachine and overrides the four On* virtuals
// (OnConstruct / OnDetermineNextState / OnChangeState / OnTick) that the base
// state machine calls.
//
// SOURCE-OF-TRUTH: behaviour + member OFFSETS verified against the ARTIST X360
// asm (OnConstruct 0x82280620, OnDetermineNextState 0x82288230, OnChangeState
// 0x8229E3D8, OnTick 0x82298BE0, plus the Set*/Start/Stop helpers); declaration
// SHAPE (virtual/const, vtable order, names, the MAX_BOOST_EFFECTS / mEffects /
// muNumBoostTags / mfExhaustSmokeBlend member set) from the DecFIGS DWARF
// (GameSource/Effects/Boost/BoostStateMachine.h), gated on the ARTIST ledger.
//
// LAYOUT (base EffectsStateMachine occupies +0x00..+0x0B = vptr + mState +
// mTime; verified by OnConstruct's stores):
//   +0x0C  muNumBoostTags      (count of live tagged effects)
//   +0x10  mEffects[4]         (TaggedEffect = one u32 LION effect handle each)
//   +0x20  mfExhaustSmokeBlend (the exhaust-smoke blend the boost states drive)
//   sizeof == 0x24.
// ============================================================================

namespace BrnEffects
{
    // Forward declarations of the types this state machine is handed by reference.
    class ParticleEffectHelper;
    class RaceCarParticleEffectHelper;
    struct CarState;

    // ------------------------------------------------------------------------
    // BrnEffects::BoostStateMachine
    // ------------------------------------------------------------------------
    class BoostStateMachine : public EffectsStateMachine
    {
    public:
        // DWARF BoostStateMachine.h:38. The boost effect slots are a fixed array of 4.
        static const u32 KU_MAX_BOOST_EFFECTS = 4;

        // ---- public helpers (DWARF BoostStateMachine.h:42 onward) ----------
        // These were attested by the ARTIST ledger; the ones whose bodies live in
        // THIS TU are defined in BoostStateMachine.cpp. Reset/SetWorldIndex/
        // SetBlendValue/SetEnabled walk the live tagged effects and poke the
        // matching playing-LION slots.

        // Reset @ 0x82298B20. Stop every live boost effect, invalidate the tags,
        // and clear the tag count, state and exhaust-smoke blend.
        void Reset(ParticleEffectHelper& lParticleEffectHelper);

        // SetWorldIndex @ 0x82280578. Set the world index on every live boost
        // effect slot (and flag it changed).
        void SetWorldIndex(ParticleEffectHelper& lParticleEffectHelper, u32 luWorldIndex);

        // SetBlendValue @ 0x82280660. Set the state blend on every live boost
        // effect slot (and flag it changed).
        void SetBlendValue(ParticleEffectHelper& lParticleEffectHelper, f32 lfBlend);

        // SetEnabled @ 0x82280710. Enable / disable every live boost effect slot.
        void SetEnabled(ParticleEffectHelper& lParticleEffectHelper, bool lbEnabled);

    protected:
        // ---- EffectsStateMachine virtual overrides --------------------------
        // OnConstruct @ 0x82280620. Zero the tag count / state / exhaust blend,
        // invalidate the 4 tag handles, and set mfExhaustSmokeBlend to 1.0f.
        virtual void OnConstruct();

        // OnDetermineNextState @ 0x82288230. The boost effects state transition
        // table: given the current state, the car state and whether the state
        // timer has expired, returns the next EffectsState (and drives the
        // exhaust-smoke blend along the way).
        virtual EffectsState OnDetermineNextState(CarState& lCarState,
                                                  bool lbStateTimerExpired,
                                                  EffectsState leCurrentState,
                                                  RaceCarParticleEffectHelper& lHelper);

        // OnChangeState @ 0x8229E3D8. Runs on entry to each state: start / stop
        // the appropriate boost / exhaust effects and seed the exhaust-smoke blend.
        virtual void OnChangeState(EffectsState leNewState,
                                   RaceCarParticleEffectHelper& lHelper,
                                   CarState& lCarState);

        // OnTick @ 0x82298BE0. Per-frame: refresh the boost-effect world
        // transforms from the car's boost locators (or stop the ones with no
        // live locator), and round-trip the boost locators through the replay
        // effects serialiser when recording / playing.
        virtual void OnTick(CarState& lCarState, RaceCarParticleEffectHelper& lHelper);

    private:
        // ---- private effect helpers (DWARF BoostStateMachine.h:96/97 region) -
        // StartEffects @ 0x8229B920. For each tag slot, stop any existing effect
        // then start the named LION effect (forwarding the world index) and store
        // the new handle in the tag.
        void StartEffects(ParticleEffectHelper& lParticleEffectHelper,
                          const char* lpcEffectName, u32 luWorldIndex);

        // StopEffects @ 0x822990C0. Stop every live boost effect and invalidate
        // the tag handles (does NOT clear the tag count).
        void StopEffects(ParticleEffectHelper& lParticleEffectHelper);

        // ---- data members (offsets relative to the BoostStateMachine object) -
        // A single boost effect tag: just the playing-LION effect handle. The
        // DWARF spells the array element TaggedEffect; the ARTIST asm only ever
        // reads/writes the 4-byte handle, so the wrapper holds exactly that.
        struct TaggedEffect
        {
            u32 muHandle;   // +0x00  the LION effect handle (or KU_HANDLE_INVALID)
        };

        u32          muNumBoostTags;                  // +0x0C  live tag count
        TaggedEffect mEffects[KU_MAX_BOOST_EFFECTS];  // +0x10  the 4 boost effect tags
        f32          mfExhaustSmokeBlend;             // +0x20  exhaust-smoke blend value
    };

    // Console sizeof == 0x24 (4-byte vptr + mState + mTime + the four added members).
    // On the x64 host the vptr widens to 8 bytes, so the byte-exact size differs by
    // design; members are accessed by name, not raw offset. The derived-member offsets
    // above are the ARTIST X360 offsets (verified by OnConstruct's stores).
}

#endif // GAMESOURCE_EFFECTS_BOOST_BOOSTSTATEMACHINE_H
