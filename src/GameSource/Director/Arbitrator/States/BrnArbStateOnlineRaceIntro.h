#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_RACE_INTRO_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_RACE_INTRO_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

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
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateOnlineRaceIntro.h, X360-attested for this build). The per-member X360 offsets
// are pinned from the ARTIST asm (Construct @0x8225AE98, CalculateStateTimes @0x821F6370,
// Update @0x822734A8, SetupRivalMovie @0x8226EC18):
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
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
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
        // DWARF EState (BrnArbStateOnlineRaceIntro.h:79). The online-race-intro state machine.
        // Construct seeds 0 (INACTIVE); Prepare forces 1 (PREPARING). Update's case-1 success
        // edge stores 2 (ACTIVE); the per-rival/player/lights edges store 3..9; the hand-back
        // edges run ChangeToState with blocked value 10 (CHANGING_TO_ROAMING). The dispatch
        // table is indexed by this value (0..11; >0xA hits the default assert). Values are the
        // X360 jump-table case indices / the immediates the asm stores into meState (+0x220).
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

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                             // @0x8225AE98
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x821F6340
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x822734A8
        const char* GetName() const override;                        // @0x821F6480

        // Release() and Destruct() are listed in the DWARF (cpp:499 / cpp:529) but the X360
        // ledger does NOT attest a body for either in this TU's recovered function set (the
        // postmortem packet bodies only Construct / GetName / Prepare / CalculateStateTimes /
        // Update / SetupRivalMovie). They are therefore DECLARATION-ONLY here -- declared so the
        // concrete class overrides the base virtuals this state's X360 set defines, with their
        // bodies landing when the ARTIST asm for them is recovered. NEVER fabricate a body for a
        // function with no attested asm.
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;  // @cpp:499 (no asm; decl-only)
        void        Destruct() override;                                // @cpp:529 (no asm; decl-only)

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Allocated in Prepare/Update via BehaviourManager::NewBehaviour<TBehaviour> and
        // released in Update/Release via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Update/SetupRival
        // asm: mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup helper word(+0x08),
        // mpManager(+0x0C), mpBehaviour(+0x10). The X360 re-resolves the live behaviour from the
        // manager pool through (helper word, allocation key); GetBehaviour() returns the cached
        // pointer to that same behaviour after asserting IsAllocated() (sub_821FD3E8 ==
        // BrnBehaviourManager.h:589). GetProducedCamera() returns the camera the live behaviour
        // produced this frame, which the manager keeps alongside the behaviour in the same pool
        // slot (sub_821FD450 reads slot+0x10 == BrnBehaviourManager.h:610). FLAG: the +0x08
        // word's role is not fully recovered in this TU (modelled as an opaque behaviour-lookup
        // helper index, as in BrnArbStateRaceIntro.h / BrnArbStateOnlineCarSelect.h).
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool IsAllocated() const { return mbAllocated; }

            // The live behaviour this handle owns (only valid while IsAllocated()). The X360
            // asserts IsAllocated() inside the manager-pool lookup before returning it
            // (sub_821FD3E8; BrnBehaviourManager.h:589).
            TBehaviour* GetBehaviour() const
            {
                CGS_ASSERT(mbAllocated, "IsAllocated()");
                return mpBehaviour;
            }

            // The camera the live behaviour produced this frame. The X360 reads it from the
            // manager pool slot the handle resolves to (sub_821FD450 returns slot+0x10;
            // BrnBehaviourManager.h:610), which is the same camera the behaviour writes each
            // frame -- modelled by NAME as the behaviour's produced camera. Asserts
            // IsAllocated(). Defined out-of-line in the .cpp where the behaviour type is
            // complete.
            const Camera::Camera& GetProducedCamera() const;

            // Drop the manager-side hold on the behaviour and clear the handle back to empty
            // (X360 sub_8222DFD8 / sub_8222DCA8): when allocated, UnSetBehaviourUsedByHandle(
            // mpManager, muAllocationKey) then zero the slots. Always returns true. Defined
            // out-of-line in the .cpp where BehaviourManager is complete.
            bool Release();

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // ---- the per-take interpolation parameters (mInterpolatorParams, +0x194, 0x10) -----
        // The BehaviourInterpolate::Parameters block Construct seeds and Update hands to the
        // interpolator setup (sub_8224EE58). The DecFIGS DWARF names the member
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

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourInterpolate> mInterpolator;         // X360 +0x180
        InterpolatorParameters                        mInterpolatorParams;   // X360 +0x194
        BehaviourHandle<Camera::BehaviourIceAnim> maRivalBehaviourHandle[KU_NUM_RIVAL_BEHAVIOURS]; // +0x1A4
        BehaviourHandle<Camera::BehaviourIceAnim>     mPlayerBehaviourHandle; // +0x1E0
        BehaviourHandle<Camera::BehaviourIceAnim>     mLightsBehaviourHandle; // +0x1F4
        f32                                           mfTimeToSpendInterpolating;   // +0x208
        f32                                           mfTimeToSpendLookingAtRival;  // +0x20C
        f32                                           mfTimeToSpendLookingAtPlayer; // +0x210
        f32                                           mfTimeInState;          // +0x214
        u32                                           muCurrentRival;         // +0x218
        u32                                           muRivalMovieOffset;     // +0x21C
        EState                                        meState;                // +0x220

        // ---- private helpers (X360 attested) ---------------------------------------------
        // Split the event's intro time budget across the per-rival "show" / "move" segments.
        // @0x821F6370. luNumRivals == 0 puts the whole budget in mfTimeToSpendLookingAtPlayer;
        // otherwise it divides lfMaxTime (asserting it is > 0 after a fixed 0.25s deduction)
        // across the (numRivals+1) shows plus the 2*(numRivals+1) moves.
        void CalculateStateTimes(u32 luNumRivals, f32 lfMaxTime);   // @cpp:74

        // Allocate + configure the rival "show" ICE-anim behaviour for rival liRivalIndex from
        // the event's online-race-start shot group. @0x8226EC18.
        void SetupRivalMovie(ArbStateSharedInfo& lrSharedInfo, u32 luRivalIndex);   // @cpp:472

        // Allocate the take-to-take interpolator and latch it to blend lrFrom -> lrTo over
        // mfTimeToSpendInterpolating, seeded from mInterpolatorParams. De-inlines the X360
        // interpolator-setup sequence (NewBehaviour<BehaviourInterpolate> + the per-take params
        // + the sub_8224EE58 Setup over the two producing cameras) the MOVING_TO_* edges share.
        // FLAG: the X360 builds the from/to camera references from the producing BehaviourHandles
        // (sub_821FD4B8) before the multi-arg Setup; modelled here through the BehaviourInterpolate
        // named-setup API, NOT paraphrased to per-field stores.
        void SetupInterpolator(ArbStateSharedInfo& lrSharedInfo,
                               const Camera::Camera& lrFromCamera,
                               const Camera::Camera& lrToCamera);
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ONLINE_RACE_INTRO_H
