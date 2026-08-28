// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraImpactEffectRegisterImpact.cpp
//
// ⭐ A PARTFILE of Camera/Utils/BrnCameraImpactEffect.cpp, carrying ONE body:
//     BrnDirector::Camera::Utils::CameraImpactEffect::RegisterImpact  @0x821F3648
//
// SAME SPLIT, SAME REASON as Camera/Utils/BrnCameraShakeUpdate.cpp (2026-08-02), which carved
// CameraShake::Update out of BrnCameraShake.cpp: the parent TU also carries the three explicit
// `Parameters::Serialise<S>` instantiations, and DebugMenuSerialiser /
// TextFileWriteSerialiser / TextFileReadSerialiser are NOT on the exe source list, so mounting
// the parent to reach this one function opens serialiser externals to close one impact one --
// "three unresolved opened to close one", exactly what BrnCameraShake.cpp's own mount note
// records. Split out, the mount cost is zero: this file's only dependency is its own header.
//
// WHO NEEDS IT: ImpactShakeController::Update (Behaviours/BehaviourBystanderCamImpactControllers.cpp),
// which ArbStateCrashing::ApplySlomoAndShake @0x8224F8D8 drives every frame of a crash. Before
// this split it was the ONE unresolved external in the whole link.
//
// DELETE-WHEN: the three camera-tunings serialisers are mounted. Then mount
// BrnCameraImpactEffect.cpp whole and delete this file (and its mount line).
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// @0x821F3648 -- BrnCameraShake.h:221 tripwire (non-gating), then keep the larger of the pending
// factor and the new magnitude (the asm's branchless
// `fsel(mfImpactFactor - lfImpulseMagnitude, mfImpactFactor, lfImpulseMagnitude)`).
void CameraImpactEffect::RegisterImpact(f32 lfImpulseMagnitude)
{
    CGS_ASSERT(lfImpulseMagnitude >= 0.0f, "lfImpulseMagnitude >= 0.0f");   // :221

    if (mfImpactFactor - lfImpulseMagnitude < 0.0f)
    {
        mfImpactFactor = lfImpulseMagnitude;
    }
}

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
