#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RACE_INTRO_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RACE_INTRO_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"            // Attrib::Gen::shotgroup (mShotGroup by value)

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h
//
// BrnDirector::ArbStateRaceIntro -- the director arbitrator state that runs the camera
// "race intro" sequence at the start of an event: it picks the event's intro shot-group
// (the ordered list of ICE-anim camera takes), allocates an ICE-anim camera behaviour to
// play it, and walks the intro state machine INACTIVE -> PREPARING ->
// ACTIVE_PRE_COUNTDOWN -> ACTIVE_COUNTDOWN -> CHANGING_TO_ROAMING, handing control back
// to the roaming state once the take has finished (or when a freeburn-join aborts it).
// Derives from ArbitratorState (vtable order pinned by the base).
//
// LAYOUT: the member names and declaration order are attested for this build; the
// per-member offsets quoted below are pinned from Construct / Prepare / Update / Release:
//   * mCamera is the base ArbitratorState's by-value Camera at +0x10 (this state reaches it
//     by name through the base GetNonConstCamera() / the effect-trigger free functions;
//     Construct/Update construct it in place).
//   * mRaceIntroBehaviourHandle @+0x180 (BehaviourHandle, 0x14) -- the ICE-anim cam handle
//   * meState                   @+0x194 (EState, the intro state machine value)
//   * mShotGroup                @+0x198 (Attrib::Gen::shotgroup, an Attrib::Instance, 0x10)
//   * mpRaceStartShotGroup      @+0x1A8 (const shotgroup*, points at the event's intro
//                                shots: either the resource-manager-owned default group or
//                                &mShotGroup when an event-specific group id is set)
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera / shotgroup widen, so
// absolute offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; class BehaviourIceAnim; }

    class ArbStateRaceIntro : public ArbitratorState
    {
    public:
        // EState -- the race-intro state machine. Construct
        // seeds 0 (INACTIVE); Prepare forces 1 (PREPARING). Update's case-1 success edge
        // stores 2 (ACTIVE_PRE_COUNTDOWN); the COUNTDOWN edge stores 3; the hand-back-to-
        // roaming edges run ChangeToState with blocked value 4 (CHANGING_TO_ROAMING). The
        // dispatch table is indexed by this value (0..4).
        enum EState
        {
            E_STATE_INACTIVE             = 0,
            E_STATE_PREPARING            = 1,
            E_STATE_ACTIVE_PRE_COUNTDOWN = 2,
            E_STATE_ACTIVE_COUNTDOWN     = 3,
            E_STATE_CHANGING_TO_ROAMING  = 4,
            E_STATE_RELEASING            = 5,

            E_NUM_STATES                 = 6
        };

        // ---- ArbitratorState virtual overrides (vtable order; see base) -----------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;
        const char* GetName() const override;

        // Destruct() is not in this TU's function set (it keeps the base declaration; no
        // override added here).

    private:
        // RETIRED: this state used to carry its OWN nested five-word BehaviourHandle<> copy.
        // It now uses the SHARED BrnDirector::Camera::BehaviourHandle<TBehaviour>, which is
        // what the console has -- ONE template instantiated per behaviour type, not a
        // per-state duplicate. Using the shared handle is therefore more faithful, and it is
        // what lets the bodied BehaviourManager::NewBehaviour<> overload bind here (the
        // generic THandle overload is declaration-only, so a nested fork compiles and then
        // leaves the behaviour unallocated at link time). The shared handle also identifies
        // the +0x08 word this file used to flag as "role not recovered": it is the owning
        // BehaviourHelper pool pointer, which a u32 would have truncated on this host.
        // It brings IsAllocated / GetBehaviour / GetProducedCamera / Release with it, all
        // bodied, so this header no longer declares any of them.

        // ---- members, declaration order; offsets in comments -----------------------------
        Camera::BehaviourHandle<Camera::BehaviourIceAnim> mRaceIntroBehaviourHandle; // +0x180
        EState                  meState;             // +0x194  the intro state-machine state
        Attrib::Gen::shotgroup  mShotGroup;          // +0x198  the event-specific shot group (built in Prepare)
        const Attrib::Gen::shotgroup* mpRaceStartShotGroup; // +0x1A8  the intro shots in force
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RACE_INTRO_H
