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

namespace BrnDirector
{
namespace Camera
{

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

} // namespace Camera
} // namespace BrnDirector
