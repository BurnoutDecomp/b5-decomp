#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RACE_INTRO_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RACE_INTRO_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
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
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateRaceIntro.h, X360-attested for this build). The per-member X360 offsets are
// pinned from the ARTIST asm (Construct @0x8225ADB8, Prepare @0x8226E350,
// Update @0x8226E5B0, Release @0x82235D50):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it
//     by name through the base GetNonConstCamera() / the effect-trigger free functions;
//     Construct/Update call Camera::Construct(this+0x10)).
//   * mRaceIntroBehaviourHandle @+0x180 (BehaviourHandle, 0x14) -- the ICE-anim cam handle
//   * meState                   @+0x194 (EState, the intro state machine value)
//   * mShotGroup                @+0x198 (Attrib::Gen::shotgroup, an Attrib::Instance, 0x10)
//   * mpRaceStartShotGroup      @+0x1A8 (const shotgroup*, points at the event's intro
//                                shots: either the resource-manager-owned default group or
//                                &mShotGroup when an event-specific group id is set)
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera / shotgroup widen, so
// absolute offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; class BehaviourIceAnim; }

    class ArbStateRaceIntro : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateRaceIntro.h:68). The race-intro state machine. Construct
        // seeds 0 (INACTIVE); Prepare forces 1 (PREPARING). Update's case-1 success edge
        // stores 2 (ACTIVE_PRE_COUNTDOWN); the COUNTDOWN edge stores 3; the hand-back-to-
        // roaming edges run ChangeToState with blocked value 4 (CHANGING_TO_ROAMING). The
        // dispatch table is indexed by this value (0..4). Values are the X360 jump-table
        // case indices / the immediates the asm stores into meState.
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

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                            // @0x8225ADB8
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x8226E350
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x8226E5B0
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x82235D50
        const char* GetName() const override;                        // @0x821F6320

        // Destruct() is NOT in this TU's X360 function set (it keeps the base declaration; no
        // override added here).

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Allocated in Prepare/Update via BehaviourManager::NewBehaviour<BehaviourIceAnim>
        // and released in Update/Release via BehaviourManager::UnSetBehaviourUsedByHandle(
        // mpManager, muAllocationKey). 0x14-byte block (5 words) pinned from the
        // Construct/Prepare/Update/Release asm: mbAllocated(+0x00), muAllocationKey(+0x04), a
        // behaviour-lookup helper word(+0x08), mpManager(+0x0C), mpBehaviour(+0x10). The X360
        // re-resolves the live behaviour from the manager pool through (helper word,
        // allocation key); GetBehaviour() returns the cached pointer to that same behaviour
        // after asserting IsAllocated(). GetProducedCamera() returns the camera the live
        // behaviour produced this frame (the manager keeps it alongside the behaviour pointer
        // in the same pool slot -- the X360 reads it at slot+0x10). FLAG: the +0x08 word's
        // role is not fully recovered in this TU (modelled as an opaque behaviour-lookup
        // helper index, as in BrnArbStateRoaming.h / BrnArbStateCrashMode.h).
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

            // The camera the live behaviour produced this frame. The X360 reads it from the
            // manager pool slot the handle resolves to (sub_821FD450 returns slot+0x10;
            // BrnBehaviourManager.h:610), which is the same camera the ICE-anim behaviour
            // writes each frame -- modelled by NAME as the behaviour's produced camera.
            // Asserts IsAllocated() (the X360 asserts it inside the pool lookup). Defined
            // out-of-line in the .cpp where BehaviourIceAnim is complete.
            const Camera::Camera& GetProducedCamera() const;

            // Drop the manager-side hold and clear the handle (sub_8222DFD8): when allocated,
            // UnSetBehaviourUsedByHandle(mpManager, muAllocationKey) then zero the slots.
            // Always returns true (the X360 returns 1).
            bool Release();

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourIceAnim> mRaceIntroBehaviourHandle; // X360 +0x180
        EState                  meState;             // +0x194  the intro state-machine state
        Attrib::Gen::shotgroup  mShotGroup;          // +0x198  the event-specific shot group (built in Prepare)
        const Attrib::Gen::shotgroup* mpRaceStartShotGroup; // +0x1A8  the intro shots in force
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_RACE_INTRO_H
