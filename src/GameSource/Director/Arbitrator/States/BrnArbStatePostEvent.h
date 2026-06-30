#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_POST_EVENT_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_POST_EVENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h
//
// BrnDirector::ArbStatePostEvent -- the director arbitrator state that runs the camera
// "post-event" sequence once an event has finished (the results / winner reveal): it asks
// the resource manager for the event-completion shot-group, picks the appropriate
// completion shot for the live finish-line geometry, allocates an ICE-anim camera
// behaviour to play it, and walks the post-event state machine INACTIVE -> PREPARING ->
// ACTIVE -> CHANGING_TO_ROAMING, handing control back to the roaming state once the take
// has finished. Derives from ArbitratorState (vtable order pinned by the base).
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStatePostEvent.h, X360-attested for this build). The per-member X360 offsets are
// pinned from the ARTIST asm (Construct @0x8225AD60, Prepare @0x8226E228,
// Update @0x82235AA8, Release @0x82235CF0):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it
//     by name through the base GetNonConstCamera(); Construct calls Camera::Construct(
//     this+0x10), Update copies the behaviour's produced camera into it).
//   * mfTimeActive  @+0x180 (f32) -- DWARF member 0; NOT touched by this TU's recovered
//                    function set (Construct does not zero it, nothing reads it). Modelled
//                    at the slot preceding mbPlayedFlash; FLAG: its role is declared by
//                    DWARF but unexercised here.
//   * mbPlayedFlash @+0x184 (bool) -- Construct zeroes it (stb 0,+0x184); Update case-2
//                    reads/sets it to gate the one-shot "Car_Reset" flash effect.
//   * meState       @+0x188 (EState, the post-event state machine value)
//   * mPostEventCam @+0x18C (BehaviourHandle, 0x14) -- the ICE-anim cam handle
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute
// offsets shift -- the member ROLES are what is reproduced. (Same BehaviourHandle subset +
// convention as BrnArbStateRaceIntro.h.)
// ----------------------------------------------------------------------------

namespace Attrib { namespace Gen { class shotgroup; class iceanim; } }

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; class BehaviourIceAnim; }

    class ArbStatePostEvent : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStatePostEvent.h:72). The post-event state machine. Construct
        // seeds 0 (INACTIVE); Prepare forces 1 (PREPARING). Update's case-1 success edge
        // stores 2 (ACTIVE); the hand-back-to-roaming edges run ChangeToState with blocked
        // value 3 (CHANGING_TO_ROAMING). The dispatch table is indexed by this value (0..3).
        // Values are the X360 jump-table case indices / the immediates the asm stores into
        // meState.
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
        void        Construct() override;                              // @0x8225AD60
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x8226E228
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x82235AA8
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x82235CF0
        const char* GetName() const override;                          // @0x821F6310

        // Destruct() IS in this TU's DWARF function set (BrnArbStatePostEvent.h:308 declares a
        // virtual override) but NO asm body was recovered for it (it is not in the ledger's
        // 6-function postmortem set, which is Construct/GetName/PickAppropriateShot/Prepare/
        // Release/Update). DECLARATION-ONLY: the override is declared to keep the X360 vtable
        // slot, but the body is left to the base / the unrecovered TU (the per-TU cl /c gate
        // does not link it). FLAG: body not recovered.
        void        Destruct() override;                               // @0x???????? (no asm)

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Allocated in Prepare via BehaviourManager::NewBehaviour<BehaviourIceAnim> and
        // released in Release via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Prepare/
        // Update/Release asm: mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup
        // helper word(+0x08), mpManager(+0x0C), mpBehaviour(+0x10). The X360 re-resolves the
        // live behaviour from the manager pool through (helper word, allocation key);
        // GetBehaviour() returns the cached pointer after asserting IsAllocated().
        // GetProducedCamera() returns the camera the live behaviour produced this frame (the
        // manager keeps it alongside the behaviour pointer in the same pool slot -- the X360
        // reads it at slot+0x10). FLAG: the +0x08 word's role is not fully recovered (modelled
        // as an opaque behaviour-lookup helper index, as in BrnArbStateRaceIntro.h).
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
            // Asserts IsAllocated(). Defined out-of-line in the .cpp where BehaviourIceAnim is
            // complete.
            const Camera::Camera& GetProducedCamera() const;

            // Whether the behaviour this handle owns is ready to use (no longer queued for its
            // first Prepare). Prepare returns this. The X360 asserts IsAllocated() then returns
            // !mpManager->IsBehaviourWaitingToPrepare(muAllocationKey). Defined out-of-line in
            // the .cpp where BehaviourManager is complete (same pattern as BrnArbStateRankUp).
            bool IsBehaviourReadyToUse() const;

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // PickAppropriateShot @0x8221A080 -- choose the completion shot to play from the
        // event-completion shot-group, given the live finish-line geometry, and return its
        // ICE-anim parameter block. DWARF (BrnArbStatePostEvent.h:171) types the return as
        // Camera::ShotReference&; that resolves to BehaviourIceAnim::ShotReference ==
        // Attrib::Gen::iceanim (the block fed to BehaviourIceAnim::SetParameters), so the
        // return is spelled Attrib::Gen::iceanim& here (BehaviourIceAnim is only forward-
        // declared at this point, so its nested typedef is not nameable). DECLARATION-ONLY --
        // see the FLAG in the .cpp: the X360 body's shot-selection is a multi-stage VMX
        // pipeline (vmsum3fp128 finish-line dot products + Camera::Utils::CreateLookAt), and
        // the per-vector field roles are not recovered; reconstructing it would require
        // fabricating a per-axis scalar formula, which the rules forbid. The declaration is
        // kept so Prepare can name the call (the per-TU cl /c gate does not link the body).
        Attrib::Gen::iceanim& PickAppropriateShot(const Attrib::Gen::shotgroup& lrShotGroup,
                                                  ArbStateSharedInfo& lrSharedInfo);

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        f32                                       mfTimeActive;   // +0x180  (DWARF :83; unexercised here)
        bool                                      mbPlayedFlash;  // +0x184  one-shot "Car_Reset" flash gate
        EState                                    meState;        // +0x188  the post-event state-machine state
        BehaviourHandle<Camera::BehaviourIceAnim> mPostEventCam;  // +0x18C  the completion-take cam handle
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_POST_EVENT_H
