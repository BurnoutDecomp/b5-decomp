#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_DRIVE_THRU_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_DRIVE_THRU_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h
//
// BrnDirector::ArbStateDriveThru -- the director arbitrator state that runs the camera
// while the player is inside a drive-thru shop (Auto Parts / Body Shop / Gas Station /
// Tuning Shop / Tire Shop). Prepare resolves the shop-specific shot-group from the
// resource manager, works out whether the drive-thru approach is "reversed" (the player
// entered the drive-thru bay from the far end) from the bay/player transforms, and
// allocates a generic camera Behaviour to play the resolved shot. Update drives that
// behaviour, and once it reports finished plays the one-shot "Car_Reset" flash before
// handing control back to the roaming state. Derives from ArbitratorState (vtable order
// pinned by the base).
//
// LAYOUT: the member NAMES + DWARF declaration order come from the DecFIGS DWARF
// (BrnArbStateDriveThru.h:37/73-88). The per-member X360 offsets are pinned from the
// ARTIST asm (Construct @0x8225AE10, Prepare @0x8226E938, Update @0x82235DB0,
// Release @0x82235F00):
//   * mCamera is the base ArbitratorState's by-value Camera @+0x10 (this state reaches it
//     by name through the base GetNonConstCamera(); Construct calls Camera::Construct(
//     this+0x10)).
//   * mDriveThruBehaviourHandle @+0x180 (BehaviourHandle, 0x14) -- the shop-shot cam handle.
//   * mInterpolater             @+0x194 (BehaviourHandle, 0x14) -- an interpolation handle
//                                 the DWARF declares but this TU's recovered function set
//                                 never touches beyond zeroing it in Construct.
//   * mInterpolaterParams       @+0x1A8 (0x10)  -- Construct seeds {8, 0, 0, 1}; unread here.
//   * meState                   @+0x1B8 (s32)   -- the drive-thru state machine value.
//   * mbIsReversed              @+0x1BC (bool)  -- the resolved bay-approach-direction flag.
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute
// offsets shift -- the member ROLES are what is reproduced. (Same BehaviourHandle subset +
// convention as BrnArbStateRoaming.h / BrnArbStatePostEvent.h / BrnArbStateCrashNav.h.)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera { class Behaviour; class BehaviourInterpolate; }

    class ArbStateDriveThru : public ArbitratorState
    {
    public:
        // DWARF EState (BrnArbStateDriveThru.h:73). The drive-thru state machine. Construct
        // does not seed it explicitly (it is left at whatever the pool-allocation zero-fill
        // leaves; Prepare's guard is "meState != 0"); Prepare forces PREPARING; Update's
        // dispatch table is indexed 0..4 by this value.
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
        void        Construct() override;                              // @0x8225AE10
        bool        Prepare(ArbStateSharedInfo& lrSharedInfo) override; // @0x8226E938
        void        Update(ArbStateSharedInfo& lrSharedInfo) override;  // @0x82235DB0
        bool        Release(ArbStateSharedInfo& lrSharedInfo) override; // @0x82235F00
        const char* GetName() const override;                          // @0x821F6330

        // Destruct() is NOT in this TU's recovered X360 function set (the ledger's 5-function
        // postmortem set is Construct/GetName/Prepare/Release/Update); no override is added
        // here so it keeps the base's no-op body. FLAG: body not recovered for this state.

    private:
        // ---- a typed handle to a camera behaviour owned by the BehaviourManager ----------
        // Allocated in Prepare via BehaviourManager::NewBehaviour<Camera::Behaviour> and
        // released in Release via BehaviourManager::UnSetBehaviourUsedByHandle(mpManager,
        // muAllocationKey). 0x14-byte block (5 words) pinned from the Construct/Prepare/
        // Release asm: mbAllocated(+0x00), muAllocationKey(+0x04), a behaviour-lookup helper
        // word(+0x08), mpManager(+0x0C), mpBehaviour(+0x10). Same convention as the sibling
        // arbitrator states' own nested BehaviourHandle<> (BrnArbStateRoaming.h /
        // BrnArbStatePostEvent.h / BrnArbStateCrashNav.h -- each is its own distinct nested
        // type, left untouched here). FLAG: the +0x08 word's role is not fully recovered.
        template <typename TBehaviour>
        struct BehaviourHandle
        {
            BehaviourHandle()
                : mbAllocated(false), muAllocationKey(0), muHelperIndex(0),
                  mpManager(0), mpBehaviour(0) {}

            bool IsAllocated() const { return mbAllocated; }

            bool                      mbAllocated;     // +0x00
            u32                       muAllocationKey; // +0x04
            u32                       muHelperIndex;   // +0x08  FLAG: role not recovered (lookup helper)
            Camera::BehaviourManager* mpManager;       // +0x0C
            TBehaviour*               mpBehaviour;     // +0x10
        };

        // The DWARF-declared (but functionally unread by this TU's recovered bodies)
        // interpolation-curve parameter block (BrnArbStateDriveThru.h:86). Construct seeds
        // its four words {8, 0, 0, 1} (same shape as BrnArbStateRoaming's InterpolateParameters,
        // field roles not recovered).
        struct InterpolateParameters
        {
            u32 mauParams[4];   // +0x00..+0x0C
        };

        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        BehaviourHandle<Camera::Behaviour>            mDriveThruBehaviourHandle; // +0x180
        BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;             // +0x194
        InterpolateParameters                         mInterpolaterParams;       // +0x1A8
        EState                                        meState;                   // +0x1B8
        bool                                           mbIsReversed;              // +0x1BC
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_DRIVE_THRU_H
