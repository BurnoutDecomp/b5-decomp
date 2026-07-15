// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayBumper slices this TU set
// owns:
//   - BehaviourGameplayBumper::SetParameters @0x821F39C0   (inline in the header; the
//     out-of-line anchor below forces its emission)
//   - BehaviourGameplayBumper::Parameters::Set @0x821F94C8  (defined here)
//
// SetParameters is adopted by the replay director and the roaming arbitrator state when they
// install a bumper-cam parameter block; Parameters::Set is the seeding step the main director
// runs (ProcessNewVehicleEvents / UpdateAttribSys) to populate a bumper-cam block from the
// vehicle's attribute-system source block before installing it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"

// The bumper-cam Parameters::Serialise<S> visitor drives the camera-tunings serialiser S by name;
// each S provides the scalar/nested field helpers. Pull in the three serialisers this TU
// instantiates the visitor over: the two file serialisers plus the debug-menu serialiser (its
// reconstructed home, BrnDebugMenuSerialiser.h, has since landed -- see the note above the
// explicit instantiations).
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata @0x820051C0): the field label the bumper-cam Serialise passes for
// mfFOV (the +0x24 field, sibling to "FOV during boost" at +0x30). The X360 references only the
// address (lis/addi unk_820051C0) in both the write @0x822150DC and debug-menu @0x82214B40
// visitors; the literal string bytes are not in the export, so the label is declared extern and
// NOT fabricated (same treatment as the read serialiser's unrecovered component suffixes). When
// that rodata is recovered, define it with the literal bytes at @0x820051C0.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfFOV field label

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayBumper::Parameters::Set @0x821F94C8
//
// Seeds this bumper-cam parameter block: stamps the type tag + debug name + the fixed default
// tunables, then copies the per-vehicle tunables out of the attribute-system source array
// (reached through lpSource->mpfValues, i.e. *(a2+4)). Finally asserts the two FOV tunables
// are positive. Store-for-store from the asm: the default-tunable stores are issued with the
// X360's intermediate overwrites (e.g. +0x44 is seeded 0.11f then overwritten 1.0f); the
// final committed values are 0.0f/0.0f/3.0f/1.0f at +0x38/+0x3C/+0x40/+0x44.
//
// Float constants (decoded from the rodata loaded by the asm):
//   flt_820047C0 = 0.11f   flt_820047BC = 1.15f   flt_820047B8 = 0.06f (0.059999999)
//   flt_82001CC0 = 0.0f    flt_82001C98 = 1.0f    flt_82004270 = 3.0f
// ----------------------------------------------------------------------------
void BehaviourGameplayBumper::Parameters::Set(const Source* lpSource)
{
    meType = eBehaviourGameplayBumper;            // stw r9(=1), 0(r31)

    // --- default tunables, in asm store order (intermediate values are overwritten) ---
    mfField44 = 0.11f;                            // stfs flt_820047C0, 0x44   (seed)
    mfField40 = 1.15f;                            // stfs flt_820047BC, 0x40   (seed)
    mfField38 = 0.06f;                            // stfs flt_820047B8, 0x38   (seed; 0.059999999)
    mfField3C = 0.0f;                             // stfs flt_82001CC0, 0x3C
    mpcName   = "Bumper Cam";                     // stw  "Bumper Cam", 0x04
    mfField38 = 0.0f;                             // stfs flt_82001CC0, 0x38   (override)
    mbField34 = 1;                                // stb  r9(=1), 0x34
    mfField3C = 0.0f;                             // stfs flt_82001CC0, 0x3C
    mfField44 = 1.0f;                             // stfs flt_82001C98, 0x44   (override)
    mfField40 = 3.0f;                             // stfs flt_82004270, 0x40   (override)

    // --- per-vehicle tunables copied from the attribute-system source array ---
    const f32* lpfSrc = lpSource->mpfValues;      // lwz r11, 4(r4)  (re-loaded each store on X360)
    mfField10 = lpfSrc[0x28 / 4];                 // <- source[0x28]
    mfField14 = lpfSrc[0x24 / 4];                 // <- source[0x24]
    mfField2C = lpfSrc[0x20 / 4];                 // <- source[0x20]
    mfField28 = lpfSrc[0x1C / 4];                 // <- source[0x1C]
    mfBoostFOV = lpfSrc[0x18 / 4];                // <- source[0x18]  (the v3/boost-FOV check value)
    mfFOV     = lpfSrc[0x14 / 4];                 // <- source[0x14]
    mfField18 = lpfSrc[0x10 / 4];                 // <- source[0x10]
    mfField20 = lpfSrc[0x0C / 4];                 // <- source[0x0C]
    mfField1C = lpfSrc[0x08 / 4];                 // <- source[0x08]
    mfField08 = lpfSrc[0x04 / 4];                 // <- source[0x04]
    mfField0C = lpfSrc[0x00 / 4];                 // <- source[0x00]

    CGS_ASSERT(mfBoostFOV > 0.0f, "mfBoostFOV > 0.0f");
    CGS_ASSERT(mfFOV > 0.0f, "mfFOV > 0.0f");
}

// Out-of-line anchor: forces BehaviourGameplayBumper::SetParameters (inline in the header) to
// be emitted in this TU.
void BehaviourGameplayBumper_SetParametersAnchor(
    BehaviourGameplayBumper& lrBehaviour,
    const BehaviourGameplayBumper::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

// ----------------------------------------------------------------------------
// BehaviourGameplayBumper::Parameters::Serialise<S> -- the ONE bumper-cam field-walk visitor body.
//
// Walks the block's eleven tunable f32 fields in ascending-offset order, handing each to the
// serialiser S by name. S supplies the per-field direction: TextFileWriteSerialiser writes each as
// a "<label> : <value>\n" line (FormatName + fprintf, inlined -> @0x82214F10); TextFileReadSerialiser
// reads each back (fscanf "%s : %f\n", inlined -> @0x82202B50). The body is uniform across S -- the
// field sequence + labels are identical in the write and read instances' asm; only S's inlined
// scalar helper differs. The DebugMenuSerialiser instance (@0x82214A08, Process<float> + SetStep per
// field) shares this same source body but is NOT instantiated here (its serialiser type has no
// reconstructed home yet -- see the note below).
//
// Field/label map (from the write+read asm, ascending offset):
//   +0x08 mfField08 "Y Offset"          +0x1C mfField1C "Yaw Spring"
//   +0x0C mfField0C "Z Offset"          +0x20 mfField20 "Roll Spring"
//   +0x10 mfField10 "Accel. Dampening"  +0x24 mfFOV      <unk_820051C0>
//   +0x14 mfField14 "Accel. Response"   +0x28 mfField28 "Body Roll Scale"
//   +0x18 mfField18 "Pitch Spring"      +0x2C mfField2C "Body Pitch Scale"
//                                       +0x30 mfBoostFOV "FOV during boost"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourGameplayBumper::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Y Offset", mfField08);
    lrSerialiser.Serialise("Z Offset", mfField0C);
    lrSerialiser.Serialise("Accel. Dampening", mfField10);
    lrSerialiser.Serialise("Accel. Response", mfField14);
    lrSerialiser.Serialise("Pitch Spring", mfField18);
    lrSerialiser.Serialise("Yaw Spring", mfField1C);
    lrSerialiser.Serialise("Roll Spring", mfField20);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfFOV);      // +0x24 -- label unrecovered (extern)
    lrSerialiser.Serialise("Body Roll Scale", mfField28);
    lrSerialiser.Serialise("Body Pitch Scale", mfField2C);
    lrSerialiser.Serialise("FOV during boost", mfBoostFOV);
}

// Explicit instantiations -- one per serialiser this block is menu-mirrored / saved / loaded through.
// The DebugMenuSerialiser home (BrnDebugMenuSerialiser.h) has since landed -- its scalar
// Serialise(const char*, f32&) overload inlines to the Process<float> + CgsDev::DebugComponent::SetStep
// pair the X360 debug-menu instance @0x82214A08 emits per field -- so the debug-menu instance is now
// emitted here too (mirrors the committed BehaviourBystanderCam::Parameters::Serialise instantiations).
template void BehaviourGameplayBumper::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourGameplayBumper::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGameplayBumper::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
