// =============================================================================
// BrnEffectsBuildUVData.cpp  (out-of-line home for BrnEffects::Utils::BuildUVData::
//                             SetupFromMaterial @ 0x822780C8)
//
// BuildUVData is the per-material UV-atlas block the Lion blend renderer fills once before
// drawing a run of sprites/quads/tilts (callers BrnGraphics::LionBlendRenderer::RenderSprites
// / RenderQuads / RenderTilts, which then hand it to BuildUVs per particle). The TYPE now
// lives in its DWARF home, GameSource/Effects/BrnEffectsUtils.h:395 -- only the body is here.
//
// ⭐ TWO THINGS THIS FILE USED TO GET WRONG, both fixed 2026-09-05 from the DecFIGS DWARF:
//   1. The struct was defined INSIDE this .cpp, so no caller could name the type. That is why
//      the three draw halves could not declare their `BuildUVData lUVData` local. It is now in
//      the header the DWARF puts it in.
//   2. The source was modelled as a private "BuildUVMaterialSource" view struct with GUESSED
//      field names (texel count, texture width/height, frame columns/rows). The real parameter
//      is `const cParticleMaterial&` (DWARF BrnEffectsUtils.h:398) and every offset the asm
//      reads is an attested member of the already-committed 164-byte record:
//        +0x24 mFlags   +0x34 mFrameCount   +0x38 mXFrames   +0x39 mYFrames
//        +0x3F mUCoordOption   +0x40 mVCoordOption
//      So this is a FRAME-ATLAS descriptor: how many frames the material has, how many columns
//      the atlas is, and the reciprocals of the column/row counts that turn a frame index into
//      a UV offset. Nothing here is about texel dimensions.
//
// THE SETUP (asm @ 0x822780C8), reconstructed store-for-store:
//   mvfMaterialFrameCount        (+0x00) = splat4((f32)(s32) mFrameCount)   lwz 0x34 / extsw
//   mvfMaterialNumXFrames        (+0x10) = splat4((f32)(u8)  mXFrames)      lbz 0x38
//   mvfMaterialOneOverNumXFrames (+0x20) = splat4(1.0f / that)
//   mvfMaterialOneOverNumYFrames (+0x30) = splat4(1.0f / (f32)(u8) mYFrames) lbz 0x39
//   muMaterialFlags              (+0x40) = mFlags        (lwz 0x24 / stw 0x40, verbatim)
//   muMaterialUCoordOption       (+0x44) = mUCoordOption (lbz 0x3F)
//   muMaterialVCoordOption       (+0x45) = mVCoordOption (lbz 0x40)
// The reciprocal numerator is the shared rodata 1.0f (flt_82001C98). NOTE: the broadcast source
// for the first lane block is the SIGNED word at +0x34 while muMaterialFlags comes from the word
// at +0x24 -- the asm reads two different fields and they are not interchangeable.
// =============================================================================

#include "GameSource/Effects/BrnEffectsUtils.h"   // BrnEffects::Utils::BuildUVData (the DWARF home)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"

namespace BrnEffects
{
namespace Utils
{

// IEEE-754 1.0f, the shared reciprocal numerator (rodata flt_82001C98).
static const f32 KF_ONE = 1.0f;

static void BroadcastVecFloat(VecFloat& lrTarget, f32 lfValue)
{
    lrTarget.x = lfValue;
    lrTarget.y = lfValue;
    lrTarget.z = lfValue;
    lrTarget.w = lfValue;
}

// @ 0x822780C8   (DWARF BrnEffectsUtils.h:398)
void BuildUVData::SetupFromMaterial(const cParticleMaterial& arMaterial)
{
    // +0x00: (f32)(s32) frame count, broadcast across all four lanes.
    BroadcastVecFloat(mvfMaterialFrameCount, static_cast<f32>(arMaterial.mFrameCount));

    // +0x10: (f32)(u8) atlas column count, broadcast.
    const f32 lfNumXFrames = static_cast<f32>(arMaterial.mXFrames);
    BroadcastVecFloat(mvfMaterialNumXFrames, lfNumXFrames);

    // +0x20 / +0x30: the two reciprocals (1.0f / columns, 1.0f / rows), broadcast.
    BroadcastVecFloat(mvfMaterialOneOverNumXFrames, KF_ONE / lfNumXFrames);
    const f32 lfNumYFrames = static_cast<f32>(arMaterial.mYFrames);
    BroadcastVecFloat(mvfMaterialOneOverNumYFrames, KF_ONE / lfNumYFrames);

    // +0x40 / +0x44 / +0x45: the trailing scalars, copied verbatim.
    muMaterialFlags        = arMaterial.mFlags;
    muMaterialUCoordOption = arMaterial.mUCoordOption;
    muMaterialVCoordOption = arMaterial.mVCoordOption;
}

} // namespace Utils
} // namespace BrnEffects
