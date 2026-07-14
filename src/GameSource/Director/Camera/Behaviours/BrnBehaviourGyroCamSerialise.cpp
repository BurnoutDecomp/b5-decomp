// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCamSerialise.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGyroCam::Parameters field-walk visitor
// this TU owns:
//   - Parameters::Serialise<DebugMenuSerialiser>     @0x8224C240
//   - Parameters::Serialise<TextFileReadSerialiser>  @0x82231F10
//   - Parameters::Serialise<TextFileWriteSerialiser> @0x8224E040
// The gyro-cam parameter block is saved/loaded to the human-readable camera-tunings text file and
// edited in the in-game debug menu; each serialiser S walks the same field sequence, so the source
// is ONE templated body with one explicit instantiation per S the X360 emits. Mirrors the committed
// sibling CameraImpactEffect::Parameters::Serialise (BrnCameraImpactEffect.cpp) and the designated
// mirror BrnBehaviourGameplayBumper.cpp.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"

// The canonical homes of the three embedded sub-blocks, reinterpret_cast from the block's raw
// storage in the visitor body below (kept OUT of the widely-included BrnBehaviourGyroCam.h -- see the
// header note; BehaviourRig.h drags an inline Tweaker slice that ODR-clashes with the behaviour
// manager's canonical BrnCameraTweaker.h).
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"        // Utils::CameraShake::Parameters (+ Utils::Looker::Parameters via BrnLooker.h)
#include "GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h"  // AttachmentTruck::Parameters

// The three serialiser types this block is walked through. Each supplies the per-field/-section
// direction (Serialise(const char*, f32&) / (const char*, bool&) scalar leaves + the nested
// Serialise<T>(const char*, T&) section overload); the shared visitor body below is uniform across
// all three -- only the inlined leaf helper differs per S.
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser

#include <cstddef>   // offsetof (layout pins)

namespace BrnDirector
{
namespace Camera
{

// Layout pins (host-pointer-width invariant -- the whole block is f32/bool/enum aggregates with no
// pointers): every offset is the a1+OFF displacement the X360 Serialise<S> asm addresses. The
// embedded sub-blocks land at +0x08 / +0x2C / +0x90 (a bare CameraShake::Parameters is 16B, Looker
// ::Parameters is 100B, AttachmentTruck::Parameters is 8B), and the trailing scalar tunables follow.
static_assert(sizeof(Utils::CameraShake::Parameters) == 16,  "CameraShake::Parameters is 16B");
static_assert(sizeof(Utils::Looker::Parameters)      == 100, "Looker::Parameters is 100B");
static_assert(sizeof(AttachmentTruck::Parameters)    == 8,   "AttachmentTruck::Parameters is 8B");
static_assert(offsetof(BehaviourGyroCam::Parameters, maShakeParams)                == 0x08, "maShakeParams @ +0x08");
static_assert(offsetof(BehaviourGyroCam::Parameters, maLookerParams)               == 0x2C, "maLookerParams @ +0x2C");
static_assert(offsetof(BehaviourGyroCam::Parameters, maAttachmentTruck)            == 0x90, "maAttachmentTruck @ +0x90");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfSlowDistance)               == 0x98, "mfSlowDistance @ +0x98");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfFastPitch)                  == 0xAC, "mfFastPitch @ +0xAC");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfField_B0)                   == 0xB0, "mfField_B0 @ +0xB0");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfHeightDistanceVelocityRange) == 0xC4, "mfHeightDistanceVelocityRange @ +0xC4");
static_assert(offsetof(BehaviourGyroCam::Parameters, mbUseTruck)                   == 0xC8, "mbUseTruck @ +0xC8");
static_assert(offsetof(BehaviourGyroCam::Parameters, mbStickToGround)              == 0xCB, "mbStickToGround @ +0xCB");

// FLAG (unrecovered rodata @0x820051C0): the field label the gyro-cam Serialise passes for the
// +0xB0 f32 field (referenced only as `lis/addi unk_820051C0` in the debug-menu @0x8224C3E4 and
// write @0x8224E290 visitors; the literal string bytes are not in the export). This is the SAME
// unrecovered label address the bumper-cam block passes for its mfFOV field
// (BrnBehaviourGameplayBumper.cpp) -- declared extern and NOT fabricated; define it with the literal
// bytes at @0x820051C0 when that rodata is recovered.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0

// ----------------------------------------------------------------------------
// BehaviourGyroCam::Parameters::Serialise<S> -- the ONE gyro-cam field-walk visitor body.
//
// Recurses into the three embedded tuning sub-blocks as nested named sections, then walks the twelve
// f32 tunables and four bool flags in ascending-offset order, handing each to the serialiser S by
// name. S supplies the per-field/-section direction; the field sequence + labels are identical
// across all three instances' asm, so the source is this single uniform body:
//   - Serialise<DebugMenuSerialiser>    @0x8224C240: AddToPath(section) + recurse + pop path for each
//       sub-block, then each float -> Process<float>(name,&field) + SetStep(0.01) (the scalar
//       DebugMenuSerialiser::Serialise(const char*, f32&) inlines to that Process+SetStep pair) and
//       each flag -> Process<bool>(name,&field).
//   - Serialise<TextFileWriteSerialiser> @0x8224E040: the section-header line + recurse per sub-block,
//       then "<label> : <value>\n" per float ("%f") and per flag ("%d").
//   - Serialise<TextFileReadSerialiser>  @0x82231F10: consume the section-header line + recurse per
//       sub-block, then read each "<label> : <value>\n" line back.
//
// Field/label map (from the debug-menu asm, ascending offset):
//   +0x08 mShakeParams     "Shake Params"        +0xB0 mfField_B0                   <unk_820051C0>
//   +0x2C mLookerParams    "Looker Params"       +0xB4 mfBlendFactorBlendFactor     "Blend Factor Blend Factor"
//   +0x90 mAttachmentTruck "Attachment truck"    +0xB8 mfMinimumBlendFactor         "Minimum Blend Factor"
//   +0x98 mfSlowDistance   "Slow Distance"       +0xBC mfMaximumBlendFactor         "Maximum Blend Factor"
//   +0x9C mfSlowHeight     "Slow Height"         +0xC0 mfHeightDistanceBlendFactor  "Height Distance Blend Factor"
//   +0xA0 mfSlowPitch      "Slow Pitch"          +0xC4 mfHeightDistanceVelocityRange"Height Distance Velocity Range"
//   +0xA4 mfFastDistance   "Fast Distance"       +0xC8 mbUseTruck                   "Use Truck"
//   +0xA8 mfFastHeight     "Fast Height"         +0xC9 mbUseSideVector              "Use Side Vector"
//   +0xAC mfFastPitch      "Fast Pitch"          +0xCA mbInvertVector               "Invert Vector"
//                                                +0xCB mbStickToGround              "Stick to ground"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourGyroCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    // The three nested sub-blocks: cast the size-exact raw storage to its canonical sub-Parameters
    // type BY NAME (the X360 passes a1+8 / a1+0x2C / a1+0x90 straight to each block's Serialise).
    lrSerialiser.Serialise("Shake Params",
        reinterpret_cast<Utils::CameraShake::Parameters&>(maShakeParams));
    lrSerialiser.Serialise("Looker Params",
        reinterpret_cast<Utils::Looker::Parameters&>(maLookerParams));
    lrSerialiser.Serialise("Attachment truck",
        reinterpret_cast<AttachmentTruck::Parameters&>(maAttachmentTruck));

    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Slow Pitch", mfSlowPitch);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise("Fast Pitch", mfFastPitch);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField_B0);      // +0xB0 -- label unrecovered (extern)
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);

    lrSerialiser.Serialise("Use Truck", mbUseTruck);
    lrSerialiser.Serialise("Use Side Vector", mbUseSideVector);
    lrSerialiser.Serialise("Invert Vector", mbInvertVector);
    lrSerialiser.Serialise("Stick to ground", mbStickToGround);
}

// Explicit instantiations -- one per serialiser the X360 emits the gyro-cam block's field-walk over.
template void BehaviourGyroCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourGyroCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGyroCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
