#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ROAMING_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ROAMING_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/MomentController/BrnMomentSelector.h"      // MomentSelector (by value)

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h
//
// BrnDirector::ArbStateRoaming -- the "free roam / driving" director arbitrator state. It is
// the default state the director sits in while the player drives around Paradise City: it
// runs the establishing-shot moment selector, requests the per-event camera PFX hooks
// (Smash / Billboard / Checkpoint / Wrecked / Race_Day / Damage_Crit), and decides when to
// transition out to the crashing / takedown / race-intro / drive-thru / car-select / rank-up
// states. Derives from ArbitratorState (vtable order pinned by the base).
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateRoaming.h, X360-attested for this build). The per-member X360 offsets are
// pinned from the ARTIST asm (Construct @0x82259C00, Release @0x82234930, Prepare
// @0x82259D58, ProcessPossibleFX @0x82234A00, ProcessPossibleStateChanges @0x82219C58):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (the X360
//     Construct/Prepare/FX reach it via this+0x10); this state reaches it by name through the
//     base (GetCamera()/effect-trigger free functions).
//   * mInterpolater     @+0x180 (BehaviourHandle, 0x14)  -- the interpolation cam handle
//   * mRoadRunnerCam    @+0x194 (BehaviourHandle, 0x14)  -- the road-runner cam handle
//   * mInterpolateParams@+0x1A8 (0x10)                   -- the interpolation parameters
//   * mMomentSelector   @+0x1B8 (the establishing-shot selector; embedded by value)
//   * the trailing scalar/flag block @+0x39C..+0x3C8.
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera / MomentSelector widen, so
// absolute offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // The interpolation behaviour the roaming state blends to (full type lands with its own
    // TU); referenced only as the BehaviourHandle template argument here.
    namespace Camera { class BehaviourInterpolate; class BehaviourRoadRunner; }

    class ArbStateRoaming : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateRoaming.h:94). The roaming state machine: drive, idle,
        // and the "changing to <other arbitrator state>" hand-off values. Values are the
        // X360 immediates stored into meState (Construct seeds 1==PREPARING; the transition
        // calls pass 2/3/5/6/7 directly).
        enum EState
        {
            E_STATE_INACTIVE                       = 0,
            E_STATE_PREPARING                      = 1,
            E_STATE_DRIVING                        = 2,
            E_STATE_CHANGING_TO_IDLE_INTERPOLATE   = 3,
            E_STATE_CHANGING_TO_IDLE_BLACKOUT      = 4,
            E_STATE_IDLE                           = 5,
            E_STATE_IDLE_RESET                     = 6,
            E_STATE_CHANGING_TO_CRASHING           = 7,
            E_STATE_CHANGING_TO_TAKEDOWN           = 8,
            E_STATE_CHANGING_TO_RACE_INTRO         = 9,
            E_STATE_CHANGING_TO_ONLINE_RACE_INTRO  = 10,
            E_STATE_CHANGING_TO_RACE_POST_EVENT    = 11,
            E_STATE_CHANGING_TO_DRIVETHRU          = 12,
            E_STATE_CHANGING_TO_CAR_SELECT         = 13,
            E_STATE_CHANGING_TO_RANK_UP            = 14,
            E_STATE_CHANGING_TO_ONLINE_CAR_SELECT  = 15,
            E_STATE_RELEASING                      = 16,

            E_NUM_STATES                           = 17
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                            // @0x82259C00
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x82259D58
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x82234930
        const char* GetName() const override;                        // @0x821F6238

        // Update() / Destruct() are NOT in this TU's X360 function set (they live in their
        // own TUs); they keep the base declarations (no override added here).

    private:
        // ---- private per-state helpers (DWARF BrnArbStateRoaming.h) ----------------------
        // ProcessPossiblePaybackEffects (DWARF :1017) is NOT in this TU's X360 set; omitted.
        void ProcessPossibleFX(ArbStateSharedInfo& lrSharedInfo);           // @0x82234A00
        void ProcessPossibleStateChanges(ArbStateSharedInfo& lrSharedInfo); // @0x82219C58

        // De-inlined inner branch of ProcessPossibleStateChanges (the X360 flattens it inline):
        // the drive-thru / car-select / showtime / idle-reset / rank-up transitions taken while
        // an event is running but not yet ACTIVE. Not a separate X360 function.
        void ProcessActiveDrivingTransitions(ArbStateSharedInfo& lrSharedInfo,
                                             GameState& lrGameState);

        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Released in Release() via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Release asm:
        // mbAllocated(+0x00), muAllocationKey(+0x04), a spare helper word(+0x08),
        // mpManager(+0x0C), mpBehaviour(+0x10). FLAG: the +0x08 word's role is not recovered
        // (never read in this TU); modelled as an opaque helper-index word.
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (spare)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // The interpolation curve/easing parameters the behaviour reads. Opaque slice
        // (Construct seeds four words: 8, 0, then the curve/method selectors); 0x10 bytes.
        struct InterpolateParameters
        {
            u32 mauParams[4];   // +0x00..+0x0C : (Construct: [+0x00]=8, ...). FLAG: field roles not recovered.
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;   // X360 +0x180
        BehaviourHandle<Camera::BehaviourRoadRunner>  mRoadRunnerCam;  // X360 +0x194
        InterpolateParameters                         mInterpolateParams; // X360 +0x1A8
        MomentSelector                                mMomentSelector; // X360 +0x1B8

        // trailing scalar / flag block (X360 +0x39C..+0x3C8). DWARF names; Construct zeroes
        // mbHasReversed / mbDisablePictureParadise / mbPlayRaceEndEffect, Prepare seeds the
        // float/timer block.
        bool   mbHasReversed;            // +0x39C
        bool   mbStartedJumpEffect;      // +0x39D (Prepare clears, FX sets on Billboard)
        bool   mbUsingInterpolator;      // +0x39E (driven by Update; default here)
        bool   mbDisablePictureParadise; // +0x39F (StateChanges reads)
        bool   mbPlayRaceEndEffect;      // +0x3A0 (FX reads)
        f32    mfTimeInShowtimeIntro;    // +0x3A4
        f32    mfBordersTime;            // +0x3A8
        f32    mfImpactShakeAmount;      // +0x3AC
        f32    mfNormalShakeAmount;      // +0x3B0
        f32    mfTimeUntilNextStrobe;    // +0x3B4
        s32    miStrobeCount;            // +0x3B8
        EState meState;                  // +0x3BC (the roaming state-machine state)
        f32    mfStateTimer;             // +0x3C0
        u8     mu8ImpactShakeType;       // +0x3C4
        // FLAG: +0x3C8 is an extra f32 (the per-frame "Wrecked" effect counter) the X360 FX
        // reads/decrements and Prepare seeds to 1.0; absent from the DWARF member list (a
        // small merge-window delta). Named from its FX role.
        f32    mfWreckedEffectAmount;    // +0x3C8
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ROAMING_H
