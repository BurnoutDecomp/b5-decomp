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
