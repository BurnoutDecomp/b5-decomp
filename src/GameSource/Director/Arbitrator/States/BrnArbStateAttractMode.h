#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ATTRACT_MODE_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ATTRACT_MODE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (handle IsAllocated check)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

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
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x822361F0
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x82236320
        const char* GetName() const override;                          // @0x821F6700

        // Prepare() / Destruct() are NOT in this TU's X360 function set (Prepare is reached as a
        // virtual call -- (*(*this+4))(this, info) -- from case 1 of Update, so its body lives in
        // another driver TU; the base declaration is kept, no override added here).

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Released in Release() via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Release/Update
        // asm: mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup helper word(+0x08),
        // mpManager(+0x0C), mpBehaviour(+0x10). Mirrors ArbStateRoaming::BehaviourHandle /
        // ArbStateCrashNav::BehaviourHandle (each state keeps its own nested copy of this same
        // five-word layout; the shared BrnBehaviourManager.h BehaviourHandle<T> is a distinct
        // type, left untouched).
        // FLAG: the +0x08 word's exact role is not fully recovered (never read in this TU;
        // modelled as an opaque behaviour-lookup helper index, as in the sibling states).
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool IsAllocated() const { return mbAllocated; }

            // The live behaviour this handle owns (only valid while IsAllocated()).
            TBehaviour* GetBehaviour() const
            {
                CGS_ASSERT(mbAllocated, "IsAllocated()");
                return mpBehaviour;
            }

            // The camera the live behaviour produced this frame. The X360 reaches it through the
            // handle helper sub_821FDC58 (unattested body); modelled BY NAME as the behaviour's
            // produced camera, same as ArbStateCrashNav::BehaviourHandle::GetProducedCamera().
            // Defined out-of-line in the .cpp where BehaviourRoadRunner is complete.
            const Camera::Camera& GetProducedCamera() const;

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::BehaviourRoadRunner> mRoadRunnerCam;  // X360 +0x180
        EState                                       meState;         // X360 +0x194
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_ATTRACT_MODE_H
