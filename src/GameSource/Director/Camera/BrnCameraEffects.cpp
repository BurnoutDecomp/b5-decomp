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

    // ------------------------------------------------------------------------
    // BrnDirector::Camera::CameraEffects::Construct (destub wave 2026-07-26)
    //
    // Default-initialise the effects block. INLINED on the X360: both director-
    // camera bring-up paths (Camera::Construct @0x82255E68 and Camera::Clear
    // @0x8223CE70, the r11 = camera+0x68 store run) emit the same store set:
    // NUL both hook-name heads, zero the motion-blur block, zero the reserved
    // request bytes/words (+0x50/+0x78/+0x7C), zero the fade floats (+0x90/
    // +0x94/+0xA0), zero the lag/race-end/shake amounts (+0xA4/+0xA8/+0xAC),
    // zero the shake type + trailing flags (+0xB4/+0xB7..+0xBA), and set the
    // two 1.0 defaults: mfSimTimeScale (+0x9C) and mfShakeFrequency (+0xB0).
    // Store-for-store against the @0x8223CF98..0x8223CFF4 run.
    // ------------------------------------------------------------------------
    void CameraEffects::Construct()
    {
        mMotionBlurData.Construct();                    // stfs 0 @+0x44/+0x48, stb 0 @+0x4C/+0x4D
        maReserved78[0]           = 0;                  // stb 0 @+0x78
        maReserved50[0]           = 0;                  // stb 0 @+0x50
        maReserved84[0x90 - 0x84] = 0;                  // stfs 0 @+0x90 (zeroed float, byte-wise span)
        maReserved84[0x91 - 0x84] = 0;
        maReserved84[0x92 - 0x84] = 0;
        maReserved84[0x93 - 0x84] = 0;
        maReserved84[0x94 - 0x84] = 0;                  // stfs 0 @+0x94
        maReserved84[0x95 - 0x84] = 0;
        maReserved84[0x96 - 0x84] = 0;
        maReserved84[0x97 - 0x84] = 0;
        maReservedA0[0]           = 0;                  // stfs 0 @+0xA0
        maReservedA0[1]           = 0;
        maReservedA0[2]           = 0;
        maReservedA0[3]           = 0;
        mbSetTimeOfDay            = false;              // stb 0 @+0xB9 (carved from maReservedB9)
        mfCameraLag               = 0.0f;               // stfs 0 @+0xA4
        mbHasStartHookNameString  = false;              // stb 0 @+0xB7
        mfSimTimeScale            = 1.0f;               // stfs 1.0 @+0x9C
        mbHasStopHookNameString   = false;              // stb 0 @+0xB8
        mfShakeAmplitude          = 0.0f;               // stfs 0 @+0xAC
        maReservedBA[0]           = 0;                  // stb 0 @+0xBA
        mfShakeFrequency          = 1.0f;               // stfs 1.0 @+0xB0
        mStartHookNameString.mHookNameString[0] = '\0'; // stb 0 @+0x00
        mfRaceEndEffectAmount     = 0.0f;               // stfs 0 @+0xA8
        mStopHookNameString.mHookNameString[0]  = '\0'; // stb 0 @+0x21
        mu8ShakeType              = 0;                  // stb 0 @+0xB4
        muRequestedPostFxId       = 0;                  // stw 0 @+0x7C
    }
}
}
