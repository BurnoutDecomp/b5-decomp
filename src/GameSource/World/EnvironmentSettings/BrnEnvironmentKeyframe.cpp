#include "GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h"

// =============================================================================
// GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.cpp
//
// The runtime Keyframe's own TU. Reconstructed from BURNOUT_X360_ARTIST.XEX.
// DWARF home: references/DecFIGS/dwarfdump/SharedClasses/World/
// BrnEnvironmentKeyframe.{h,cpp} (Keyframe::Construct at :53 / :48). The header
// lives beside this file under GameSource/World/EnvironmentSettings/ because
// that is where the tree already homes Keyframe (BrnEnvironmentKeyframe.h) and
// its bring-up twin (BrnEnvironmentKeyframeBringUp.cpp).
//
// The other two methods the DWARF declares on Keyframe -- ParseFile (:71) and
// EndianSwap (:90) -- are NOT reconstructed here: neither has a standalone symbol
// in the X360 image (a name scan of all 30,095 exported functions returns no
// `Keyframe::ParseFile` / `Keyframe::EndianSwap`; both were inlined into their
// single call sites, EnvironmentManager::SetupUpdateFromToolBlend @0x827C5018 and
// the keyframe resource type's serialise path respectively). Nothing in the tree
// references either, so no declaration is added for them.
// =============================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{

// The keyframe format version the guest stamps into +0x00 (`li r7, 8` /
// `stw r7, 0(r31)` @0x826762B4 / 0x826762D0). Corroborated by shipped data: the
// embedded ENV_KF_Paradise_ingame_junk_city_1200 resource in
// BrnEnvironmentKeyframeBringUp.cpp opens with 0x00000008.
const u32 KU_KEYFRAME_VERSION = 8;

// ---------------------------------------------------------------------------
// Keyframe::Construct @ 0x82676298 (232 bytes)
//
// Recovered from HOLES_DUMP.md (no .ida-exports JSON for this address; the
// conductor disassembled it from the .i64, and it was re-verified byte-for-byte
// against the decrypted shipped image in this round -- see REPORT section 1).
//
// The X360 compiler INLINED BrnEffects::BloomData::Construct,
// BrnEffects::VignetteData::Construct and BrnEffects::TintData::Construct (all
// three are header-inline in the tree today as well) and left the three
// EnvironmentSettings data Constructs as out-of-line calls. De-inlined here back
// to the six sub-block Constructs the DWARF names -- BrnEnvironmentKeyframe.cpp:48
// lists exactly BloomData::Construct, VignetteData::Construct,
// ScatteringData::Construct, TintData::Construct, LightingData::Construct,
// CloudsData::Construct.
//
// Store-by-store correspondence with the asm:
//   0x826762D0  stw  r7(=8), 0(r31)                        -> muVersion = 8
//   0x826762D8  stfs f0 , 0x10(r31)  flt_820A3A14 = 0.98f  -> mBloom.mfLuminance
//   0x826762E8  stfs f13, 0x14(r31)  flt_820A3A98 = 0.77f  -> mBloom.mfThreshold
//   0x82676308  stvx128       +0x20  <- unk_82FFAED0       -> mBloom.mv4Scale
//   0x82676310  stfs f12, 0x30(r31)  flt_82001CC0 = 0.0f   -> mVignette.mfAngle
//   0x82676318  stfs f11, 0x34(r31)  flt_820A3AA0 = 0.33f  -> mVignette.mfSharpness
//   0x82676324  stvx128       +0x40  <- unk_82FFAE20       -> mVignette.mv2Amount
//   0x82676334  stvx128       +0x50  <- unk_82FFAEC0       -> mVignette.mv2Centre
//   0x82676344  stvx128       +0x60  <- unk_82FFB220       -> mVignette.mv4InnerColour
//   0x82676350  stvx128       +0x70  <- unk_82FFB1A0       -> mVignette.mv4OuterColour
//   0x82676354  stw  r4(=0), 0x80(r31)                     -> the tint colour-cube id = 0
//   0x82676358  bl ScatteringData::Construct  (r3 = this + 0x090)
//   0x82676360  bl LightingData::Construct    (r3 = this + 0x140)
//   0x82676368  bl CloudsData::Construct      (r3 = this + 0x1D0)
//
// Every constant above is ALREADY in the tree, byte-identical, as the
// BrnEffects::{Bloom,Vignette}Data statics those two inline Constructs read
// (SharedClasses/Graphics/BrnEffectsData.{h,cpp}: kfDefLuminance 0.98 =
// flt_820A3A14, kfDefThreshold 0.77 = flt_820A3A98, kfDefAngle 0.0 = flt_820A3A9C
// (the guest reaches the same 0.0f here through flt_82001CC0), kfDefSharpness
// 0.33 = flt_820A3AA0, and the four unk_82FF* vectors). So calling the inline
// Constructs reproduces the guest store-for-store and invents nothing.
//
// CALLER: EnvironmentManager::SetupUpdateFromToolBlend @0x827C5064 -- the
// dev-only d:\LightSetup.txt tool path. It is the ONLY xref in the image.
// ---------------------------------------------------------------------------
void Keyframe::Construct()
{
    muVersion = KU_KEYFRAME_VERSION;

    mBloom.Construct();       // BrnEffects::BloomData::Construct    (inlined on X360)
    mVignette.Construct();    // BrnEffects::VignetteData::Construct (inlined on X360)

    // BrnEffects::TintData::Construct -- inlined on X360 to the single
    // `stw 0, 0x80(this)`. The tree models the keyframe's tint sub-block as the
    // SERIALISED (4-byte) colour-cube import slot rather than an embedded host
    // BrnEffects::TintData -- see the long note on BrnEnvironmentKeyframe.h's
    // mpColourCube for why the on-disk width has to stay the console's -- so the
    // de-inlined call is spelled as that one store, on the slot itself.
    mpColourCube.muSlot = 0;

    mScattering.Construct();
    mLighting.Construct();
    mClouds.Construct();
}

}
}
