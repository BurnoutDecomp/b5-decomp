// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafeSerialise.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourFailsafe::Parameters::Serialise<S> --
// the failsafe-camera parameter-block field-walk visitor (ledger key
// class:BrnDirector::Camera::BehaviourFailsafe::Parameters). Separate TU from
// BrnBehaviourFailsafe.cpp (which anchors SetParameters/GetCollisionPolicy); it carries only the
// three template instantiations the camera-tunings serialiser system drives:
//   - Serialise<DebugMenuSerialiser>     @0x8224C960
//   - Serialise<TextFileReadSerialiser>  @0x822324B0
//   - Serialise<TextFileWriteSerialiser> @0x8224E9F8
//
// ONE uniform templated body drives the serialiser S by name; each S supplies the per-field
// direction. The three X360 instances are the same field/label sequence in the same order --
// only S's inlined leaf helper differs (Hex-Rays renders the write as FormatName+fprintf, the
// read as fscanf, the debug-menu as AddToPath/pop for the nested blocks + Process<T>+SetStep(0.01)
// for the scalar leaves). Reconstructed as the shared source that produces all three -- mirrors
// the committed BrnDirector::Camera::Utils::Looker::Parameters::Serialise design
// (BrnLookerSerialise.cpp) and BehaviourRig::Parameters (nested sub-block recursion).
//
// Field/label map (ascending offset; from the write @0x8224E9F8 + read @0x822324B0 + debug-menu
// @0x8224C960 asm):
//   +0x08 mShakeParams                    "Shake Params"   (nested CameraShake::Parameters)
//   +0x2C mLookerParams                   "Looker Params"  (nested Looker::Parameters)
//   +0x90 mfSlowDistance                  "Slow Distance"
//   +0x94 mfSlowHeight                    "Slow Height"
//   +0x98 mfSlowPitch                     "Slow Pitch"
//   +0x9C mfFastDistance                  "Fast Distance"
//   +0xA0 mfFastHeight                    "Fast Height"
//   +0xA4 mfFastPitch                     "Fast Pitch"
//   +0xA8 mfField168                      <rodata @0x820051C0 -- label unrecovered (see FLAG)>
//   +0xAC mfBlendFactorBlendFactor        "Blend Factor Blend Factor"
//   +0xB0 mfMinimumBlendFactor            "Minimum Blend Factor"
//   +0xB4 mfMaximumBlendFactor            "Maximum Blend Factor"
//   +0xB8 mfHeightDistanceBlendFactor     "Height Distance Blend Factor"
//   +0xBC mfHeightDistanceVelocityRange   "Height Distance Velocity Range"
//   +0xC0 mbStickToGround                 "Stick to ground"
// (The Failsafe block carries no leading version tag -- unlike Looker/Rig -- so the visitor opens
// straight on the shake sub-block, consistent across all three instances' asm.)
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser

#include <cstddef>   // offsetof (layout pins)

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata @0x820051C0): the field label the failsafe Serialise passes for the
// +0xA8 f32 (mfField168) -- the same rodata address the bumper-cam visitors reference for their
// mfFOV label (BrnBehaviourGameplayBumper.cpp). The X360 references only the address
// (lis/addi unk_820051C0) in all three failsafe instances (write @0x8224E9F8, debug-menu
// @0x8224C960); the literal string bytes are not in the export, so the label is declared extern
// and NOT fabricated. Define it with the literal bytes at @0x820051C0 when that rodata is
// recovered. (Same extern the bumper-cam TU declares; a single unrecovered string, reused.)
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- shared unrecovered field label

// ----------------------------------------------------------------------------
// BehaviourFailsafe::Parameters::Serialise<S> -- the ONE failsafe field-walk visitor body.
//
// Serialises the two embedded camera post-process sub-blocks as nested sections (each S's nested
// Serialise<T>(name, block) pushes the section name onto the path / writes-or-reads the section
// header, recurses into the sub-block's own Serialise<S>, then unwinds), then the failsafe scalar
// tunables in ascending offset order. The nested sub-block Serialise bodies + their explicit
// instantiations over all three serialisers are the CameraShake / Looker Serialise TUs
// (BrnCameraShake.cpp / BrnLookerSerialise.cpp); this body only drives them by name.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourFailsafe::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);    // nested CameraShake::Parameters (this+0x08)
    lrSerialiser.Serialise("Looker Params", mLookerParams);  // nested Looker::Parameters (this+0x2C)

    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Slow Pitch", mfSlowPitch);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise("Fast Pitch", mfFastPitch);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField168);   // +0xA8 -- label unrecovered (extern)
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);
    lrSerialiser.Serialise("Stick to ground", mbStickToGround);
}

// Layout pins (pointer-invariant; the block is pointer-free so the LLP64 host reproduces the
// console byte offsets exactly). Guards the embedded-sub-block offsets + the trailing scalar run.
static_assert(offsetof(BehaviourFailsafe::Parameters, mShakeParams)   == 0x08,
              "BehaviourFailsafe::Parameters::mShakeParams @ +0x08");
static_assert(offsetof(BehaviourFailsafe::Parameters, mLookerParams)  == 0x2C,
              "BehaviourFailsafe::Parameters::mLookerParams @ +0x2C");
static_assert(offsetof(BehaviourFailsafe::Parameters, mfSlowDistance) == 0x90,
              "BehaviourFailsafe::Parameters::mfSlowDistance @ +0x90 (144)");
static_assert(offsetof(BehaviourFailsafe::Parameters, mbStickToGround) == 0xC0,
              "BehaviourFailsafe::Parameters::mbStickToGround @ +0xC0 (192)");

// Explicit instantiations -- one per serialiser this block is saved / loaded / menu-mirrored through.
template void BehaviourFailsafe::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourFailsafe::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourFailsafe::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
