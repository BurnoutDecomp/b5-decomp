// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayExternal::Parameters slice
// this TU owns:
//   - BehaviourGameplayExternal::Parameters::Set @0x821F9228  (defined here)
//
// Parameters::Set is the seeding step the main director runs (ProcessNewVehicleEvents /
// UpdateAttribSys) and the replay director runs (PreSceneQueryUpdate) to populate an
// external ("chase") gameplay-camera block from the vehicle's attribute-system source block
// before installing it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayExternal::Parameters::Set @0x821F9228
//
// Seeds this external-cam parameter block: stamps the type tag + debug name + integer slot +
// the fixed default tunables, then copies the per-vehicle tunables out of the attribute-system
// source array (reached through lpSource->mpfValues, i.e. *(a2+4)). Asserts mfBoostFOV and
// mrFOV are positive, then overrides mfFieldA0 to -1.0f as the final store.
//
// Float constants (decoded from the rodata loaded by the asm):
//   flt_82001CC0 = 0.0f     flt_82001C98 = 1.0f      flt_82001DA0 = 0.5f
//   flt_82002138 = 0.01f    flt_82003F40 = 0.25f     flt_82004014 = 0.1f
//   flt_82004018 = 0.75f    flt_82004270 = 3.0f      flt_820047B8 = 0.06f (0.059999999)
//   flt_820047BC = 1.15f    flt_820047C0 = 0.11f     flt_82004C68 = 0.7f (0.69999999)
//   flt_82004C6C = 60.0f    flt_82004C70 = 17.0f     flt_82004C74 = 0.015f
//   flt_82004C78 = -0.5f    flt_82004C88 = 8.0f      flt_820037C8 = -1.0f
//
// Field offsets verified store-for-store: the source-copied slots are the lfs ...(r11) loads
// off lpSource->mpfValues, and the default slots are the lfs flt_...(r10) rodata loads. Every
// 4-byte slot in +0x00..+0xAC is written.
// ----------------------------------------------------------------------------
void BehaviourGameplayExternal::Parameters::Set(const Source* lpSource)
{
    meType    = eBehaviourGameplayExternal;        // stw r9(=0), 0x00(r31)

    // --- fixed default tunables (rodata constant stores) ---
    mfField18 = 0.11f;                             // stfs flt_820047C0, 0x18
    mfField10 = 0.0f;                              // stfs flt_82001CC0, 0x10
    mfField14 = 1.15f;                             // stfs flt_820047BC, 0x14
    mfField0C = 0.06f;                             // stfs flt_820047B8, 0x0C  (0.059999999)
    mfField28 = 0.11f;                             // stfs flt_820047C0, 0x28  (seed, overridden)
    mfField24 = 1.15f;                             // stfs flt_820047BC, 0x24  (seed, overridden)
    mfField1C = 0.0f;                              // stfs flt_82001CC0, 0x1C
    mfField20 = 0.0f;                              // stfs flt_82001CC0, 0x20
    mbFieldAC = 1;                                 // stb  r8(=1), 0xAC
    mfField28 = 1.0f;                              // stfs flt_82001C98, 0x28  (override)
    mpcName   = "Gameplay";                        // stw  "Gameplay", 0x04
    mfField24 = 3.0f;                              // stfs flt_82004270, 0x24  (override)
    miField08 = 3;                                 // stw  r7(=3), 0x08
    mfField1C = 0.0f;                              // stfs flt_82001CC0, 0x1C
    mfField20 = 0.0f;                              // stfs flt_82001CC0, 0x20
    mfField2C = 8.0f;                              // stfs flt_82004C88, 0x2C
    mfField30 = 8.0f;                              // stfs flt_82004C88, 0x30
    mfField34 = 0.75f;                             // stfs flt_82004018, 0x34
    mfField38 = 0.0f;                              // stfs flt_82001CC0, 0x38
    mfField44 = -0.5f;                             // stfs flt_82004C78, 0x44
    mfField74 = 0.0f;                              // stfs flt_82001CC0, 0x74
    mfField48 = 0.015f;                            // stfs flt_82004C74, 0x48
    mfField98 = 0.0f;                              // stfs flt_82001CC0, 0x98
    mfField60 = 17.0f;                             // stfs flt_82004C70, 0x60
    mfField64 = 0.25f;                             // stfs flt_82003F40, 0x64
    mfField70 = 60.0f;                             // stfs flt_82004C6C, 0x70
    mfFieldA8 = 0.5f;                              // stfs flt_82001DA0, 0xA8
    mfField7C = 0.01f;                             // stfs flt_82002138, 0x7C  (0.0099999998)
    mfField80 = 0.1f;                              // stfs flt_82004014, 0x80
    mfField84 = 0.7f;                              // stfs flt_82004C68, 0x84  (0.69999999)

    // --- per-vehicle tunables copied from the attribute-system source array ---
    const f32* lpfSrc = lpSource->mpfValues;       // lwz r11, 4(r4)  (re-loaded each store on X360)
    mfBoostFOV = lpfSrc[0x40 / 4];                 // <- source[+0x40]  (asserted > 0)
    mfField90  = lpfSrc[0x3C / 4];                 // <- source[+0x3C]
    mfField94  = lpfSrc[0x38 / 4];                 // <- source[+0x38]
    mfField8C  = lpfSrc[0x34 / 4];                 // <- source[+0x34]
    mrFOV      = lpfSrc[0x30 / 4];                 // <- source[+0x30]  (asserted > 0)
    mfField3C  = lpfSrc[0x2C / 4];                 // <- source[+0x2C]
    mfField4C  = lpfSrc[0x28 / 4];                 // <- source[+0x28]
    mfField50  = lpfSrc[0x24 / 4];                 // <- source[+0x24]
    mfField54  = lpfSrc[0x20 / 4];                 // <- source[+0x20]
    mfField40  = lpfSrc[0x08 / 4];                 // <- source[+0x08]
    mfField58  = lpfSrc[0x1C / 4];                 // <- source[+0x1C]
    mfField5C  = lpfSrc[0x18 / 4];                 // <- source[+0x18]
    mfField68  = lpfSrc[0x14 / 4];                 // <- source[+0x14]
    mfField40  = lpfSrc[0x08 / 4];                 // <- source[+0x08]  (re-stored on X360)
    mfField9C  = lpfSrc[0x04 / 4];                 // <- source[+0x04]
    mfFieldA0  = lpfSrc[0x0C / 4];                 // <- source[+0x0C]  (overridden below)
    mfFieldA4  = lpfSrc[0x10 / 4];                 // <- source[+0x10]
    mfField88  = lpfSrc[0x00 / 4];                 // <- source[+0x00]

    CGS_ASSERT(mfBoostFOV > 0.0f, "mfBoostFOV > 0.0f");
    CGS_ASSERT(mrFOV > 0.0f, "mrFOV > 0.0f");

    mfFieldA0 = -1.0f;                             // stfs flt_820037C8, 0xA0  (final override)
}

} // namespace Camera
} // namespace BrnDirector
