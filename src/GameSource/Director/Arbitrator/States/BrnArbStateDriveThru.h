#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_DRIVE_THRU_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_DRIVE_THRU_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState / ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"              // Camera::BehaviourHandle<>, Camera::BehaviourManager
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h" // Camera::BehaviourInterpolate (+ its Parameters)

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
//   * mDriveThruBehaviourHandle @+0x180 (Camera::BehaviourHandle, 0x14) -- shop-shot cam handle.
//   * mInterpolater             @+0x194 (Camera::BehaviourHandle, 0x14) -- an interpolation
//                                 handle the DWARF declares but this TU's recovered function set
//                                 never touches beyond zeroing it in Construct.
//   * mInterpolaterParams       @+0x1A8 (0x10)  -- Camera::BehaviourInterpolate::Parameters;
//                                 Construct runs its Construct() ({8, 0, SLERP, SINUSOIDAL}).
//   * meState                   @+0x1B8 (s32)   -- the drive-thru state machine value.
//   * mbIsReversed              @+0x1BC (bool)  -- the resolved bay-approach-direction flag.
// Parity is BY NAMED MEMBER (the project's x64-gate rule): the X360 4-byte-pointer offsets
// quoted above are provenance; on the x64 host the embedded Camera widens, so absolute
// offsets shift -- the member ROLES are what is reproduced. (Same BehaviourHandle subset +
// convention as BrnArbStateRoaming.h / BrnArbStatePostEvent.h / BrnArbStateCrashNav.h.)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
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
        // ---- members, DWARF order; X360 offsets in comments ------------------------------
        // ⛔⛔ ODR FORK RETIRED 2026-08-29 (the drive-thru camera wave). Until now this header
        // declared its OWN private `template <typename TBehaviour> struct BehaviourHandle` and a
        // private `struct InterpolateParameters { u32 mauParams[4]; }` -- a second, incompatible
        // spelling of two types that have real homes in
        // GameSource/Director/Camera/BrnBehaviourManager.h and
        // .../Behaviours/BrnBehaviourInterpolate.h. That fork is exactly why the .cpp reached its
        // own handle through two `extern "C" sub_821FCxxx` shims declared and never defined:
        // through the fork there was no GetProducedCamera()/GetBehaviour() to call, so the TU
        // could not link and stayed unmounted. Both console accessors are byte-identical to the
        // canonical ones (verified: sub_821FCE10 = pool slot + 0x10 == BehaviourHelper::mCamera,
        // sub_821FCDA8 = *(pool slot) == the live Behaviour*, both asserting "IsAllocated()" at
        // BrnBehaviourManager.h:589/:610) -- i.e. THIS WAS NEVER A MISSING FUNCTION, it was the
        // fork hiding the one we already have. Same defect class the crash-camera wave retired
        // from VehicleTracker (its private DirectorIO::InputBuffer fork), 2026-08-29.
        //
        // mDriveThruBehaviourHandle is allocated in Prepare via
        // BehaviourManager::NewBehaviour<Camera::Behaviour> (the attribute-taking overload) and
        // released in Release; mInterpolater is DWARF-declared but this TU's recovered function
        // set only zeroes it in Construct. mInterpolaterParams is the canonical
        // BehaviourInterpolate::Parameters -- Construct's four stores (+0x1A8 = 8, +0x1AC = 0,
        // +0x1B0 = 0, +0x1B4 = 1) ARE that type's own Construct() (mType 8 / SetDebugName(0) /
        // E_METHOD_SLERP / E_MAPPING_SINUSOIDAL), not four anonymous words.
        Camera::BehaviourHandle<Camera::Behaviour>            mDriveThruBehaviourHandle; // +0x180
        Camera::BehaviourHandle<Camera::BehaviourInterpolate> mInterpolater;             // +0x194
        Camera::BehaviourInterpolate::Parameters              mInterpolaterParams;       // +0x1A8
        EState                                                meState;                   // +0x1B8
        bool                                                  mbIsReversed;              // +0x1BC
    };
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_STATES_BRN_ARB_STATE_DRIVE_THRU_H
