#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_RACE_INTRO_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_RACE_INTRO_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.h
//
// BrnDirector::ArbStateOnlineRaceIntro -- the director arbitrator state that runs the camera
// for the ONLINE race intro: the pre-race fly-by that shows each rival in turn, then the
// player, then the start lights/countdown, before handing control back to the roaming state.
// It owns one camera interpolator (the blend between successive ICE-anim takes) and four
// ICE-anim camera behaviours -- three rival "show" takes (cycled through muRivalMovieOffset),
// one player "show" take and one start-lights take -- and walks the state machine
// INACTIVE -> PREPARING -> ACTIVE -> SHOWING_PLAYER -> SHOWING_RIVAL -> MOVING_TO_RIVAL ->
// MOVING_TO_PLAYER -> SHOWING_PLAYER_AGAIN -> WAITING_FOR_COUNTDOWN -> SHOWING_LIGHTS ->
// CHANGING_TO_ROAMING -> RELEASING. Derives from ArbitratorState (vtable order pinned by the
// base).
//
// CalculateStateTimes() splits the event's intro time budget across the per-rival "show" /
// "move" segments so the whole fly-by fits the time the game gives it.
//
// LAYOUT: the member names and declaration order are the shipped build's; the per-member
// console offsets quoted below are pinned from Construct / CalculateStateTimes / Update /
// SetupRivalMovie:
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by
//     name through the base GetNonConstCamera() / the effect-trigger free functions; Construct
//     calls Camera::Construct(this+0x10)).
//   * mInterpolator        @+0x180 (BehaviourHandle, 0x14) -- the take-to-take blend handle
//                           (Construct zeroes its five words: mbAllocated@+0x180,
//                           muAllocationKey@+0x184, muHelperIndex@+0x188, mpManager@+0x18C,
//                           mpBehaviour@+0x190).
//   * mInterpolatorParams  @+0x194 (BehaviourInterpolate::Parameters, 0x10) -- the per-take
//                           interpolation parameters Construct seeds ({+0x00:0, +0x04:8,
//                           +0x08:0, +0x0C:2}; the +0x04 word is set to 8 then the +0x0C word
//                           to 2, the rest 0).
//   * maRivalBehaviourHandle[3] @+0x1A4 / +0x1B8 / +0x1CC (BehaviourHandle, 0x14 each) -- the
//                           three rival "show" ICE-anim handles.
//   * mPlayerBehaviourHandle @+0x1E0 (BehaviourHandle, 0x14) -- the player "show" handle.
//   * mLightsBehaviourHandle @+0x1F4 (BehaviourHandle, 0x14) -- the start-lights handle.
//   * mfTimeToSpendInterpolating @+0x208  (f32) -- per "move" segment time.
//   * mfTimeToSpendLookingAtRival @+0x20C (f32) -- per rival "show" segment time.
//   * mfTimeToSpendLookingAtPlayer @+0x210 (f32) -- player "show" segment time.
//   * mfTimeInState        @+0x214 (f32) -- seconds spent in the current sub-state.
//   * muCurrentRival       @+0x218 (u32) -- which rival the fly-by is on.
//   * muRivalMovieOffset   @+0x21C (u32) -- the rival-handle-array allocation cursor.
//   * meState              @+0x220 (EState) -- the state-machine value; the dispatch table is
//                           indexed by it (0..11; default -> assert).
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the console's 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute offsets
// shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera
    {
        class BehaviourManager;
        class BehaviourIceAnim;
        class BehaviourInterpolate;
    }

    class ArbStateOnlineRaceIntro : public ArbitratorState
    {
    public:
        // EState -- the online-race-intro state machine.
        // Construct seeds 0 (INACTIVE); Prepare forces 1 (PREPARING). Update's case-1 success
        // edge stores 2 (ACTIVE); the per-rival/player/lights edges store 3..9; the hand-back
        // edges run ChangeToState with blocked value 10 (CHANGING_TO_ROAMING). The dispatch
        // table is indexed by this value (0..11; >0xA hits the default assert). Values are the
        // console's dispatch-table case indices / the values it stores into meState (+0x220).
        enum EState
        {
            E_STATE_INACTIVE             = 0,
            E_STATE_PREPARING            = 1,
            E_STATE_ACTIVE               = 2,
            E_STATE_SHOWING_PLAYER       = 3,
            E_STATE_SHOWING_RIVAL        = 4,
            E_STATE_MOVING_TO_RIVAL      = 5,
            E_STATE_MOVING_TO_PLAYER     = 6,
            E_STATE_SHOWING_PLAYER_AGAIN = 7,
            E_STATE_WAITING_FOR_COUNTDOWN = 8,
            E_STATE_SHOWING_LIGHTS       = 9,
            E_STATE_CHANGING_TO_ROAMING  = 10,
            E_STATE_RELEASING            = 11,

            E_NUM_STATES                 = 12
        };

        // ---- ArbitratorState virtual overrides (vtable order; see base) ------------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        const char* GetName() const override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;

        // Destruct() is NOT in this TU's recovered function set -- no symbol and no reference to
        // one exists anywhere in the shipped build for this class -- so it keeps the base
        // declaration and no override is added here (the declaration record lists one; the
        // shipped build does not).

    private:
        // RETIRED: this state used to carry its OWN nested five-word BehaviourHandle<> copy.
        // It now uses the SHARED BrnDirector::Camera::BehaviourHandle<TBehaviour>, which is what
        // the console has -- ONE template instantiated per behaviour type, not a per-state
        // duplicate. Using the shared handle is therefore more faithful, and it is what lets the
        // bodied BehaviourManager::NewBehaviour<> overload bind here (the generic THandle overload
        // is declaration-only, so a nested fork compiles and then leaves the behaviour unallocated
        // at link time). The shared handle also identifies the +0x08 word this file used to flag
        // as "role not recovered": it is the owning BehaviourHelper pool pointer, which a u32
        // would have truncated on this host. It brings IsAllocated / GetBehaviour /
        // GetProducedCamera / Release with it, all bodied, so this header no longer declares any
        // of them.

        // ---- the per-take interpolation parameters (mInterpolatorParams, +0x194, 0x10) -----
        // The BehaviourInterpolate::Parameters block Construct seeds and Update hands to the
        // interpolator setup. The declaration record names the member
        // BrnDirector::Camera::BehaviourInterpolate::Parameters but the BehaviourInterpolate TU's
        // minimal slice models Parameters as an opaque type; the four words Construct writes are
        // reproduced here BY VALUE so the seed is byte-faithful. FLAG: the field ROLES are not
        // recovered (only the seeded values are asm-attested: {+0x00:0, +0x04:8, +0x08:0,
        // +0x0C:2}); replace with the real BehaviourInterpolate::Parameters layout when that TU
        // lands. Kept a nested POD (not the shared slice's empty Parameters) so the 16-byte
        // seed has somewhere faithful to live without forking the shared type.
        struct InterpolatorParameters
        {
            InterpolatorParameters()
                : muField00(0), muField04(0), muField08(0), muField0C(0) {}

            u32 muField00;   // +0x00  Construct: 0
            u32 muField04;   // +0x04  Construct: 8   (FLAG: role not recovered)
            u32 muField08;   // +0x08  Construct: 0
            u32 muField0C;   // +0x0C  Construct: 2   (FLAG: role not recovered)
        };

        static const u32 KU_NUM_RIVAL_BEHAVIOURS = 3;

        // ---- members, declaration order; offsets in comments -----------------------------
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolator;       // +0x180
        InterpolatorParameters                        mInterpolatorParams;   // +0x194
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> maRivalBehaviourHandle[KU_NUM_RIVAL_BEHAVIOURS]; // +0x1A4
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mPlayerBehaviourHandle; // +0x1E0
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mLightsBehaviourHandle; // +0x1F4
        f32                                           mfTimeToSpendInterpolating;   // +0x208
        f32                                           mfTimeToSpendLookingAtRival;  // +0x20C
        f32                                           mfTimeToSpendLookingAtPlayer; // +0x210
        f32                                           mfTimeInState;          // +0x214
        u32                                           muCurrentRival;         // +0x218
        u32                                           muRivalMovieOffset;     // +0x21C
        EState                                        meState;                // +0x220

        // ---- private helpers (console-attested) ------------------------------------------
        // Split the event's intro time budget across the per-rival "show" / "move" segments.
        // luNumRivals == 0 puts the whole budget in mfTimeToSpendLookingAtPlayer;
        // otherwise it divides lfMaxTime (asserting it is > 0 after a fixed 0.25s deduction)
        // across the (numRivals+1) shows plus the 2*(numRivals+1) moves.
        void CalculateStateTimes(u32 luNumRivals, f32 lfMaxTime);   // @cpp:74

        // Allocate + configure the rival "show" ICE-anim behaviour for rival liRivalIndex from
        // the event's online-race-start shot group.
        void SetupRivalMovie(ArbStateSharedInfo& lrSharedInfo, u32 luRivalIndex);   // @cpp:472

        // Allocate the take-to-take interpolator and latch it to blend lrFrom -> lrTo over
        // mfTimeToSpendInterpolating, seeded from mInterpolatorParams. De-inlines the console's
        // interpolator-setup sequence (NewBehaviour<BehaviourInterpolate> + the per-take params
        // + the Setup over the two producing cameras) the MOVING_TO_* edges share.
        // FLAG: the console builds the from/to camera references from the producing
        // BehaviourHandles before the multi-arg Setup; modelled here through the BehaviourInterpolate
        // named-setup API, NOT paraphrased to per-field stores.
        void SetupInterpolator(ArbStateSharedInfo& lrSharedInfo,
                               const Camera::Camera& lrFromCamera,
                               const Camera::Camera& lrToCamera);
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_RACE_INTRO_H
