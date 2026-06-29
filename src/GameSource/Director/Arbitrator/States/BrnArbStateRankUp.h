#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RANK_UP_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RANK_UP_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h
//
// BrnDirector::ArbStateRankUp -- the director arbitrator state that runs the "rank up"
// camera sequence (the post-event ICE-anim flourish that cycles a camera "take" per rival
// as the player climbs the rankings). It allocates an ICE-anim camera behaviour, plays the
// rank-up shot-group's shots one rival at a time, and walks the state machine
// INACTIVE -> PREPARING -> ACTIVE -> CHANGING_TO_ROAMING, handing control back to the
// roaming state once the take has finished (or once the player's rank-up is no longer
// active). Derives from ArbitratorState (vtable order pinned by the base).
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateRankUp.h, X360-attested for this build). The per-member X360 offsets are
// pinned from the ARTIST asm (Construct @0x8225B2B8, Prepare @0x82270EE8,
// Update @0x82236380, Release @0x82236650):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by
//     name through the base GetNonConstCamera() / the effect-trigger free functions;
//     Construct calls Camera::Construct(this+0x10)).
//   * mIceCam   @+0x180 (BehaviourHandle, 0x14) -- the ICE-anim cam handle (Construct zeroes
//                its five words: mbAllocated@+0x180, muAllocationKey@+0x184,
//                muHelperIndex@+0x188, mpManager@+0x18C, mpBehaviour@+0x190)
//   * miRival   @+0x194 (the rival index walked through the shot list; Update's `Get*` path
//                increments it each frame the rank-up is active and indexes the shot list by
//                it modulo the shot count)
//   * meState   @+0x198 (EState, the rank-up state machine value; the dispatch table is
//                indexed by this value 0..3, default -> assert)
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute
// offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; class BehaviourIceAnim; }

    class ArbStateRankUp : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateRankUp.h:66). The rank-up state machine. Construct seeds 0
        // (INACTIVE); Update's case-1 (PREPARING) success edge stores 2 (ACTIVE), and the
        // dispatch table is indexed by this value (0..3; values >3 hit the default assert).
        // The hand-back-to-roaming edge stores 3 (CHANGING_TO_ROAMING). Values are the X360
        // jump-table case indices / the immediates the asm stores into meState (+0x198).
        enum EState
        {
            E_STATE_INACTIVE            = 0,
            E_STATE_PREPARING           = 1,
            E_STATE_ACTIVE              = 2,
            E_STATE_CHANGING_TO_ROAMING = 3,
            E_STATE_RELEASING           = 4,

            E_NUM_STATES                = 5
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                              // @0x8225B2B8
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;  // @0x82270EE8
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;   // @0x82236380
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;  // @0x82236650
        const char* GetName() const override;                          // @0x821F6710

        // Destruct() is NOT in this TU's X360 function set (it keeps the base declaration; no
        // override added here).

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Allocated in Prepare/Update via BehaviourManager::NewBehaviour<BehaviourIceAnim> and
        // released in Release via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Prepare/Update/
        // Release asm: mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup helper
        // word(+0x08), mpManager(+0x0C), mpBehaviour(+0x10). The X360 re-resolves the live
        // behaviour from the manager pool through (helper word, allocation key); GetBehaviour()
        // returns the cached pointer to that same behaviour after asserting IsAllocated().
        // FLAG: the +0x08 word's role is not fully recovered in this TU (modelled as an opaque
        // behaviour-lookup helper index, as in BrnArbStateRaceIntro.h / BrnArbStateCrashMode.h).
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool IsAllocated() const { return mbAllocated; }

            // The live behaviour this handle owns (only valid while IsAllocated()). The X360
            // asserts IsAllocated() inside the manager-pool lookup before returning it
            // (sub_821FD3E8 / BrnBehaviourManager.h:589).
            TBehaviour* GetBehaviour() const
            {
                CGS_ASSERT(mbAllocated, "IsAllocated()");
                return mpBehaviour;
            }

            // True once the owned behaviour has finished its initial Prepare and is ready to
            // use. The X360 (sub @0x822128A0, BrnBehaviourManager.h:517) asserts mbAllocated
            // then returns !mpManager->IsBehaviourWaitingToPrepare(muAllocationKey). Defined
            // out-of-line in the .cpp where BehaviourManager is complete.
            bool IsBehaviourReadyToUse() const;

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourIceAnim> mIceCam;   // X360 +0x180  the rank-up ICE cam handle
        s32    miRival;                                      // +0x194  the rival index walked per take
        EState meState;                                      // +0x198  the rank-up state-machine state
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RANK_UP_H
