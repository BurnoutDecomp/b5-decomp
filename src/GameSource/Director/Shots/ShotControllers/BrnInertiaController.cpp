#include "GameSource/Director/Shots/ShotControllers/BrnInertiaController.h"

#include "GameSource/Director/Camera/Camera.h"          // Camera::Camera / CameraEffects / CameraState
#include "rw/math/vpu/matrix44affine_operation.h"       // rw::math::vpu::SLerp

// BrnDirector::InertiaController -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Director/Shots/ShotControllers/BrnInertiaController.cpp):
//   InertiaController::Update @0x8221ECD0  (called by CameraFinaliser::Update)
//
// X360 asm walk:
//   ld camera+0x140 & bit-6 mask  -> the camera-state current flag 6 (the ld pulls the
//     whole 64-bit CameraState::mCurrentFlags field; the rlwinm 0x40 mask is BitArray
//     bit index 6): when set, reset miFrame to 0 (a camera cut restarts the inertia).
//   ++miFrame; on the FIRST frame just latch mPreviousActualXform = camera transform
//     (4 row copies) and return.
//   lfBlendAmount = 1.0 - camera+0x10C  (effects +0xA4 == CameraEffects::mfCameraLag);
//     when >= 1.0 (no lag requested) return WITHOUT touching the camera or the latch.
//   otherwise SLerp(out, mPreviousActualXform, camera transform, ...) by the splatted
//     blend amount (the same vendor op/convention as the reviewed BrnLooker /
//     OrientationLag sites; lfTimeStep rides into the vendor op in f1 -- see the
//     OrientationLag FLAG), store the blended rows into the camera (+ the inlined
//     SetTransform's ValidateTransformWithDebugInfo), then latch
//     mPreviousActualXform = the blended transform.

namespace BrnDirector
{
namespace
{
    // The camera-state flag whose set state restarts the inertia (BitArray index 6 of
    // CameraState::mCurrentFlags). FLAG: the producer-side name of flag 6 is not yet
    // recovered; its role here (a hard camera cut) is from this consumer.
    const u32 KU_CAMERA_FLAG_RESET_INERTIA = 6;
}

// @ 0x8221ECD0
void InertiaController::Update(Camera::Camera* lpCamera, f32 lfTimeStep)
{
    if (lpCamera->mState.IsFlagSet(KU_CAMERA_FLAG_RESET_INERTIA))
    {
        miFrame = 0;
    }

    ++miFrame;
    if (miFrame == 1)
    {
        // First frame after a reset: latch the actual transform, no smoothing yet.
        mPreviousActualXform = lpCamera->GetTransform();
        return;
    }

    const f32 lfBlendAmount = 1.0f - lpCamera->GetEffects().GetCameraLag();
    if (lfBlendAmount < 1.0f)
    {
        // Slerp last frame's actual transform toward the freshly-finalised one and
        // adopt the result (lfTimeStep feeds the vendor SLerp -- see header note).
        (void)lfTimeStep;
        const rw::math::vpu::Matrix44Affine lBlended =
            rw::math::vpu::SLerp(mPreviousActualXform, lpCamera->GetTransform(), &lfBlendAmount);

        lpCamera->SetTransform(lBlended);
        mPreviousActualXform = lBlended;
    }
}
}
