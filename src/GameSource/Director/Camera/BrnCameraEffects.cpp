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

    // ------------------------------------------------------------------------
    // BrnDirector::Camera::MotionBlurData::Set @0x8220AED8
    //
    // Store the two flags, then store each amount CLAMPED TO [0,1]. The whole body is 17
    // instructions and every one of them is accounted for:
    //   0x8220AED8  fneg  f13, f1                  ; -cars
    //   0x8220AEE0  fneg  f12, f2                  ; -world
    //   0x8220AEE4  stb   r4, 8(r3)                ; mbIsActive              <- arg1 (bool)
    //   0x8220AEE8  stb   r5, 9(r3)                ; mbIsExpensiveMotionBlur <- arg2 (bool)
    //   0x8220AEEC  lfs   f0, flt_82001CC0         ; 0.0f  (DATA_DUMP.md: flt_82001CC0 == 0.0f;
    //                                              ;        the same symbol BrnRendererModule.cpp
    //                                              ;        already cites for its clear colour)
    //   0x8220AEF4  fsel  f13, f13, f0, f1         ; cars  = (-cars  >= 0) ? 0.0f : cars   (lower clamp)
    //   0x8220AEF8  fsel  f12, f12, f0, f2         ; world = (-world >= 0) ? 0.0f : world
    //   0x8220AEFC  lfs   f0, flt_82001C98         ; 1.0f  (0x3F800000 -- the same constant
    //                                              ;        Camera.cpp already reads as 1.0f)
    //   0x8220AF00  fsubs f11, f0, f13             ; 1 - cars
    //   0x8220AF04  fsubs f10, f0, f12             ; 1 - world
    //   0x8220AF08  fsel  f13, f11, f13, f0        ; cars  = (1-cars  >= 0) ? cars  : 1.0f (upper clamp)
    //   0x8220AF0C  stfs  f13, 0(r3)               ; mfCarsBlurAmount
    //   0x8220AF10  fsel  f0,  f10, f12, f0        ; world = (1-world >= 0) ? world : 1.0f
    //   0x8220AF14  stfs  f0,  4(r3)               ; mfWorldBlurAmount
    // The store ORDER is the asm's: both bytes first, then cars, then world. Written as the
    // arithmetic the fsel pair expresses (the strength-reduction rule), NOT as fsel intrinsics --
    // the one behavioural difference is NaN: fsel takes the "else" arm on an unordered compare,
    // so a NaN amount would store 1.0f on the console and (with `>` / `<` here) also 1.0f, because
    // both comparisons against a NaN are false and the ternaries fall to the same arms.
    //
    // This is the SAME clamp the committed Camera::SetShowtimeBlurAndBars performs inline (its
    // caller re-materialises the sequence per lane); the difference is the argument order, which
    // the FLAG on that function already records. Set is the console's own canonical setter, and
    // its ONLY caller is EffectsModule::GenerateRenderRequests @0x8227FF10.
    // ------------------------------------------------------------------------
    void MotionBlurData::Set(bool lbIsActive, bool lbIsExpensiveMotionBlur,
                             f32 lfCarsBlurAmount, f32 lfWorldBlurAmount)
    {
        mbIsActive              = lbIsActive;                 // stb r4, 8(r3)
        mbIsExpensiveMotionBlur = lbIsExpensiveMotionBlur;    // stb r5, 9(r3)

        f32 lfCars  = (lfCarsBlurAmount  > 0.0f) ? lfCarsBlurAmount  : 0.0f;   // fsel on -cars
        f32 lfWorld = (lfWorldBlurAmount > 0.0f) ? lfWorldBlurAmount : 0.0f;   // fsel on -world
        lfCars  = (lfCars  < 1.0f) ? lfCars  : 1.0f;                           // fsel on 1-cars
        lfWorld = (lfWorld < 1.0f) ? lfWorld : 1.0f;                           // fsel on 1-world

        mfCarsBlurAmount  = lfCars;    // stfs f13, 0(r3)
        mfWorldBlurAmount = lfWorld;   // stfs f0,  4(r3)
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
        mfBloomThreshold          = 0.0f;               // stfs 0 @+0x90 (carved 2026-08-16 out of
        mfBloomLuminance          = 0.0f;               // stfs 0 @+0x94  the maReserved84 span)
        mfGameCameraBlend         = 0.0f;               // stfs 0 @+0xA0 (carved from maReservedA0)
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
