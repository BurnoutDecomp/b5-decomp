// =============================================================================
// BrnEffectsBuildUVData.cpp  (OWNING HOME for BrnEffects::Utils::BuildUVData)
//
// BuildUVData is the per-material UV-setup block the Lion blend renderer fills
// before drawing sprites/quads/tilts (callers BrnGraphics::LionBlendRenderer::
// RenderSprites / RenderQuads / RenderTilts). SetupFromMaterial reads a handful
// of fields off a material descriptor and broadcasts them into four Vector4
// lanes plus three trailing scalars.
//
// No prior source and no DecFIGS DWARF exist for this TU, so this is a minimal
// OWNING slice. The output layout is asm-attested (store-for-store) from
//   BrnEffects::Utils::BuildUVData::SetupFromMaterial @ 0x822780C8
// and the material source is modelled as a small named view over the offsets the
// asm reads (no raw-offset casts leak out of this file).
//
// THE SETUP (asm @ 0x822780C8), reconstructed store-for-store:
//   maTexels         (+0x00) = (f32)(s32) material[+0x34]            , broadcast (lwz 0x34/extsw)
//   maTextureWidth   (+0x10) = (f32)(u8)  material[+0x38]            , broadcast
//   maInvTextureW    (+0x20) = 1.0f / (f32)(u8) material[+0x38]      , bcast
//   maInvTextureH    (+0x30) = 1.0f / (f32)(u8) material[+0x39]      , bcast
//   muFlags          (+0x40) = material[+0x24]         (u32 word; lwz 0x24/stw 0x40)
//   muFrameColumns   (+0x44) = material[+0x3F]         (u8)
//   muFrameRows      (+0x45) = material[+0x40]         (u8)
// The reciprocal numerator is the shared rodata 1.0f (flt_82001C98). NOTE: the broadcast source
// is the signed word at material +0x34 and the verbatim muFlags source is the word at +0x24 --
// these two are NOT interchangeable (the asm reads +0x34 for the texel lanes, +0x24 for muFlags).
// =============================================================================

#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu float lanes)

namespace BrnEffects
{
namespace Utils
{

// IEEE-754 1.0f, the shared reciprocal numerator (rodata flt_82001C98).
static const f32 KF_ONE = 1.0f;

// Minimal named view over the material-descriptor fields the asm reads. The
// concrete material type is owned by another TU and read here only by these
// asm-attested offsets; modelling it as a named struct keeps every access
// inside this slice by-name rather than by raw cast.
struct BuildUVMaterialSource
{
    // @+0x24 (lwz): copied verbatim into muFlags(+0x40).
    u32 muFlags;
    // @+0x34 (lwz/extsw): read as a signed word and broadcast as (f32)(s32) into maTexels.
    s32 miTexelCount;
    // @+0x38 (lbz): texture width in texels (unsigned byte).
    u8  muTextureWidth;
    // @+0x39 (lbz): texture height in texels (unsigned byte).
    u8  muTextureHeight;
    // @+0x3F (lbz): frame-grid column count.
    u8  muFrameColumns;
    // @+0x40 (lbz): frame-grid row count.
    u8  muFrameRows;
};

struct BuildUVData
{
    // asm-attested store layout (see header block above).
    Vector4 maTexels;        // +0x00
    Vector4 maTextureWidth;  // +0x10
    Vector4 maInvTextureW;   // +0x20
    Vector4 maInvTextureH;   // +0x30
    u32     muFlags;         // +0x40
    u8      muFrameColumns;  // +0x44
    u8      muFrameRows;     // +0x45

    // @ 0x822780C8. Returns *this (the asm returns the result pointer in r3).
    BuildUVData& SetupFromMaterial(const BuildUVMaterialSource& lrMaterial);
};

static void BroadcastVector4(Vector4& lrTarget, f32 lfValue)
{
    lrTarget.x = lfValue;
    lrTarget.y = lfValue;
    lrTarget.z = lfValue;
    lrTarget.w = lfValue;
}

// @ 0x822780C8
BuildUVData& BuildUVData::SetupFromMaterial(const BuildUVMaterialSource& lrMaterial)
{
    // +0x00: (f32)(s32) texel count, broadcast across all four lanes.
    BroadcastVector4(maTexels, static_cast<f32>(lrMaterial.miTexelCount));

    // +0x10: (f32)(u8) texture width, broadcast.
    const f32 lfTextureWidth = static_cast<f32>(lrMaterial.muTextureWidth);
    BroadcastVector4(maTextureWidth, lfTextureWidth);

    // +0x20: reciprocal of the texture width (1.0f / width), broadcast.
    BroadcastVector4(maInvTextureW, KF_ONE / lfTextureWidth);

    // +0x30: reciprocal of the texture height (1.0f / height), broadcast.
    const f32 lfTextureHeight = static_cast<f32>(lrMaterial.muTextureHeight);
    BroadcastVector4(maInvTextureH, KF_ONE / lfTextureHeight);

    // +0x40 / +0x44 / +0x45: trailing scalars copied verbatim.
    muFlags        = lrMaterial.muFlags;
    muFrameColumns = lrMaterial.muFrameColumns;
    muFrameRows    = lrMaterial.muFrameRows;

    return *this;
}

} // namespace Utils
} // namespace BrnEffects
