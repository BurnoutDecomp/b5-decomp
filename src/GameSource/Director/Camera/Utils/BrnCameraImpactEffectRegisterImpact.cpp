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
// ⭐ 2026-09-24 (FX-CAMRIG): the class's other two bodies, CameraImpactEffect::Construct (DWARF
// BrnCameraShake.h:111) and ::Update (:118), live here too -- BehaviourAftertouchCrash's rig
// (Construct @0x822461F8, Update @0x82228158) is the first caller of either, and this is the
// class's only mounted file. Neither has an X360 symbol (both are inlined into the rig); Update is
// transcribed from the PS3 out-of-line body @0x28074 and checked against the rig's inline copy.
//
// DELETE-WHEN: the three camera-tunings serialisers are mounted. Then mount
// BrnCameraImpactEffect.cpp whole, MOVE Construct/Update/RegisterImpact there, and delete this
// file (and its mount line).
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"
#include "GameSource/Director/Camera/Camera.h"   // Camera::mTransform (Update shakes it in place)

#include <cmath>                                 // std::fmaf (Update's decay, one fmadds)

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

// DWARF BrnCameraShake.h:111. No X360 symbol -- BehaviourAftertouchCrash::Construct @0x822461F8
// inlines it on its +0x370 member: `stfs f0(0.0)` at 4/8/0xC/0x10(r10) (the embedded shake's own
// Construct, @0x82246488..0x82246494) and then at 0(r10) (@0x82246498).
void CameraImpactEffect::Construct()
{
    mCameraShake.Construct();
    mfImpactFactor = 0.0f;
}

// DWARF BrnCameraShake.h:118, PS3 @0x28074 (the out-of-line body):
//   0x28088  addi  this, this, 4                 ; &mCameraShake
//   0x28090  lfs   f0, 0x18(lParameters)         ; mfShakeFrequencyScale
//   0x28098  lfs   f2, 0x14(lParameters)         ; mfShakeMagnitude
//   0x2809C  fmuls f1, f0, f1                    ; frequency scale * timestep
//   0x280A0  fmuls f2, f13(mfImpactFactor), f2   ; impact * magnitude
//   0x280AC  bl    CameraShake::Update(transform, lParameters.mShakeParams, random, f1, f2)
//   0x280B8  fsubs f13, 0.0, impact ; 0x280CC fmadds f0, decay(+0x10), f13, impact
// The X360 inlines it into BehaviourAftertouchCrash::Update (0x8222921C..0x82229280) with the
// rig's constant block folded in: f1 is the bare timestep (its frequency scale is 1.0, and
// x * 1.0 == x for every x), f2 = impact * 60.0 (`fmuls f2, f13, f0` @0x8222926C), and the decay
// is `fneg f13, impact ; fmadds f0, f13, 0.06, impact` (@0x82229278/0x8222927C) -- the same
// value as the PS3's `0.0 - impact` for every input (the two differ only in the sign of a zero
// intermediate, and x + (+-0) with x == +-0 rounds to the same result).
// ⭐ The decay is ONE fmadds on both consoles (X360 @0x8222927C, PS3 @0x280CC): -impact * decay + impact
// rounded once (ROUNDING_RULE 3, FX-GATE crash parity 2026-09-25). The PC's separate multiply and add
// missed it by an ulp on 583 of 20000 impact factors in [0, 1) at the rig's decay 0.06, and the factor
// carries into the next frame's shake scale.
// The shake runs on the camera's transform in place (the rig passes the camera itself, r4 = r24,
// whose transform is its +0x00 member).
void CameraImpactEffect::Update(Camera& lrCamera, const Parameters& lrParameters, Random& lrRandom,
                                f32 lfTimestep)
{
    mCameraShake.Update(lrCamera.mTransform, lrParameters.mShakeParams, lrRandom,
                        lrParameters.mfShakeFrequencyScale * lfTimestep,
                        mfImpactFactor * lrParameters.mfShakeMagnitude);

    mfImpactFactor = std::fmaf(-mfImpactFactor, lrParameters.mfShakeDecayFactor, mfImpactFactor);
}

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
