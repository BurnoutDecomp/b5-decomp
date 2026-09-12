#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RANK_UP_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RANK_UP_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle

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
// LAYOUT: member names and declaration order are reproduced from the shipped build; the
// per-member console offsets are recorded as +0xNN:
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it by
//     name through the base GetNonConstCamera() / the effect-trigger free functions;
//     Construct calls Camera::Construct on it).
//   * mIceCam   +0x180 (BehaviourHandle, 0x14) -- the ICE-anim cam handle (Construct zeroes
//                its five words: mbAllocated +0x180, muAllocationKey +0x184,
//                the owning helper-pool pointer +0x188, mpManager +0x18C, mpBehaviour +0x190)
//   * miRival   +0x194 (the rival index walked through the shot list; Update increments it
//                each frame the game moves to the next rival and indexes the shot list by it
//                modulo the shot count)
//   * meState   +0x198 (EState, the rank-up state machine value; the dispatch is indexed by
//                this value 0..3, default -> assert)
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the 4-byte-pointer offsets quoted
// above are provenance; on the x64 host the embedded Camera widens, so absolute offsets
// shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourIceAnim; }

    class ArbStateRankUp : public ArbitratorState
    {
    public:
        // EState -- the rank-up state machine. Construct seeds 0 (INACTIVE); Update's case-1
        // (PREPARING) success edge stores 2 (ACTIVE), and the dispatch is indexed by this
        // value (0..3; values >3 hit the default assert). The hand-back-to-roaming edge
        // stores 3 (CHANGING_TO_ROAMING).
        enum EState
        {
            E_STATE_INACTIVE            = 0,
            E_STATE_PREPARING           = 1,
            E_STATE_ACTIVE              = 2,
            E_STATE_CHANGING_TO_ROAMING = 3,
            E_STATE_RELEASING           = 4,

            E_NUM_STATES                = 5
        };

        // ---- ArbitratorState virtual overrides (vtable order; see base) ------------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;
        const char* GetName() const override;

        // Destruct() is NOT in this TU's function set (it keeps the base declaration; no
        // override added here).

    private:
        // RETIRED: this state used to carry its OWN nested five-word BehaviourHandle<> copy.
        // It now uses the SHARED BrnDirector::Camera::BehaviourHandle<TBehaviour>, which is
        // what the shipped build has -- ONE template instantiated per behaviour type, not a
        // per-state duplicate. Using the shared handle is therefore more faithful, and it is
        // what lets the bodied BehaviourManager::NewBehaviour<> overload bind here (the
        // generic THandle overload is declaration-only, so a nested fork compiles and then
        // leaves the behaviour unallocated at link time). The shared handle also identifies
        // the +0x08 word this file used to flag as "role not recovered": it is the owning
        // BehaviourHelper pool pointer, which a u32 would have truncated on this host. It
        // brings IsAllocated / GetBehaviour / IsWaitingToPrepare / Release with it, all
        // bodied, so this header no longer declares any of them.

        // ---- members, declaration order; console offsets in comments ---------------------
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mIceCam;   // +0x180  the rank-up ICE cam handle
        s32    miRival;                                              // +0x194  the rival index walked per take
        EState meState;                                              // +0x198  the rank-up state-machine state
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RANK_UP_H
