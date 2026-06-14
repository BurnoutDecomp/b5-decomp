#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnDirector::Camera::MotionBlurData::Construct    @ 0x821F84E8
//   BrnDirector::Camera::MotionBlurData::Interpolate  @ 0x8220AF20
//   BrnDirector::Camera::CameraEffects::Interpolate   @ 0x8220B050

namespace BrnDirector
{
namespace Camera
{
    static f32 Lerp(f32 lfStart, f32 lfEnd, f32 lfT)
    {
        return lfStart + ((lfEnd - lfStart) * lfT);
    }

    struct MotionBlurData
    {
        void Construct();
        static MotionBlurData Interpolate(const MotionBlurData& lLhs, const MotionBlurData& lRhs, f32 lfT);

        f32 mfCarsBlurAmount;
        f32 mfWorldBlurAmount;
        bool mbIsActive;
        bool mbIsExpensiveMotionBlur;
        u8 maPad10[2];
    };

    struct HookNameStringWrapper
    {
        u8 maStorage[34];
    };

    struct BackgroundEffectRequest
    {
        u8 maStorage[72];
    };

    struct CameraEffects
    {
        static CameraEffects Interpolate(const CameraEffects& lLhs, const CameraEffects& lRhs, f32 lfT);

        HookNameStringWrapper mStartHookNameStringWrapper;
        HookNameStringWrapper mStopHookNameStringWrapper;
        MotionBlurData mMotionBlurData;
        BackgroundEffectRequest mBackgroundEffectRequest;
        u32 muRequestedPostFX;
        f32 mfStartHookBlendAmount;
        u32 muFadeColor;
        s32 meOverlay;
        f32 mfRaceEndEffectAmount;
        f32 mfBloomThreshold;
        f32 mfBloomLuminance;
        f32 mfTimeOfDay;
        f32 mfSimTimeScale;
        f32 mfGameCameraBlend;
        f32 mfCameraLag;
        f32 mfBlackBarAmount;
        f32 mfShakeAmplitude;
        f32 mfShakeFrequency;
        u8 mu8ShakeType;
        u8 mu8GameCameraBlendCurve;
        u8 mu8GameCameraBlendMethod;
        bool mbHasStartHookNameString;
        bool mbHasStopHookNameString;
        bool mbSetTimeOfDay;
        bool mbRequestingScreenshot;
        u8 maPad187;
    };

    void MotionBlurData::Construct()
    {
        mfCarsBlurAmount = 0.0f;
        mfWorldBlurAmount = 0.0f;
        mbIsActive = false;
        mbIsExpensiveMotionBlur = false;
        maPad10[0] = 0;
        maPad10[1] = 0;
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

        lResult.mfStartHookBlendAmount =
            Lerp(lLhs.mfStartHookBlendAmount, lRhs.mfStartHookBlendAmount, lfT);
        lResult.mfRaceEndEffectAmount =
            Lerp(lLhs.mfRaceEndEffectAmount, lRhs.mfRaceEndEffectAmount, lfT);
        lResult.mMotionBlurData =
            MotionBlurData::Interpolate(lLhs.mMotionBlurData, lRhs.mMotionBlurData, lfT);

        return lResult;
    }
}
}
