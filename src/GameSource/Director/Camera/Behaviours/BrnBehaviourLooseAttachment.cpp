// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourLooseAttachment slice this TU owns:
//   - BehaviourLooseAttachment::AttachTo      @0x821F4458
//   - BehaviourLooseAttachment::Get           @0x821FAA58
//   - BehaviourLooseAttachment::SetParameters @0x821F43E8
//   - BehaviourLooseAttachment::SetTarget     @0x821F44B8
// and the loose-attachment Parameters field-walk visitor (class:BrnDirector::Camera::
// BehaviourLooseAttachment::Parameters):
//   - Parameters::Serialise<DebugMenuSerialiser>     @0x82254248
//   - Parameters::Serialise<TextFileReadSerialiser>  @0x8224D2F0
//   - Parameters::Serialise<TextFileWriteSerialiser> @0x82254BC8
//
// The four accessor methods are defined inline in the header; this .cpp is the translation-unit
// anchor that pulls the header into the compile gate and pins the asm-attested member offsets.
// Installed by the new-car-joined / shutdown-takedown moments and the testbed arbitrator state.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"

// The three Parameters::Serialise<S> instances drive one serialiser type each by name; pull in
// every serialiser this TU instantiates the visitor over. (BrnCameraImpactEffect.h, for the
// embedded "Impact" sub-block's own nested Serialise<S>, arrives transitively via the behaviour
// header.)
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser

#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata @0x820051C0): the field label the loose-attachment Serialise passes for
// the +0x54 tunable (mfField54, sibling to "Distance" @+0x50 and "Dutch" @+0x58). All three visitor
// instances reference only the address (lis/addi unk_820051C0) -- the debug-menu Process<float>
// @0x82254248, the write FormatName @0x82254BC8 -- so the literal string bytes are not in the export
// and the label is declared extern and NOT fabricated (the same shared @0x820051C0 rodata the
// bumper-cam / camera-rig visitors reference for their own unrecovered field labels). Define it with
// the literal bytes at @0x820051C0 when that rodata is recovered.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfField54 field label

// Pin the asm-attested member offsets the four methods touch.
static_assert(offsetof(BehaviourLooseAttachment, mParamWord1)          == 0x010, "param word 1 @ +0x10");
static_assert(offsetof(BehaviourLooseAttachment, miTargetSet)          == 0x304, "target set flag @ +0x304");
static_assert(offsetof(BehaviourLooseAttachment, meTargetRaceCarIndex) == 0x308, "target index @ +0x308");
static_assert(offsetof(BehaviourLooseAttachment, miTargetField30C)     == 0x30C, "target field @ +0x30C");
static_assert(offsetof(BehaviourLooseAttachment, mbTargetField310)     == 0x310, "target byte @ +0x310");
static_assert(offsetof(BehaviourLooseAttachment, miAttachSet)          == 0x314, "attach set flag @ +0x314");
static_assert(offsetof(BehaviourLooseAttachment, meAttachRaceCarIndex) == 0x318, "attach index @ +0x318");
static_assert(offsetof(BehaviourLooseAttachment, miAttachField31C)     == 0x31C, "attach field @ +0x31C");
static_assert(offsetof(BehaviourLooseAttachment, mbAttachField320)     == 0x320, "attach byte @ +0x320");
static_assert(offsetof(BehaviourLooseAttachment, muParametersSlot)     == 0x324, "params slot @ +0x324");
static_assert(offsetof(BehaviourLooseAttachment, mbNoResult)           == 0x32C, "no-result flag @ +0x32C");

// Out-of-line anchors: force the four inline methods to be emitted in this TU.
void BehaviourLooseAttachment_AttachToAnchor(BehaviourLooseAttachment& lr, s32 li)      { lr.AttachTo(li); }
void* BehaviourLooseAttachment_GetAnchor(BehaviourLooseAttachment& lr)                  { return lr.Get(); }
void BehaviourLooseAttachment_SetParametersAnchor(BehaviourLooseAttachment& lr,
                                                   const BehaviourLooseAttachment::Parameters* lp) { lr.SetParameters(lp); }
void BehaviourLooseAttachment_SetTargetAnchor(BehaviourLooseAttachment& lr, s32 li)     { lr.SetTarget(li); }

// Pin the asm-attested Parameters field-walk offsets (host-pointer-width invariant -- the walked
// region holds no pointers): the "Impact" sub-block at +0x2C and the loose-attachment tunables at
// the a1+0x48..a1+0x60 offsets the write/read/menu asm loads/stores.
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mImpact)            == 0x2C, "Impact block @ +0x2C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfPitch)            == 0x48, "Pitch @ +0x48");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfHeight)           == 0x4C, "Height @ +0x4C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfDistance)         == 0x50, "Distance @ +0x50");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfField54)          == 0x54, "unk label field @ +0x54");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfDutch)            == 0x58, "Dutch @ +0x58");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfDetachLerpAmount) == 0x5C, "Detach Lerp Amount @ +0x5C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mbLookFromTarget)   == 0x60, "Look from target @ +0x60");

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::Parameters::Serialise<S> -- the ONE loose-attachment field-walk visitor
// body. Recurses into the embedded impact block as the nested "Impact" section, then walks the
// loose-attachment tunables, handing each to the serialiser S by name. S supplies the per-field/
// -section direction; the field sequence + labels are identical across all three instances' asm
// (mirrors the committed sibling CameraImpactEffect::Parameters::Serialise in BrnCameraImpactEffect.cpp):
//   - Serialise<DebugMenuSerialiser>    @0x82254248: AddToPath("Impact") + recurse into the impact
//       block, then each float -> Process<float>(name, &field) + SetStep(0.01); the bool via
//       Process<bool>("Look from target", &field). (The scalar DebugMenuSerialiser::Serialise
//       overloads inline to those Process+SetStep pairs.)
//   - Serialise<TextFileWriteSerialiser> @0x82254BC8: the "Impact" section header + recurse, then
//       each float -> FormatName + fprintf "%s : %f\n"; the bool -> "%s : %d\n" when mpFile is open.
//   - Serialise<TextFileReadSerialiser>  @0x8224D2F0: consume the section-header line + recurse, then
//       each float -> fscanf "%s : %f\n"; the bool -> fscanf "%s : %d\n" while the FILE* is open.
//
// Field/label map (from the write/read/menu asm, in visitor walk order):
//   +0x2C mImpact            "Impact"              (nested CameraImpactEffect::Parameters block)
//   +0x48 mfPitch            "Pitch"
//   +0x4C mfHeight           "Height"
//   +0x50 mfDistance         "Distance"
//   +0x54 mfField54          <unk_820051C0>        (label rodata unrecovered -- extern, FLAG)
//   +0x58 mfDutch            "Dutch"
//   +0x60 mbLookFromTarget   "Look from target"    (walked BEFORE the +0x5C float -- asm order)
//   +0x5C mfDetachLerpAmount "Detach Lerp Amount"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourLooseAttachment::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Impact", mImpact);
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Height", mfHeight);
    lrSerialiser.Serialise("Distance", mfDistance);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField54);   // +0x54 -- label unrecovered (extern)
    lrSerialiser.Serialise("Dutch", mfDutch);
    lrSerialiser.Serialise("Look from target", mbLookFromTarget);   // +0x60 bool, walked before +0x5C
    lrSerialiser.Serialise("Detach Lerp Amount", mfDetachLerpAmount);
}

// Explicit instantiations -- one per serialiser this block is menu'd / saved / loaded through. All
// three serialiser types are reconstructed and homed (the DebugMenu instance's nested-block section
// overload lands via the additive declaration in BrnDebugMenuSerialiser.h), so every present
// instance is emitted.
template void BehaviourLooseAttachment::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourLooseAttachment::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourLooseAttachment::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
