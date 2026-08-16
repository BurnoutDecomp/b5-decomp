#ifndef BRN_ENVIRONMENT_KEYFRAME_H
#define BRN_ENVIRONMENT_KEYFRAME_H

#include "types.hpp"

#include "SharedClasses/Graphics/BrnEffectsData.h"                       // BrnEffects::BloomData / VignetteData
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"            // CgsGraphics::Ptr32 (the +0x80 import slot)
#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"

// ============================================================================
// GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h
//
// BrnWorld::EnvironmentSettings::Keyframe -- the runtime, 0x240-byte environment
// keyframe (the in-memory form of the 0x10012 "Environment Settings Keyframe"
// resource; its serialised size is the 0x240 the KeyframeResourceType descriptor
// reports). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Only the sub-blocks that this batch's EnvironmentManager code actually touches
// are named, at their asm-attested offsets; the intervening regions are explicit
// padding (layout-recovery-with-padding). The named offsets are cross-validated
// two ways:
//   * EnvironmentManager::SetupUpdateFromToolBlend (@0x827C5018) builds a Keyframe
//     on the stack and passes &kf[16]/&kf[48]/&kf[144]/&kf[320]/&kf[464] to
//     ParseEnvironmentFile as the Bloom / Vignette / Scattering / Lighting /
//     Clouds out-params (r-offsets 0x10 / 0x30 / 0x90 / 0x140 / 0x1D0).
//   * EnvironmentManager::PerformBlend (@0x827B0EB8) reads each source keyframe's
//     scattering(+0x90) / lighting(+0x140) / clouds(+0x1D0) sub-block.
//   * EnvironmentManager::GenerateEffects (@0x827BE698) reads the tint COLOUR-CUBE
//     IMPORT SLOT at +0x80 (`lwz r11, 0x80(kf)` -> `stw r11, 0x110(effectsFrame)`, i.e.
//     straight into BrnEffectsFrame::mTintData::mpColourCube). NAMED 2026-08-15 (bloom
//     wave); corrected 2026-08-16 -- the word is a resolved POINTER, not a resource id,
//     and the member below is mpColourCube. See its own banner for the import.
// The +0x00..+0x10 header, the rest of the +0x80..+0x90 gap (DoF/blur, deferred) and the
// small inter-block gaps are opaque padding rather than fabricated members.
// ============================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{

struct Keyframe
{
    // Keyframe::Construct @0x82676298 -- body in BrnEnvironmentKeyframe.cpp beside
    // this header (envfix round, 2026-08-16). Sets every sub-block to its default
    // template. Called by EnvironmentManager::SetupUpdateFromToolBlend @0x827C5064,
    // its only xref in the image.
    void Construct();

    // 0x000  the keyframe format version. NAMED from the DWARF
    // (SharedClasses/World/BrnEnvironmentKeyframe.h:58 `uint32_t muVersion`); the
    // guest's Construct stores the literal 8 here (`li r7,8` @0x826762B4 /
    // `stw r7,0(r31)` @0x826762D0), and the shipped ENV_KF_..._1200 resource
    // embedded in BrnEnvironmentKeyframeBringUp.cpp opens with 0x00000008.
    u32                       muVersion;      // 0x000
    u8                        mPad4[0xC];     // 0x004  rest of the header (deferred)
    BrnEffects::BloomData     mBloom;         // 0x010  (0x20)
    BrnEffects::VignetteData  mVignette;      // 0x030  (0x50)
    // 0x080  the keyframe's TINT COLOUR CUBE. DWARF BrnEnvironmentKeyframe.h:62 types this slot
    // `Keyframe::TintData mTintData` -- the typedef at BrnEnvironmentData.h:42 is
    // BrnEffects::TintData, i.e. the one-pointer struct the effects frame carries -- and
    // GenerateEffects @0x827BE698 copies it straight across (`lwz r11, 0x80(kf)` ->
    // `stw r11, 0x110(effectsFrame)`, a whole-struct copy of one word).
    //
    // ⭐ IT IS AN *IMPORT SLOT*, NOT AN ID (corrected 2026-08-16, group tintdata). The name
    // `muColourCube` and the comment "resource id" that stood here were wrong: the keyframe's
    // bundle carries a BundleV2 ImportEntry whose muOffset is this member, and
    // CgsResource::Pool::ResolveImportForEntry writes the imported ColourCube's MAIN-MEMORY
    // POINTER into it during load (KeyframeResourceType::FixUp @0x82678C40 clears the same
    // +128 word first). Every shipped keyframe imports the same cube, 0x8b7e999a
    // "ENV_CC_Paradise_ingame_junk ... TINT_Art_Style.psd" (envdata's dump, step 9).
    //
    // ⚠ WHY IT STAYS FOUR BYTES, i.e. why this is a Ptr32 and not `BrnEffects::TintData`.
    // The Keyframe is an IN-PLACE serialised resource: KeyframeResourceType::
    // GetSerialisedResourceDescriptor @0x8267D220 reports a FIXED 0x240-byte record and the
    // loader relocates it where it lies, so every offset in it is fixed by the shipped data.
    // Embedding the host BrnEffects::TintData (8-byte pointer) would push ScatteringData off
    // +0x90, LightingData off +0x140 and CloudsData off +0x1D0 -- all three asm-attested (see
    // the banner) -- and would need tools/assets/bundles/env_transcode.py to relay every
    // Keyframe 0x10012 out to a host layout and every bundle regenerated. THE ALTERNATIVE COSTS
    // NOTHING AND LOSES NOTHING: the slot holds a pointer that is guaranteed below 4 GB
    // (CgsMemory::LowMemory reserves the whole root allocation there and asserts if it cannot --
    // CgsLowMemoryPC.cpp:120 "Serialised resource pointer slots (PointerFromU32) WILL TRUNCATE"),
    // ResolveImportForEntry already picks the narrow store for exactly this case
    // (CgsResourcePool.cpp:648-664), and CgsGraphics::Ptr32<T> is the project's committed name
    // for that slot (CgsSerialisedPtr.h; same convention as CgsModel/CgsInstance). So the
    // on-disk record is untouched and the reader still gets a real, complete host pointer.
    CgsGraphics::Ptr32<rw::graphics::postfx::ColourCube> mpColourCube;   // 0x080
    u8                        mPad84[0xC];    // 0x084  DoF/blur sub-blocks (deferred)
    ScatteringData            mScattering;    // 0x090  (0xA8)
    u8                        mPad138[0x8];   // 0x138
    LightingData              mLighting;      // 0x140  (0x84)
    u8                        mPad1C4[0xC];   // 0x1C4
    CloudsData                mClouds;        // 0x1D0  (0x6C)
    u8                        mPad23C[0x4];   // 0x23C
};

// The pad arithmetic above depends on the exact sizeof of each embedded sub-block;
// pin them so a future layout change to those types breaks here loudly.
static_assert(sizeof(BrnEffects::BloomData)    == 0x20, "BloomData size drift breaks Keyframe layout");
static_assert(sizeof(BrnEffects::VignetteData) == 0x50, "VignetteData size drift breaks Keyframe layout");
static_assert(sizeof(ScatteringData)           == 0xA8, "ScatteringData size drift breaks Keyframe layout");
static_assert(sizeof(LightingData)             == 0x84, "LightingData size drift breaks Keyframe layout");
static_assert(sizeof(CloudsData)               == 0x6C, "CloudsData size drift breaks Keyframe layout");
static_assert(sizeof(Keyframe)                 == 0x240, "Keyframe serialised size must be 0x240");

}
}

#endif // BRN_ENVIRONMENT_KEYFRAME_H
