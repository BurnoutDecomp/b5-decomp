#include "GameSource/Director/Camera/Utils/BrnCamera2DRotationController.h"

#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // GetSmallestDifferenceBetweenDegsAngles
#include "rw/math/fpu/scalar_operation.h"                   // rw::math::fpu::Clamp

#include <cmath>

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // @ 0x8220BFE0
    // Fold this frame's 2D look-stick input into the rotation controller: age the
    // rotation timer, latch the look-back edge, and drive mfRotationAngleDegs toward
    // the stick heading (or back toward centre once the stick is released past its
    // hold time). Store-for-store from BURNOUT_X360_ARTIST.XEX.
    void Camera2DRotationController::Update(f32 lfTimestep, Vector2 lStick, bool lbLookback, bool lbPaused)
    {
        // Age the rotation timer and latch the look-back state for edge detection.
        mfTimeSinceRotation += lfTimestep;
        mbIsLookbackLastFrame = mbIsLookback;
        mbIsLookback          = lbLookback;

        // Dead-zone test: the stick counts as "pushed" once its magnitude reaches the
        // dead-zone radius (kfDeadZoneRadius == 0.5, flt_82001DA0). The console works
        // over the x/y lanes only (the packed scratch vector is {0.5, 0}).
        const f32 lfStickMagnitude =
            std::sqrt(lStick.x * lStick.x + lStick.y * lStick.y);

        if (lfStickMagnitude >= kfDeadZoneRadius)
        {
            // Entering (or holding) a rotation: reset the hold timer on the leading edge.
            if (!mbIsRotated)
            {
                mfTimeSinceRotation = 0.0f;
                mbIsRotated         = true;
            }

            // Snapshot the normalised stick direction (rsqrt-Newton normalise on console).
            const f32 lfInvMagnitude = 1.0f / lfStickMagnitude;
            mStickVector.x = lStick.x * lfInvMagnitude;
            mStickVector.y = lStick.y * lfInvMagnitude;
            mStickVector.z = 0.0f;
            mStickVector.w = 0.0f;
        }
        else if (mbIsRotated && !lbPaused && mfTimeSinceRotation > mfMinRotationTime)
        {
            // Stick released past the minimum-hold time: drop the rotation and clear
            // the held direction; the angle then eases back to centre next frames.
            mbIsRotated = false;
            mStickVector.SetZero();
            return;
        }

        // Target angle for this frame (degrees). Zero (centre) unless actively rotated.
        f32 lfTargetAngleDegs = 0.0f;
        if (mbIsRotated)
        {
            // Angle of the held stick direction away from straight-up (0,1). Dot with
            // {0,1} == mStickVector.y.
            const f32 lfDot = rw::math::fpu::Clamp(mStickVector.y, -1.0f, 1.0f);
            lfTargetAngleDegs = static_cast<f32>(std::acos(lfDot)) * 57.29578f;
            // Negate when the stick is pushed to the LEFT (x lane below zero). The
            // console test is vcmpgtfp. 0.0 > mStickVector.x, i.e. mStickVector.x < 0.
            if (mStickVector.x < 0.0f)
            {
                lfTargetAngleDegs = lfTargetAngleDegs * -1.0f;
            }
        }

        // Blend the stored angle toward the target along the shortest arc. Use the
        // active blend factor while rotated, the return blend factor while easing back.
        const f32 lfDelta =
            GetSmallestDifferenceBetweenDegsAngles(mfRotationAngleDegs, lfTargetAngleDegs);
        const f32 lfBlendFactor = mbIsRotated ? mfRotationBlendFactor : mfRotationReturnBlendFactor;
        mfRotationAngleDegs = lfDelta * lfBlendFactor + mfRotationAngleDegs;

        // Wrap the accumulated angle back into (-180, 180].
        mfRotationAngleDegs = GetSmallestDifferenceBetweenDegsAngles(0.0f, mfRotationAngleDegs);
    }
}
}
}
