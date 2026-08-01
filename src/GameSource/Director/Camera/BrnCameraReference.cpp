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

// ----------------------------------------------------------------------------
// @ 0x8223E990 -- cpp:~58. BODIED 2026-08-01. Adopt a live behaviour-helper slot as the
// camera source. Recovered from asm; the function is in the export set but UNNAMED
// (sub_8223E990) -- it was pinned by its own assert (BrnCameraReference.cpp:60) plus the
// helper-pool call and the Camera::operator= tail, and by its two call sites,
// BehaviourInterpolate::SetupCameraAFromHelper / ...BFromHelper.
//   0x8223E9A8  lwz  r11, 0x168(this)                ; assert(meType == E_TYPE_INVALID)  :60
//   0x8223E9E4  stw  2,   0x168(this)                ; meType = E_TYPE_BEHAVIOUR
//   0x8223E9E8  stw  idx, 0x164(this)                ; mBehaviourHelperIndex = lHelper
//   0x8223E9EC  bl   HelperPool::operator[](mgr + 0xFAB0, idx)   ; the pooled helper slot
//   0x8223E9FC  bl   Camera::operator=(this + 0, slot + 0x10)    ; mCamera = helper.GetCamera()
//
// ⭐ THE LAST STEP IS EASY TO MISS AND IS NOT OPTIONAL: the setter does not merely record
// where the camera comes from, it SNAPSHOTS the helper's camera as it stands at setup time.
// (The sibling BrnCameraReference.h comment calling the other two Setup overloads
// "not X360-exported" was stale -- both ARE in the image, just unnamed.)
// ----------------------------------------------------------------------------
void CameraReference::Setup(BehaviourHelperIndex lBehaviourHelperIndex,
                            const BehaviourManager* lpBehaviourController)
{
    CGS_ASSERT(meType == E_TYPE_INVALID, "meType == E_TYPE_INVALID");   // :60

    meType                = E_TYPE_BEHAVIOUR;
    mBehaviourHelperIndex = lBehaviourHelperIndex;

    mCamera = lpBehaviourController->GetCameraFromBehaviour(mBehaviourHelperIndex);
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
        // ⭐ SETTLED 2026-08-01. The X360 forms &wrapper->mICECamera.mCamera (+0x11D70,
        // `lis 1 / ori 0x1D70 / add.`) and null-checks the ADDRESS, so GetCamera()
        // returns a POINTER -- now confirmed by the DecFIGS DWARF
        // (ICEWrapper.hpp:198, `const Camera * GetCamera() const`) and by the accessor
        // having no console symbol at all (it is a header inline, bodied in
        // BrnDirectorICEWrapper.h). The const_cast that used to sit here only existed
        // to work around the old reference-returning, non-const declaration.
        const Camera* lpIceCamera = mpIceWrapper->GetCamera();
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
