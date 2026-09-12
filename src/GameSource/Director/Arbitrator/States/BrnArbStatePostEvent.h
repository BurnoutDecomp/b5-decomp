#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_POST_EVENT_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_POST_EVENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle

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
// LAYOUT: the member names and declaration order are attested for this build; the
// per-member offsets quoted below are pinned from Construct / Prepare / Update / Release:
//   * mCamera is the base ArbitratorState's by-value Camera at +0x10 (this state reaches it
//     by name through the base GetNonConstCamera(); Construct constructs it in place, Update
//     copies the behaviour's produced camera into it).
//   * mfTimeActive  @+0x180 (f32) -- declared member 0; NOT touched by this TU's recovered
//                    function set (Construct does not zero it, nothing reads it). Modelled
//                    at the slot preceding mbPlayedFlash; FLAG: its role is declared but
//                    unexercised here.
//   * mbPlayedFlash @+0x184 (bool) -- Construct zeroes it; Update's ACTIVE case reads/sets
//                    it to gate the one-shot "Car_Reset" flash effect.
//   * meState       @+0x188 (EState, the post-event state machine value)
//   * mPostEventCam @+0x18C (BehaviourHandle, 0x14) -- the ICE-anim cam handle
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute
// offsets shift -- the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace Attrib { namespace Gen { class shotgroup; } }

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; class BehaviourIceAnim; }

    class ArbStatePostEvent : public ArbitratorState
    {
    public:
        // EState -- the post-event state machine. Construct seeds 0 (INACTIVE); Prepare
        // forces 1 (PREPARING). Update's PREPARING success edge stores 2 (ACTIVE); the
        // hand-back-to-roaming edges run ChangeToState with blocked value 3
        // (CHANGING_TO_ROAMING). The dispatch table is indexed by this value (0..3).
        enum EState
        {
            E_STATE_INACTIVE            = 0,
            E_STATE_PREPARING           = 1,
            E_STATE_ACTIVE              = 2,
            E_STATE_CHANGING_TO_ROAMING = 3,
            E_STATE_RELEASING           = 4,

            E_NUM_STATES                = 5
        };

        // ---- ArbitratorState virtual overrides (vtable order; see base) -----------------
        void        Construct() override;
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override;
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override;
        const char* GetName() const override;

        // This state declares a Destruct() override so it keeps its own vtable slot, but the
        // console image carries NO separate body for it (no symbol, no call site: the whole
        // recovered function set for this state is Construct / GetName / PickAppropriateShot /
        // Prepare / Update / Release). FLAG: the body below is an empty one, matching the
        // "nothing to tear down" shape every other state with the same slot has -- it is NOT
        // a reconstruction of recovered code.
        void        Destruct() override;

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
        // It brings IsAllocated / GetBehaviour / GetProducedCamera / IsWaitingToPrepare /
        // Release with it, all bodied, so this header no longer declares any of them.

        // PickAppropriateShot -- choose the completion shot to play out of the event-completion
        // shot-group, given the live finish-line geometry and the player car's motion, and
        // return that ShotList element (the block fed to BehaviourIceAnim::SetParameters).
        // The return is the shot REFERENCE the behaviour consumes: Camera::ShotReference is
        // `const Attrib::RefSpec`, one ShotList element, which is exactly what the recovered
        // body hands back (the indexed element resolve, with the 24-byte default-data-area
        // element as the null fallback). Bodied in the .cpp.
        Camera::Camera::ShotReference& PickAppropriateShot(
            const Attrib::Gen::shotgroup& lrShotGroup, ArbStateSharedInfo& lrSharedInfo);

        // ---- members, declaration order; offsets in comments -----------------------------
        f32                    mfTimeActive;   // +0x180  (declared; unexercised here)
        bool                   mbPlayedFlash;  // +0x184  one-shot "Car_Reset" flash gate
        EState                 meState;        // +0x188  the post-event state-machine state
        Camera::BehaviourHandle<Camera::BehaviourIceAnim>
                               mPostEventCam;  // +0x18C  the completion-take cam handle
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_POST_EVENT_H
