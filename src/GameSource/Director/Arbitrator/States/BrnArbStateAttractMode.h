#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ATTRACT_MODE_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ATTRACT_MODE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<> (the SHARED handle)

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateAttractMode.h
//
// BrnDirector::ArbStateAttractMode -- the director arbitrator state that runs the front-end
// "attract mode" road-runner fly-by (the demo camera shown when the game is idle at the front
// end). It drives a single road-runner camera behaviour, copies its produced camera into the
// state's own camera every frame, and -- once the destination roaming state accepts a Prepare --
// hands control over to ArbStateRoaming and releases itself. Derives from ArbitratorState
// (vtable order pinned by the base).
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateAttractMode.h, X360-attested for this build: only mRoadRunnerCam and meState are
// listed). The per-member X360 offsets are pinned from the ARTIST asm (Construct @0x8225B1C8,
// Update @0x822361F0, Release @0x82236320):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (Construct calls
//     Camera::Construct(this+0x10); Update copies the road-runner's produced camera into it).
//   * mRoadRunnerCam @+0x180 (BehaviourHandle, 0x14) -- the fly-by cam handle Update drives and
//     Release tears down (mirrors ArbStateRoaming::mRoadRunnerCam / ArbStateCrashNav::mRoadRunnerCam).
//   * meState @+0x194 (EState, the state-machine dispatch key Update switches on).
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets quoted
// above are provenance; on the x64 host the embedded Camera widens, so absolute offsets shift --
// the member ROLES are what is reproduced.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // The road-runner fly-by behaviour this state drives (full type/handle accessors live in
    // GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h); referenced only as the
    // BehaviourHandle template argument here.
    namespace Camera { class BehaviourManager; class BehaviourRoadRunner; }

    class ArbStateAttractMode : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateAttractMode.h:66). The attract-mode state machine: idle,
        // preparing the fly-by, actively driving it, handing off to roaming, and releasing.
        // Values are the X360 immediates stored into meState / the switch dispatch indices.
        enum EState
        {
            E_STATE_INACTIVE             = 0,
            E_STATE_PREPARING            = 1,
            E_STATE_ACTIVE                = 2,
            E_STATE_CHANGING_TO_ROAMING  = 3,
            E_STATE_RELEASING            = 4,

            E_NUM_STATES                 = 5
        };

        // ---- ArbitratorState virtual overrides (X360 vtable order; see base) -------------
        void        Construct() override;                              // @0x8225B1C8
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x8225B220
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x822361F0
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x82236320
        const char* GetName() const override;                          // @0x821F6700

        // Destruct() is NOT in this TU's X360 function set; the base declaration is kept, no
        // override added here.
        //
        // Prepare @0x8225B220 was previously recorded as "not in this TU's function set"
        // because Update reaches it as the virtual `(*(*this+4))(this, info)`. It IS in the
        // export set under its own symbol and is overridden here -- Arbitrator::Update's
        // CHANGING_TO_ATTRACT_MODE case calls it directly, and its return value is the gate
        // that decides whether the fly-by goes live this frame.

    private:
        // ⭐ RETIRED (2026-07-29): this state used to carry its OWN nested five-word
        // BehaviourHandle<> copy. It now uses the SHARED
        // BrnDirector::Camera::BehaviourHandle<TBehaviour> -- which is what the console has:
        // the X360 symbol is
        //     ??$NewBehaviour@VBehaviourRoadRunner@...@BehaviourManager@...QAAXAAV?$BehaviourHandle
        // i.e. ONE template instantiated per behaviour type, not a per-state duplicate. Using
        // the shared handle is therefore MORE faithful, and it is what lets the bodied
        // BehaviourManager::NewBehaviour<> overload bind here (the generic THandle overload
        // stays declaration-only for the states that still carry nested copies).
        // The shared handle brings IsWaitingToPrepare / IsReadyToPrepare / GetProducedCamera /
        // Release with it, all now bodied -- so this header no longer declares any of them.

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        Camera::BehaviourHandle<Camera::BehaviourRoadRunner> mRoadRunnerCam;  // X360 +0x180
        EState                                       meState;         // X360 +0x194
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ATTRACT_MODE_H
