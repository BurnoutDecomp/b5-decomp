#include "types.hpp"
#include "GameSource/Director/Camera/BrnCameraEffects.h"
#include "SharedClasses/Graphics/BrnEffectsData.h"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnDirector::Camera::MotionBlurData::Construct    @ 0x821F84E8
//   BrnDirector::Camera::MotionBlurData::Interpolate  @ 0x8220AF20
//   BrnDirector::Camera::CameraEffects::Interpolate   @ 0x8220B050
//
// CameraEffects itself is the canonical, X360-size-proven (0xBC) type declared in
// BrnCameraEffects.h (this TU's own header) and embedded by value in Camera::mEffects
// (see Camera.h / Camera.cpp's static_assert(sizeof(CameraEffects) == 0xBC)). Only
// Interpolate's BODY lands here; the type is not redefined in this TU.

namespace BrnDirector
{
namespace Camera
{
    static f32 Lerp(f32 lfStart, f32 lfEnd, f32 lfT)
    {
        return lfStart + ((lfEnd - lfStart) * lfT);
    }

    // MotionBlurData is the shared type from SharedClasses/Graphics/BrnEffectsData.h
    // (#included above), not a local fork.

    void MotionBlurData::Construct()
    {
        mfCarsBlurAmount = 0.0f;
        mfWorldBlurAmount = 0.0f;
        mbIsActive = false;
        mbIsExpensiveMotionBlur = false;
        mPadA[0] = 0;
        mPadA[1] = 0;
    }

    MotionBlurData MotionBlurData::Interpolate(const MotionBlurData& lLhs, const MotionBlurData& lRhs, f32 lfT)
    {
        MotionBlurData lResult;

        if (!lLhs.mbIsActive && !lRhs.mbIsActive)
        {
            return lLhs;
        }

        if (!lLhs.mbIsActive && lRhs.mbIsActive)
        {
            lResult = lRhs;
            lResult.mfCarsBlurAmount = lRhs.mfCarsBlurAmount * lfT;
            lResult.mfWorldBlurAmount = lRhs.mfWorldBlurAmount * lfT;
            return lResult;
        }

        if (lLhs.mbIsActive && !lRhs.mbIsActive)
        {
            lResult = lLhs;
            lResult.mfCarsBlurAmount = Lerp(lLhs.mfCarsBlurAmount, 0.0f, lfT);
            lResult.mfWorldBlurAmount = Lerp(lLhs.mfWorldBlurAmount, 0.0f, lfT);
            return lResult;
        }

        lResult = lLhs;
        lResult.mfCarsBlurAmount = Lerp(lLhs.mfCarsBlurAmount, lRhs.mfCarsBlurAmount, lfT);
        lResult.mfWorldBlurAmount = Lerp(lLhs.mfWorldBlurAmount, lRhs.mfWorldBlurAmount, lfT);
        lResult.mbIsExpensiveMotionBlur = lRhs.mbIsExpensiveMotionBlur;
        return lResult;
    }

    CameraEffects CameraEffects::Interpolate(const CameraEffects& lLhs, const CameraEffects& lRhs, f32 lfT)
    {
        CameraEffects lResult;
        std::memcpy(&lResult, &lLhs, sizeof(lResult));

        lResult.mfSimTimeScale =
            Lerp(lLhs.mfSimTimeScale, lRhs.mfSimTimeScale, lfT);
        lResult.mfRaceEndEffectAmount =
            Lerp(lLhs.mfRaceEndEffectAmount, lRhs.mfRaceEndEffectAmount, lfT);
        lResult.mMotionBlurData =
            MotionBlurData::Interpolate(lLhs.mMotionBlurData, lRhs.mMotionBlurData, lfT);

        return lResult;
    }
}
}
