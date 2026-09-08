#pragma once

// FLAG PC-platform leaf: snapshot the original EffectsDebugComponent controls at dispatch.
// Defaults match ARTIST Construct 0x82278C98; no renderer reads mutable UI fields.
namespace BrnEffects
{
    struct EffectsDebugPostFxSettingsPC
    {
        bool mbBloom = true;
        bool mbVignette = true;
        bool mbDepthOfField = true;
        bool mbTint = true;
        bool mbTint2d = true;
        bool mbMotionBlur = true;
        bool mbMotionBlurEnableUserSettings = false;
        bool mbMotionBlurUserHighQuality = true;
        float mfMotionBlurUserAmountCars = 0.0f;
        float mfMotionBlurUserAmountWorld = 1.0f;
    };
}
