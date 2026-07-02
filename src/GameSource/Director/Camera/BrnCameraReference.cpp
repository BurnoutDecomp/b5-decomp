#include "GameSource/Director/Camera/BrnCameraReference.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "GameSource/Director/BrnDirectorICEWrapper.h"     // BrnDirector::ICEWrapper (GetCamera @ +0x11D70)

// BrnDirector::Camera::CameraReference -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnCameraReference.cpp).
//
// Bodied here (4 ledger functions):
//   Setup(ICEWrapper) @0x821F8508   GetCamera @0x8223EA80
//   Prepare           @0x822523F0   Release   @0x822524A8
// All asserts are the X360's non-gating tripwires (the bodies fall through after
// firing, exactly as the console does).

namespace BrnDirector
{
namespace Camera
{

// @ 0x821F8508 -- cpp:93. Adopt an ICE movie wrapper as the camera source. The X360
// stores the wrapper/type BEFORE the null tripwire fires -- order preserved.
void CameraReference::Setup(const BrnDirector::ICEWrapper* lpIceWrapper)
{
    CGS_ASSERT(meType == E_TYPE_INVALID, "meType == E_TYPE_INVALID");   // :95

    mpIceWrapper = lpIceWrapper;
    meType       = E_TYPE_ICE;

    CGS_ASSERT(mpIceWrapper != 0, "mpIceWrapper != NULL");              // :99
}

// @ 0x8223EA80 -- cpp:172. Refresh mCamera from the referenced source and return it.
const Camera& CameraReference::GetCamera(const BehaviourManager* lpBehaviourController)
{
    CGS_ASSERT(IsValid(), "IsValid()");   // :174 (non-gating)

    switch (meType)
    {
    case E_TYPE_CACHED:
        break;   // mCamera already holds the cached camera

    case E_TYPE_BEHAVIOUR:
        CGS_ASSERT(mbBehaviourLocked, "mbBehaviourLocked");   // :187
        // X360: helper-pool slot lookup (manager+0xFAB0 pool, de-inlined
        // operator[]) + the slot's embedded camera at +0x10 == the committed
        // GetCameraFromBehaviour contract.
        mCamera = lpBehaviourController->GetCameraFromBehaviour(mBehaviourHelperIndex);
        break;

    case E_TYPE_ICE:
    {
        CGS_ASSERT(mpIceWrapper != 0, "mpIceWrapper != NULL");   // :194
        // The X360 forms &wrapper->mCamera (+0x11D70) and null-checks the ADDRESS --
        // the original GetCamera() evidently returned a pointer. The committed
        // ICEWrapper accessor is a non-const reference-returning method; taking its
        // address reproduces the checked value. (const_cast: the DWARF member is a
        // const wrapper pointer while the committed accessor is non-const per its
        // own asm cite.)
        const Camera* lpIceCamera =
            &const_cast<BrnDirector::ICEWrapper*>(mpIceWrapper)->GetCamera();
        CGS_ASSERT(lpIceCamera != 0, "mpIceWrapper->GetCamera() != NULL");   // :195
        mCamera = *lpIceCamera;
        break;
    }

    default:
        CGS_ASSERT(false, "this case shouldn't be reachable");   // :202
        break;
    }

    return mCamera;
}

// @ 0x822523F0 -- cpp:110. Lock the referenced behaviour for interpolation.
void CameraReference::Prepare(const BehaviourControllerLockInterface& lLockInterface)
{
    CGS_ASSERT(IsValid(), "IsValid()");   // :112 (non-gating)

    if (meType == E_TYPE_BEHAVIOUR)
    {
        CGS_ASSERT(mbBehaviourLocked == false, "mbBehaviourLocked == false");   // :116
        lLockInterface.LockBehaviourForInterpolation(mBehaviourHelperIndex);
        mbBehaviourLocked = true;
    }
}

// @ 0x822524A8 -- cpp:155. Unlock (if locked) and invalidate the reference.
void CameraReference::Release(const BehaviourControllerLockInterface& lLockInterface)
{
    CGS_ASSERT(IsValid(), "IsValid()");   // :157 (non-gating)

    if (meType == E_TYPE_BEHAVIOUR && mbBehaviourLocked)
    {
        lLockInterface.UnlockBehaviourForInterpolation(mBehaviourHelperIndex);
        mbBehaviourLocked = false;
    }
    meType = E_TYPE_INVALID;
}

}
}
