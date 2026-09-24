#include "GameSource/Director/MomentController/Moments/BrnMomentFailsafe.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnDirector::MomentFailSafe -- reconstructed from the console executable.
//
// Bodied here (3 ledger functions, home file
// GameSource/Director/MomentController/Moments/BrnMomentFailsafe.cpp):
//   MomentFailSafe::Construct
//   MomentFailSafe::Update
//   MomentFailSafe::GetName
//
// Construct (console-code walk): the inlined base Moment::Construct (meState = INACTIVE, meType
// latched through the live vtable's GetInstanceType, inhibit flag cleared, embedded
// camera constructed -- see BrnMoment.h), then mpParameters (this+0x180) = NULL.
//
// Update (console-code walk): assert mpParameters (no early-out), then switch on the
// moment state (this+0x174):
//   E_STATE_INVALID_SEARCHING (1) -> clear the switch-to gate (zero the +0x178 byte ==
//       mbCanSwitchToMeNow) and set bit 18 of the embedded camera's state-head
//       bookkeeping set (a read-modify-write of the 64-bit field at camera +0x138 --
//       CameraState::SetHeadFlag(18));
//   E_STATE_VALID (3)             -> nothing;
//   anything else                 -> assert "unhandled case in switch".
// The fail-safe never becomes valid on its own -- it just re-arms its camera request
// each searching frame.

namespace BrnDirector
{
namespace
{
    // The camera-state head bit the searching fail-safe raises each frame (role of the
    // head set not yet recovered -- see BrnCameraState.h).
    const u32 KU_CAMERA_HEAD_FLAG_FAILSAFE_REQUEST = 18;
}

void MomentFailSafe::Construct()
{
    Moment::Construct();   // inlined in the console build (state/type/inhibit/camera init)
    mpParameters = NULL;
}

void MomentFailSafe::Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo)
{
    (void)lfTimeStep;
    (void)lrBehaviourController;
    (void)lSharedInfo;

    CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_FAILSAFE_REQUEST);
        break;

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");
        break;
    }
}

const char* MomentFailSafe::GetName() const
{
    return "MomentFailSafe";
}
}

// ---- [FX-DIRECTOR 2026-09-24] the vtable one-liners the moment factory needs --------------------
// MomentController::NewMoment's AllocateVoid<MomentFailSafe> placement-constructs the moment, which emits its
// vftable, so every slot needs a body. Read off the console vftable off_8200748C (AllocateVoid<MomentFailSafe>
// @0x8222E9B0 stores it at +0) and the ICF-folded slot bodies it points at:
//   slot 1 Prepare          0x821F7560  meState = E_STATE_INVALID_SEARCHING, return true (the shared
//                                       body the export names MomentBystanderSeesAction::Prepare)
//   slot 4 Release          0x821F7560  the same body as slot 1
//   slot 5 Destruct         0x8284CB38  `blr` (the one empty body all twelve moments share)
//   slot 3 SetParameters    0x821F7670  `stw r4, 0x180(r3)` -- mpParameters
//   slot 7 GetInstanceType  0x824B5A18  `li r3, 6 ; blr` -- E_MOMENT_FAILSAFE
namespace BrnDirector
{
bool MomentFailSafe::Prepare(void* /*lrBehaviourController*/)
{
    SetState(E_STATE_INVALID_SEARCHING);   // li r10, 1 ; stw r10, 0x174(r3)
    return true;                           // li r3, 1
}

bool MomentFailSafe::Release()
{
    // Slot 4 of off_8200748C is 0x821F7560 -- the SAME body as slot 1 (the shared "searching"
    // Prepare): the fail-safe owns no behaviour, so releasing it only parks it at SEARCHING.
    SetState(E_STATE_INVALID_SEARCHING);   // li r10, 1 ; stw r10, 0x174(r3)
    return true;                           // li r3, 1
}

void MomentFailSafe::Destruct()
{
    // 0x8284CB38 is a lone `blr`: nothing to tear down.
}

void MomentFailSafe::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);   // stw r4, 0x180(r3)
}

Moment::EType MomentFailSafe::GetInstanceType()
{
    return E_MOMENT_FAILSAFE;   // li r3, 6
}
}
