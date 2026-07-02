// BrnDirector::PerlinShakeController -- the Perlin-noise camera shake. Reconstructed from
// BURNOUT_X360_ARTIST.XEX, semantic-parity (not byte-matching).
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.cpp):
//   PerlinShakeController::Update @0x8221E798  (called by KeyAnimShakeController::Update)
//
// X360 asm walk (@0x8221E798):
//   f30 = camera+0x114 (effects +0xAC, the shake amplitude -- the inlined
//         Camera::GetEffects().GetShakeAmplitude() the PS3 DWARF hint names)
//   if (f30 > 0.0):
//     three CgsNumeric::PerlinNoise::Noise calls, each phased by
//     camera+0x118 (effects +0xB0, the shake frequency) * lfTime * <axis freq scale>:
//       roll  channel: phase + 0     (flt_82001B04 ==  50.0, flt_820049E0 == 100.0)
//       pitch channel: phase + 50.0
//       yaw   channel: phase + 100.0
//     lRotationAngles = { yawNoise*amp*yawScale, pitchNoise*amp*pitchScale,
//                         rollNoise*amp*rollScale } (stack vector var_B0)
//     lTransform = identity (rows {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0})
//     Utils::RotateMatrix44AffineByEulerAnglesZXY(lTransform, lRotationAngles)
//       (angles passed by value in vr1)
//     camera rows 0..3 = lTransform x camera rows (vspltw+vmaddfp row-combine; the w row
//       seeds with the camera's own w row == rw's operator*(lhs, rhs) exactly)
//     ValidateTransformWithDebugInfo(camera)   (the tail of the inlined SetTransform)
//
// The final store+validate is the inlined Camera::SetTransform (the PS3 DWARF hint names
// Camera::SetTransform here); its body -- the row stores plus the debug validation --
// belongs to the Camera TU.

#include "GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.h"

#include "GameShared/GameClasses/Numeric/CgsPerlinNoise.h"   // CgsNumeric::PerlinNoise::Noise
#include "GameSource/Director/Camera/Camera.h"               // Camera::Camera / CameraEffects
#include "GameSource/Director/Camera/Utils/CameraUtils.h"    // Utils::RotateMatrix44AffineByEulerAnglesZXY
#include "rw/math/vpu/matrix44affine_operation.h"            // operator*(Matrix44Affine, Matrix44Affine)

namespace BrnDirector
{
namespace
{
    // The two decorrelation phase offsets the X360 adds to the pitch / yaw noise channels
    // (rodata flt_82001B04 / flt_820049E0).
    const f32 KF_PITCH_NOISE_PHASE_OFFSET = 50.0f;
    const f32 KF_YAW_NOISE_PHASE_OFFSET   = 100.0f;
}

// @0x8221E798.
void PerlinShakeController::Update(Camera::Camera* lpCamera, f32 lfTime,
                                   f32 lfRollScale, f32 lfPitchScale, f32 lfYawScale,
                                   f32 lfRollFreqScale, f32 lfPitchFreqScale, f32 lfYawFreqScale)
{
    const f32 lfShakeAmount = lpCamera->GetEffects().GetShakeAmplitude();
    if (lfShakeAmount > 0.0f)
    {
        // Three decorrelated 1-D noise channels, phased by the camera's shake frequency.
        // (The asm reloads the frequency before each call; a hoisted local is semantically
        // identical for the pure member read.)
        const f32 lfShakeFrequency = lpCamera->GetEffects().GetShakeFrequency();

        const f32 lfRollAmount = static_cast<f32>(CgsNumeric::PerlinNoise::Noise(
                                     lfShakeFrequency * lfTime * lfRollFreqScale)) * lfShakeAmount;
        const f32 lfPitchAmount = static_cast<f32>(CgsNumeric::PerlinNoise::Noise(
                                      lfShakeFrequency * lfTime * lfPitchFreqScale
                                      + KF_PITCH_NOISE_PHASE_OFFSET)) * lfShakeAmount;
        const f32 lfYawAmount = static_cast<f32>(CgsNumeric::PerlinNoise::Noise(
                                    lfShakeFrequency * lfTime * lfYawFreqScale
                                    + KF_YAW_NOISE_PHASE_OFFSET)) * lfShakeAmount;

        // The Euler-angle vector, exactly as the asm packs it (x <- the yaw channel,
        // y <- the pitch channel, z <- the roll channel).
        const Vector3 lRotationAngles = Vector3{ lfYawAmount * lfYawScale,
                                                 lfPitchAmount * lfPitchScale,
                                                 lfRollAmount * lfRollScale,
                                                 0.0f };

        Matrix44Affine lTransform;
        lTransform.SetIdentity();
        Camera::Utils::RotateMatrix44AffineByEulerAnglesZXY(lTransform, lRotationAngles);

        // Compose the shake onto the live camera transform and adopt it (the X360 inlines
        // SetTransform as the four row stores + ValidateTransformWithDebugInfo).
        lpCamera->SetTransform(lTransform * lpCamera->GetTransform());
    }
}
}
