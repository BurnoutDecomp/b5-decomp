// ============================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendRenderer.cpp
//
// BrnGraphics::LionBlendRenderer -- the concrete immediate-mode blend renderer that
// LionParticleRender drives. It OWNS a BrnGraphics::Im3dBlend by value at offset 0 (the DWARF
// says composition, not inheritance -- see BrnLionBlendRenderer.h for the whole byte table and
// the ParticleModule container bound that fixes sizeof at 0x1E0).
//
// Reconstructed here: EndRendering @0x8227E610, SetCameraData @0x822824F8 and
// BuildCameraOrientatedLocator @0x8227A478. BeginRendering is an ICF fold onto
// Im3dBlend::BeginRendering @0x82282060 and is an inline forward in the header (that body
// lives in BrnLionBlendIm3d.cpp). Still open: the two SetState overloads and the three draw
// halves -- GROW this file when they land, do NOT fork the header.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnLionBlendRenderer.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"  // CgsGraphics::ImRendererBase (mgpActiveRenderer)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"  // cParticleDescriptor::Material / Flags
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"   // cParticleBehaviour::mPivotPoint
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"    // cParticleMaterial (SetupFromMaterial)
#include "GameSource/Effects/BrnEffectsUtils.h"                 // BuildUVData / BuildUVs / FastMatrix33FromEulerXYZ / K_VECTOR4_*
#include "GameSource/Effects/Particles/Native/BrnLionBlendVertex.h"  // LionBlendVertex::VertexIterator (the vertex writer)
#include "SDKs/EATech/include/ps3/gcm/renderengine/stateparams.h"    // renderengine::RGBA8
#include "rw/math/vpu/matrix44affine_operation.h"               // rw::math::vpu::TransformPoint
#include "rw/math/vpu/vector3_operation.h"                      // Normalize / Magnitude / Dot / Cross / Lerp
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>   // sqrtf -- the two normalisations
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the one-shot CreateInternalMaterial announcement
#include "GameSource/Effects/Particles/LionParticleRender.h"   // LionParticleRender::CreateInternalMaterial / cParticleMaterial (the Lion trap stubs below)

namespace renderengine { class BlendMaterialState; }

// The process-wide default blend-state template bound at batch end (X360 .data home is the
// LionParticleRender TU; extern for the compile gate). Same static that
// LionParticleRenderMaterial.cpp / BrnSimpleParticleRenderer.cpp read their base blend
// parameters from.
extern renderengine::BlendMaterialState* dword_83010F20;

namespace BrnGraphics
{

// ---------------------------------------------------------------------------
// BrnGraphics::LionBlendRenderer::EndRendering  @ 0x8227E610
//
// End the immediate-mode blend batch: bind the shared default blend template through this
// renderer's own SetState (X360 word 10 `bl CgsGraphics::ImRendererBase::SetState` with
// r3 = this+4, r4 = dword_83010F20), then run the inlined
// CgsGraphics::ImRenderer<V>::EndRendering: assert this renderer is the active one and clear
// the active-renderer module static.
// Called by BrnParticle::LionParticleRender::EndRendering.
//
// ⭐ `this + 4` IS NOW A NAMED PATH. X360 word 7 (0x8227E62C `addi r30, r31, 4`) followed by
// the `if (this == 0) r30 = 0` guard at words 12-14 is MSVC's derived->base pointer adjustment
// for a NON-PRIMARY base: mRenderer sits at offset 0 and its own ImRendererBase subobject sits
// 4 bytes in, past the ImRenderer<V> vptr the DWARF spells `_vptr.ImRenderer`. With mRenderer
// modelled it is `&mRenderer` upcast, not a reinterpret_cast of `this`.
// ---------------------------------------------------------------------------
void LionBlendRenderer::EndRendering()
{
    // SetState(dword_83010F20): the BlendState overload against the immediate-mode renderer.
    // The X360 emits `ImRendererBase::SetState(&mRenderer + 4, dword_83010F20)`; the return
    // value (the renderer, X360 r3) is discarded.
    SetState(reinterpret_cast<const BlendState*>(dword_83010F20));

    // Inlined CgsGraphics::ImRenderer<V>::EndRendering (CgsImRenderer.h).
    CgsGraphics::ImRendererBase* lpBase = static_cast<CgsGraphics::ImRendererBase*>(&mRenderer);
    CGS_ASSERT(CgsGraphics::ImRendererBase::mgpActiveRenderer == lpBase,
               "mgpActiveRenderer == this");
    CgsGraphics::ImRendererBase::mgpActiveRenderer = nullptr;
}

// ---------------------------------------------------------------------------------------------
// BrnGraphics::LionBlendRenderer::SetCameraData  @ 0x822824F8   (DWARF BrnLionBlendRenderer.h:74)
//
// Store the frame's three camera matrices and derive the Lion-side camera transform from the
// back matrix. Called once per material group by LionParticleRender::RenderGroupBeginLite
// @0x822894C8, which passes `&mBackMat` / `&mViewMat` / `&mViewProjection` straight out of its
// own object (words 29-31: `addi r4,r30,0x60`, `addi r5,r30,0xA0`, `addi r6,r30,0xE0`).
//
// TWO HALVES, and the asm keeps them apart:
//
//   1. mCameraTransform (this+0xE0) is built SCALAR, three floats per row, with the fourth lane
//      FORCED: 0.0 on the three basis rows (flt_82001CC0, read out of the image as 00000000)
//      and 1.0 on the translation row (flt_82001C98 == 3F800000). Words 4-40 -- twelve `lfs`
//      from arBackMat at +0/4/8, +0x10/14/18, +0x20/24/28, +0x30/34/38 and sixteen `stfs` into
//      +0xE0..+0x11C. ⚠ The w lanes are NOT copied from arBackMat: the console overwrites them,
//      which is the whole point of the scalar path existing beside the vector path below.
//
//   2. mBackMat / mViewMat / mViewProjection are copied WHOLE, four `lvx128`/`stvx128` pairs
//      each at +0x00/+0x10/+0x20/+0x30 (words 43-58) -- w lanes included, verbatim.
//
// The two halves read arBackMat twice on purpose; the first is a lossy convert to the Lion
// cMatrix convention (w = 0,0,0,1), the second is the faithful copy the draw halves billboard
// against. Reproduced exactly -- collapsing them into one copy would silently change the w
// lanes mCameraTransform hands to BuildCameraOrientatedLocator.
// ---------------------------------------------------------------------------------------------
void LionBlendRenderer::SetCameraData(const rw::math::vpu::Matrix44Affine& arBackMat,
                                      const rw::math::vpu::Matrix44Affine& arViewMat,
                                      const rw::math::vpu::Matrix44& arViewProjection)
{
    // --- half 1: the scalar convert into the Lion cMatrix (asm words 4-40) ------------------
    mCameraTransform.xa.x = arBackMat.xAxis.x;
    mCameraTransform.xa.y = arBackMat.xAxis.y;
    mCameraTransform.xa.z = arBackMat.xAxis.z;
    mCameraTransform.xa.w = 0.0f;                 // flt_82001CC0

    mCameraTransform.ya.x = arBackMat.yAxis.x;
    mCameraTransform.ya.y = arBackMat.yAxis.y;
    mCameraTransform.ya.z = arBackMat.yAxis.z;
    mCameraTransform.ya.w = 0.0f;

    mCameraTransform.za.x = arBackMat.zAxis.x;
    mCameraTransform.za.y = arBackMat.zAxis.y;
    mCameraTransform.za.z = arBackMat.zAxis.z;
    mCameraTransform.za.w = 0.0f;

    mCameraTransform.wa.x = arBackMat.wAxis.x;
    mCameraTransform.wa.y = arBackMat.wAxis.y;
    mCameraTransform.wa.z = arBackMat.wAxis.z;
    mCameraTransform.wa.w = 1.0f;                 // flt_82001C98

    // --- half 2: the three verbatim 64-byte copies (asm words 43-58) -------------------------
    mBackMat         = arBackMat;
    mViewMat         = arViewMat;
    mViewProjection  = arViewProjection;
}

}  // namespace BrnGraphics

// =================================================================================================
// BrnGraphics::LionBlendRenderer::BuildCameraOrientatedLocator  @ 0x8227A478
//                                                    (DWARF BrnLionBlendRenderer.h:138)
//
// Build the camera-facing basis the three Render* shapes billboard their geometry against:
// take the emitter's locator frame, keep its translation, and replace its 3x3 with an
// orthonormal basis whose Z axis points from the locator at the camera.
//
// ⭐ IT IS A LOOK-AT WITH A CONSTANT WORLD UP, and the constant is hiding in plain sight as a
// register that never gets written. f0 is loaded with flt_82001CC0 == 0.0 at 0x8227A4C0 and
// stays 0.0 all the way to 0x8227A56C, so the first cross product at 0x8227A540..0x8227A550 --
// which reads like a general cross(A, N) -- is cross((0,1,0), N) == (N.z, 0, -N.x) with the
// two multiplies by A.x and A.z folded to zero. That is the ONLY place the up vector appears;
// there is no (0,1,0) literal anywhere in the function.
//
//     N = normalize(cameraPos - locatorPos)      -- 0x8227A4E8..0x8227A53C
//     R = normalize(cross(worldUp, N))           -- 0x8227A540..0x8227A588
//     U = cross(N, R)                            -- 0x8227A5C4..0x8227A5D8
//
// ⚠ THE SECOND CROSS IS cross(N, R), NOT cross(R, N), and the sign is the whole difference
// between a right- and a left-handed basis. The three fmsubs at 0x8227A5C4/CC/D4 spell out
// (R.z*N.y - R.y*N.z, N.z*R.x - R.z*N.x, R.y*N.x - N.y*R.x), which is the NEGATION of
// cross(R, N) term for term.
//
// ⚠ BOTH NORMALISATIONS GUARD ON EXACTLY 0.0 AND LEAVE THE UNNORMALISED VALUE (0x8227A51C /
// 0x8227A564 are `fcmpu` against 0.0 with the divide skipped, not clamped) -- so a degenerate
// case yields a zero row rather than a NaN one. Reproduced as asked.
//
// The three basis rows are stored through unk_8200DCE0, read out of the image as
// { FFFFFFFF, FFFFFFFF, FFFFFFFF, 00000000 } -- the same xyz-keep / w-drop selector
// cParticleLocator::GetMat uses -- so each row lands with w == 0. The translation row is the
// locator's own, copied verbatim before any of this and never masked.
//
// ⚠ `this` IS UNUSED: r3 is overwritten by the locator load at 0x8227A49C before anything
// reads it. It is still a non-static member (the DWARF says so, and so does the r3 slot).
// =================================================================================================
namespace BrnGraphics
{
    void LionBlendRenderer::BuildCameraOrientatedLocator(cMatrix& arOut,
                                                         const cParticleEmitter* apEmitter,
                                                         const cMatrix& arCameraTransform,
                                                         const cTime& arTime)
    {
        // asm 0x8227A48C..0x8227A4A0 -- the emitter's locator, sampled at this frame's time.
        const cParticleLocator* lpLocator = apEmitter->GetBindings().GetpLocator();
        const cMatrix& lrLocator = lpLocator->GetMat(arTime);

        // asm 0x8227A4A8..0x8227A4E0 -- the whole locator frame first, including the
        // translation row the basis rows below then overwrite.
        arOut = lrLocator;

        // asm 0x8227A4E8..0x8227A53C -- N, the axis from the locator to the camera.
        f32 lfNx = arCameraTransform.wa.x - lrLocator.wa.x;
        f32 lfNy = arCameraTransform.wa.y - lrLocator.wa.y;
        f32 lfNz = arCameraTransform.wa.z - lrLocator.wa.z;
        {
            const f32 lfLength = sqrtf(lfNx * lfNx + lfNy * lfNy + lfNz * lfNz);
            if (lfLength != 0.0f)
            {
                const f32 lfInv = 1.0f / lfLength;
                lfNx *= lfInv;
                lfNy *= lfInv;
                lfNz *= lfInv;
            }
        }

        // asm 0x8227A540..0x8227A588 -- R = normalize(cross(worldUp, N)), worldUp == (0,1,0).
        f32 lfRx = lfNz;
        f32 lfRy = 0.0f;
        f32 lfRz = -lfNx;
        {
            const f32 lfLength = sqrtf(lfRx * lfRx + lfRy * lfRy + lfRz * lfRz);
            if (lfLength != 0.0f)
            {
                const f32 lfInv = 1.0f / lfLength;
                lfRx *= lfInv;
                lfRy *= lfInv;
                lfRz *= lfInv;
            }
        }

        // asm 0x8227A5BC / 0x8227A5E8 / 0x8227A5F4 -- the three rows, each w-masked to 0.
        arOut.xa.x = lfRx;
        arOut.xa.y = lfRy;
        arOut.xa.z = lfRz;
        arOut.xa.w = 0.0f;

        arOut.ya.x = lfRz * lfNy - lfRy * lfNz;
        arOut.ya.y = lfNz * lfRx - lfRz * lfNx;
        arOut.ya.z = lfRy * lfNx - lfNy * lfRx;
        arOut.ya.w = 0.0f;

        arOut.za.x = lfNx;
        arOut.za.y = lfNy;
        arOut.za.z = lfNz;
        arOut.za.w = 0.0f;
    }
}  // namespace BrnGraphics

// =================================================================================================
// THE DRAW PATH -- the shared quad emitter and the two billboard shapes.        (2026-09-05)
//
// Everything below is decoded from the X360 asm, with the DecFIGS DWARF supplying the local
// NAMES (references/DecFIGS/dwarfdump/.../BrnLionBlendRenderer.cpp names every local of
// QuadDraw:59, RenderSprites:223 and RenderQuads:327, in the order the asm computes them).
//
// ⭐⭐ THE THING THAT UNBLOCKED THESE: SEVEN CONSTANTS AND A SELECTOR. Two previous waves read
// these three functions and refused them, because ~31% of RenderSprites is VMX128 driven by
// rodata that reads as zero. It reads as zero because it is dynamically-initialised .bss, not
// because it is unreadable -- tools/re/findinit.py + ppcdis.py + x360rd.py recover every one
// (the chain is written out in full over FastMatrix33FromEulerXYZ in BrnEffectsUtils.cpp).
// Once the sin/cos coefficients are numbers, the three rotation paths and the matrix builder
// all reduce to ordinary arithmetic and cross-check each other.
//
// ⭐ THE ONE SELECTOR THAT SHAPES EVERY CORNER: unk_82CDA350 is ordinary .rdata and reads
//   { 00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03 }
// so `vperm(vA, vB, sel)` == (A.x, B.y, A.x, A.x). Both operands are always splats here, so the
// whole idiom is "make the 2D point (thisX, thatY)". Every `vrlimi128 vD, <zero>, 2, 0` beside
// it clears lane z (mask 8/4/2/1 == x/y/z/w), which is why the corners are (x, y, 0, junk) and
// why QuadDraw is free to overwrite lane w with the frame-blend weight.
//
// ⚠ THE OTHER RECURRING IDIOM, so nobody has to re-derive it: `vspltisw128 v,-1` + `vslw v,v,v`
// builds splat4(0x80000000); `vandc x, mask` is therefore fabs(); `vcmpgtfp` against
// splat(unk_8200D990 == 0x34000000 == FLT_EPSILON) is `!IsZero(x)`; and the
// `vperm` through splat4(0x0004080C) + `stvx128` + `lwz` that follows is the standard
// "did ANY lane compare true" reduction. Since every operand is a broadcast scalar, all four
// lanes carry the same answer and the reduction is exactly the scalar test written below.
// =================================================================================================
namespace
{
    // ---------------------------------------------------------------------------------------
    // BrnEffects::Utils::ConvertVector4ToRwRgbaOverbright -- inlined into QuadDraw by the X360
    // compiler (DWARF BrnEffectsUtils.h:326 names it; it has NO row in the X360 ledger, so it
    // is outlined here rather than minted as public surface the target build does not contain).
    //
    // asm 0x82282370..0x822823E0. The console does it in two steps and this is the only place
    // the two byte orders matter, so both are spelled out:
    //   1. scaled = min(aPart.mvColour * K_VECTOR4_511_511_511_255, K_VECFLOAT_255), then
    //      `vctuxs ...,0` -- convert to unsigned fixed point, TRUNCATING toward zero and
    //      saturating a negative lane to 0. The four lanes are then packed into one word as
    //      (A<<24)|(B<<16)|(G<<8)|R, which is rw::RGBA's own byte order (A,B,G,R in memory).
    //   2. asm 0x82282414..0x8228245C rotates that word to (R<<24)|(G<<16)|(B<<8)|A before
    //      every VertexIterator::Write -- i.e. renderengine::RGBA8's { u8 r, g, b, a; }.
    // The composition of the two is the identity on the channels, so the host writes the
    // RGBA8 directly; the console's intermediate is documented, not reproduced, because
    // reproducing it would mean inventing a packing for rw::RGBA (whose ctor has no body in
    // this tree) purely to unpack it again one instruction later.
    //
    // ⚠ THE 511 IS NOT A TYPO AND NOT SHARED WITH ALPHA. RGB scale by 511 and alpha by 255 --
    // that asymmetry is the entire content of the name "Overbright": a 0.5 colour channel
    // saturates to full white, giving the artist one stop of headroom above 1.0.
    inline renderengine::RGBA8 ConvertVector4ToRwRgbaOverbright(const cVector& arColour)
    {
        const rw::math::vpu::Vector4& lrScale = BrnEffects::Utils::K_VECTOR4_511_511_511_255;
        const f32 lfMax = BrnEffects::Utils::K_VECFLOAT_255.x;

        const f32 lafScaled[4] =
        {
            arColour.x * lrScale.x, arColour.y * lrScale.y,
            arColour.z * lrScale.z, arColour.w * lrScale.w
        };

        u8 lauChannel[4];
        for (u32 luLane = 0; luLane < 4u; ++luLane)
        {
            // vminfp is defined as `A < B ? A : B`, so an unordered lane takes B (255) -- the
            // test is written that way round rather than as `> lfMax` for that reason. vctuxs
            // then truncates toward zero and saturates a negative lane to 0.
            f32 lfLane = lafScaled[luLane];
            if (!(lfLane < lfMax))
            {
                lfLane = lfMax;
            }
            lauChannel[luLane] = (lfLane > 0.0f) ? static_cast<u8>(lfLane) : static_cast<u8>(0);
        }

        return renderengine::RGBA8(lauChannel[0], lauChannel[1], lauChannel[2], lauChannel[3]);
    }

    // ---------------------------------------------------------------------------------------
    // BrnGraphics::LionBlendRenderer::MatrixConvert (DWARF BrnLionBlendRenderer.cpp:96) --
    // the Lion cMatrix to engine Matrix44Affine reinterpretation. Both are four 16-byte rows
    // in the same order, so the console emits no conversion at all: RenderSprites just
    // `lvx128`s the cMatrix rows straight into the transform cascade
    // (0x822829E4/0x822829E8/0x822829F0/0x82282A00). Written as a copy here because the two
    // C++ types are distinct; it is a member on the console and file-local here because it
    // has no row in the X360 ledger (fully inlined at all three sites).
    inline rw::math::vpu::Matrix44Affine MatrixConvert(const cMatrix& arMatIn)
    {
        rw::math::vpu::Matrix44Affine lResult;
        lResult.xAxis.x = arMatIn.xa.x; lResult.xAxis.y = arMatIn.xa.y;
        lResult.xAxis.z = arMatIn.xa.z; lResult.xAxis.w = arMatIn.xa.w;
        lResult.yAxis.x = arMatIn.ya.x; lResult.yAxis.y = arMatIn.ya.y;
        lResult.yAxis.z = arMatIn.ya.z; lResult.yAxis.w = arMatIn.ya.w;
        lResult.zAxis.x = arMatIn.za.x; lResult.zAxis.y = arMatIn.za.y;
        lResult.zAxis.z = arMatIn.za.z; lResult.zAxis.w = arMatIn.za.w;
        lResult.wAxis.x = arMatIn.wa.x; lResult.wAxis.y = arMatIn.wa.y;
        lResult.wAxis.z = arMatIn.wa.z; lResult.wAxis.w = arMatIn.wa.w;
        return lResult;
    }

    // ---------------------------------------------------------------------------------------
    // QuadDraw  @ 0x82282330    (DWARF BrnLionBlendRenderer.cpp:59)
    //
    // Emit one particle's quad: four vertices sharing a colour, each carrying its own UV and a
    // position whose w lane is the frame-blend weight.
    //
    // ⚠ THE FIFTH PARAMETER IS REAL AND UNUSED. All three draw halves set r7 before the call
    // (RenderSprites 0x82282A18, RenderQuads 0x82282ED8, RenderTilts 0x8228395C) and the body
    // never reads it -- the DWARF declares it (`const cParticleEmitter& aEmitter`), so it is a
    // parameter this build's code path happens not to need, not a phantom.
    //
    // ⭐ THE EMISSION ORDER IS 0, 1, 3, 2 -- not 0..3. The four `bl VertexIterator::Write` at
    // 0x82282480 / 0x8228249C / 0x822824B8 / 0x822824D4 take their position from r28+0x00,
    // +0x10, +0x30, +0x20 and their UV from the matching laUvUv slot, so the quad is wound
    // (X0Y0), (X0Y1), (X1Y1), (X1Y0) -- a loop, not a strip pair. Getting this wrong swaps a
    // triangle and shows a bow-tie.
    void QuadDraw(BrnGraphics::LionBlendVertex::VertexIterator& arVertexIterator,
                  const BrnEffects::Utils::BuildUVData& arUVData,
                  const RenderedParticle& arPart,
                  const rw::math::vpu::Vector3* apPos,
                  const cParticleEmitter& arEmitter)
    {
        (void)arEmitter;   // see the note above -- declared by the DWARF, unread by this build.

        // asm 0x82282370..0x822823E0 (DWARF: RGBA lColour, :61).
        const renderengine::RGBA8 lColour = ConvertVector4ToRwRgbaOverbright(arPart.mvColour);

        // asm 0x822823E4 (DWARF: Vector4 laUvUv[4], :63). The two frame arguments ride in v1/v2
        // -- `vspltw v1, [aPart+0x30], 3` is Frame() and `vspltw v2, [aPart+0x40], 3` is
        // NextFrame(), both broadcast (0x82282394 / 0x822823C4).
        rw::math::vpu::Vector4 laUvUv[4];
        VecFloat lvfFrame;
        VecFloat lvfNextFrame;
        lvfFrame.x = lvfFrame.y = lvfFrame.z = lvfFrame.w = arPart.Frame();
        lvfNextFrame.x = lvfNextFrame.y = lvfNextFrame.z = lvfNextFrame.w = arPart.NextFrame();
        BrnEffects::Utils::BuildUVs(arUVData, lvfFrame, lvfNextFrame, laUvUv);

        // asm 0x822823E8..0x82282410 (DWARF: VecFloat lvfWeight, :69). `vspltisw v0, 0` runs
        // BEFORE the test, so a material without the multi-frame bit contributes a hard zero.
        f32 lfWeight = 0.0f;
        if ((arUVData.muMaterialFlags
                & BrnEffects::Utils::BuildUVData::KU_MATERIAL_FLAG_INTERFRAMEBLEND) != 0u)
        {
            const f32 lfFrame = arPart.Frame();
            lfWeight = lfFrame - floorf(lfFrame);   // vrfim + vsubfp
        }

        // asm 0x82282420 / 0x82282460 / 0x82282470 / 0x8228247C: `vrlimi128 vD, v0, 1, 0`
        // replaces ONLY lane w (mask 1) of each corner (DWARF: lPosPlusWeight0..3, :76-:79).
        rw::math::vpu::Vector4 laPosPlusWeight[4];
        for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
        {
            laPosPlusWeight[luCorner].x = apPos[luCorner].x;
            laPosPlusWeight[luCorner].y = apPos[luCorner].y;
            laPosPlusWeight[luCorner].z = apPos[luCorner].z;
            laPosPlusWeight[luCorner].w = lfWeight;
        }

        arVertexIterator.Write(laPosPlusWeight[0], lColour, laUvUv[0]);
        arVertexIterator.Write(laPosPlusWeight[1], lColour, laUvUv[1]);
        arVertexIterator.Write(laPosPlusWeight[3], lColour, laUvUv[3]);
        arVertexIterator.Write(laPosPlusWeight[2], lColour, laUvUv[2]);
    }

    // ---------------------------------------------------------------------------------------
    // The billboard corner set both shapes build, and the three rotation paths they choose
    // between. Shared here because RenderSprites @0x82282608 and RenderQuads @0x82282B28 emit
    // byte-identical code for it (0x8228272C..0x822829CC vs 0x82282C04..0x82282EC0), differing
    // only in register allocation -- one function's worth of logic that the compiler duplicated,
    // not two different algorithms.
    //
    // ⚠ lvfX0 IS COMPUTED FROM lvfX1, NOT FROM THE PIVOT. The asm is
    //     lvfX1 = size.x - size.x * pivot.x        (vmulfp128 then vsubfp)
    //     lvfX0 = lvfX1 - size.x                   (a SECOND vsubfp, 0x82282780)
    // and the DWARF's line numbers agree (X1 at :255/:348, X0 three lines later at :258/:351).
    // `-size.x * pivot.x` is the same value in exact arithmetic and a different one in floats,
    // so the round trip is kept.
    void BuildBillboardCorners(rw::math::vpu::Vector3* apPointsOut,
                               const RenderedParticle& arPart,
                               const cVector& arPivot)
    {
        const f32 lfX1 = arPart.mvSizePlusNextFrame.x - arPart.mvSizePlusNextFrame.x * arPivot.x;
        const f32 lfY1 = arPart.mvSizePlusNextFrame.y - arPart.mvSizePlusNextFrame.y * arPivot.y;
        const f32 lfX0 = lfX1 - arPart.mvSizePlusNextFrame.x;
        const f32 lfY0 = lfY1 - arPart.mvSizePlusNextFrame.y;

        // splat(unk_8200D990) == FLT_EPSILON; rw::math::vpu::IsZero is |v| <= that.
        static const f32 KF_IS_ZERO_EPSILON = 1.1920928955078125e-07f;
        const cVector& lrRot = arPart.mvRotPlusFrame;
        const bool lbRotX = (fabsf(lrRot.x) > KF_IS_ZERO_EPSILON);
        const bool lbRotY = (fabsf(lrRot.y) > KF_IS_ZERO_EPSILON);
        const bool lbRotZ = (fabsf(lrRot.z) > KF_IS_ZERO_EPSILON);

        if (lbRotX || lbRotY)
        {
            // asm 0x82282928 -- the general case: a full Euler XYZ basis, then each corner
            // through it. The z multiply is emitted even though every corner's z is the zero
            // the `vrlimi128 ..., 2, 0` masks in, so it is kept.
            rw::math::vpu::Vector3 lv3Rot;
            lv3Rot.x = lrRot.x; lv3Rot.y = lrRot.y; lv3Rot.z = lrRot.z; lv3Rot.w = 0.0f;
            const rw::math::vpu::Matrix33 lRotMat =
                BrnEffects::Utils::FastMatrix33FromEulerXYZ(lv3Rot);

            const f32 lafCornerX[4] = { lfX0, lfX0, lfX1, lfX1 };
            const f32 lafCornerY[4] = { lfY0, lfY1, lfY0, lfY1 };
            for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
            {
                const f32 lfPx = lafCornerX[luCorner];
                const f32 lfPy = lafCornerY[luCorner];
                const f32 lfPz = 0.0f;
                apPointsOut[luCorner].x = lRotMat.xAxis.x * lfPx + lRotMat.yAxis.x * lfPy
                                        + lRotMat.zAxis.x * lfPz;
                apPointsOut[luCorner].y = lRotMat.xAxis.y * lfPx + lRotMat.yAxis.y * lfPy
                                        + lRotMat.zAxis.y * lfPz;
                apPointsOut[luCorner].z = lRotMat.xAxis.z * lfPx + lRotMat.yAxis.z * lfPy
                                        + lRotMat.zAxis.z * lfPz;
                apPointsOut[luCorner].w = 0.0f;
            }
        }
        else if (lbRotZ)
        {
            // asm 0x82282828 -- a z-only spin needs one sin/cos pair, so the console inlines
            // the same polynomial FastMatrix33FromEulerXYZ uses rather than calling it.
            // ⭐ THIS PATH AND THE ONE ABOVE AGREE: substituting sx = sy = 0, cx = cy = 1 into
            // the matrix rows gives exactly (cz, sz, 0) / (-sz, cz, 0) / (0, 0, 1), which is
            // the rotation written out here term for term. Two independently decoded VMX blocks
            // arriving at the same expression is what makes the sin/cos lane assignment safe.
            f32 lfSin, lfCos;
            BrnEffects::Utils::SinCosCycles(lrRot.z, lfSin, lfCos);

            const f32 lafCornerX[4] = { lfX0, lfX0, lfX1, lfX1 };
            const f32 lafCornerY[4] = { lfY0, lfY1, lfY0, lfY1 };
            for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
            {
                apPointsOut[luCorner].x = lafCornerX[luCorner] * lfCos
                                        - lafCornerY[luCorner] * lfSin;
                apPointsOut[luCorner].y = lafCornerX[luCorner] * lfSin
                                        + lafCornerY[luCorner] * lfCos;
                apPointsOut[luCorner].z = 0.0f;
                apPointsOut[luCorner].w = 0.0f;
            }
        }
        else
        {
            // asm 0x822828F0 -- no rotation at all: the four `vperm` weaves straight from the
            // extents. Corner order is (X0,Y0), (X0,Y1), (X1,Y0), (X1,Y1).
            const f32 lafCornerX[4] = { lfX0, lfX0, lfX1, lfX1 };
            const f32 lafCornerY[4] = { lfY0, lfY1, lfY0, lfY1 };
            for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
            {
                apPointsOut[luCorner].x = lafCornerX[luCorner];
                apPointsOut[luCorner].y = lafCornerY[luCorner];
                apPointsOut[luCorner].z = 0.0f;
                apPointsOut[luCorner].w = 0.0f;
            }
        }
    }
}  // anonymous namespace

namespace BrnGraphics
{

// =================================================================================================
// BrnGraphics::LionBlendRenderer::RenderSprites  @ 0x82282608   (DWARF BrnLionBlendRenderer.cpp:223)
//
// Draw a run of CAMERA-FACING sprites: each particle's quad is built flat in 2D, spun by its own
// rotation, then planted at the particle's world position using the CAMERA's basis -- so it always
// faces the viewer. Compare RenderQuads below, which is the same code up to the last twelve
// instructions and orients the quad in the emitter's own frame instead.
//
// ⭐ THE ONE THING THAT MAKES THIS FUNCTION LOOK STRANGE, AND IT IS REAL: the console writes the
// particle's world position INTO ITS OWN MEMBER, mBackMat.wAxis, and then transforms all four
// corners through mBackMat. asm 0x8228269C parks `this + 0x150` in a stack slot before the loop,
// 0x82282A48 reloads it and 0x82282A58 `stvx128 v0, r0, r11` stores the transformed centre there;
// 0x82282A60 onwards then reads mBackMat's four rows (r31 == this + 0x120) as the billboard
// transform. mBackMat is the camera back matrix -- its 3x3 is exactly the camera's right/up/forward
// basis -- so the composite "rotate the flat quad into screen space, translate to the particle" is
// one four-row transform once the translation row is swapped. It leaves mBackMat.wAxis holding the
// last particle's position on exit; nothing reads it before the next SetCameraData or the next
// particle overwrites it. Reproduced, because a local copy would be a different function.
//
// ⚠ AND THE VERTEX-SPACE GUARD IS ONLY ON THIS SHAPE. RenderSprites early-outs unless the iterator
// has 4 vertices per particle free (asm words 6-21: (top - current) / stride vs `slwi r10, r24, 2`,
// unsigned). RenderQuads @0x82282B28 has no such check -- its prologue goes straight to
// SetupFromMaterial. That asymmetry is in the binary, not an omission here.
// =================================================================================================
void LionBlendRenderer::RenderSprites(EffectsVertexBufferIterator& arIterator,
                                      RenderedParticle* apParticle, const cMatrix* apMatrix,
                                      U32 auCount, const cParticleEmitter* apEmitter,
                                      const cTime& arTime)
{
    // arTime is a parameter of all three shapes; only RenderTilts reads it (it needs the
    // locator sampled at this frame's time). Here r9 is never touched.
    (void)arTime;

    LionBlendVertex::VertexIterator& lrLionBlendVertexIterator =
        static_cast<LionBlendVertex::VertexIterator&>(arIterator);

    // asm words 6-21 -- `twllei r8, 0` traps a zero stride, then the unsigned compare.
    // ⚠ ONE STATED HOST DIVERGENCE, and it is in the shared accessor, not here: the committed
    // EffectsVertexBufferIterator::GetVerticesFree returns 0 when top <= current, whereas the
    // console's `subf` + `divwu` would underflow to a huge unsigned and sail past this guard.
    // The host is the safer of the two on a malformed iterator; noted rather than forked.
    const U32 luVerticesFree = arIterator.GetVerticesFree();
    if (luVerticesFree < auCount * 4u)
    {
        return;
    }

    // asm 0x8228265C..0x82282668 -- descriptor +0x1F8, its material +0x4C.
    const cParticleDescriptor& lrDescriptor = *apEmitter->GetDescriptor();
    cParticleMaterial* lpMaterial = lrDescriptor.Material();
    BrnEffects::Utils::BuildUVData lUVData;
    lUVData.SetupFromMaterial(*lpMaterial);

    // asm 0x8228266C..0x82282680 -- behaviour +0x20C, its mPivotPoint at +0xF0. Only the x and
    // y lanes are ever read (`vspltw v11, v0, 0` / `vspltw v10, v0, 1`).
    const cParticleBehaviour& lrBehaviour = *apEmitter->GetCurrentBehaviour();
    const cVector& lrPivot = lrBehaviour.mPivotPoint;

    for (U32 luIndex = 0; luIndex < auCount; ++luIndex)
    {
        const RenderedParticle& lrPart = apParticle[luIndex];

        // asm 0x8228272C..0x822829CC.
        rw::math::vpu::Vector3 laPoints[4];
        BuildBillboardCorners(laPoints, lrPart, lrPivot);

        // asm 0x822829D8..0x82282A58 -- the particle centre through its own matrix, parked in
        // the billboard transform's translation row (see the note above).
        const rw::math::vpu::Matrix44Affine lConvertedXform = MatrixConvert(apMatrix[luIndex]);
        rw::math::vpu::Vector3 lv3Centre;
        lv3Centre.x = lrPart.mPos.x; lv3Centre.y = lrPart.mPos.y;
        lv3Centre.z = lrPart.mPos.z; lv3Centre.w = 0.0f;
        mBackMat.wAxis = rw::math::vpu::TransformPoint(lConvertedXform, lv3Centre);

        // asm 0x82282A60..0x82282AFC -- the four corners through the camera basis.
        for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
        {
            laPoints[luCorner] = rw::math::vpu::TransformPoint(mBackMat, laPoints[luCorner]);
        }

        QuadDraw(lrLionBlendVertexIterator, lUVData, lrPart, laPoints, *apEmitter);
    }
}

// =================================================================================================
// BrnGraphics::LionBlendRenderer::RenderQuads  @ 0x82282B28   (DWARF BrnLionBlendRenderer.cpp:327)
//
// Draw a run of quads oriented in the EMITTER's frame rather than the camera's: the flat corner
// set is spun by the particle's own rotation exactly as for sprites, offset by the particle's
// position IN THAT LOCAL SPACE, and only then transformed by the per-particle matrix.
//
// ⭐ THAT ORDER IS THE ENTIRE DIFFERENCE FROM RenderSprites, and it is four instructions:
// 0x82282EF0..0x82282EFC add `lPart.mPos` to each corner BEFORE the matrix cascade
// (0x82282F18..0x82282F94), whereas RenderSprites transformed the centre alone and used the
// camera basis for the corners. Sprites therefore stay square-on to the viewer and quads lie in
// whatever plane the emitter's matrix puts them.
//
// ⚠ NO VERTEX-SPACE GUARD -- see the note on RenderSprites. This shape trusts the caller.
//
// ⚠ apMatrix IS AN ARRAY, ONE ENTRY PER PARTICLE. Both shapes advance it by 0x40 -- one cMatrix
// -- alongside the 0x70 particle stride every iteration (0x82282FA4/0x82282FA8 here,
// 0x82282B08/0x82282B0C in RenderSprites).
// =================================================================================================
void LionBlendRenderer::RenderQuads(EffectsVertexBufferIterator& arIterator,
                                    RenderedParticle* apParticle, const cMatrix* apMatrix,
                                    U32 auCount, const cParticleEmitter* apEmitter,
                                    const cTime& arTime)
{
    (void)arTime;   // r9 is never read on this shape either.

    LionBlendVertex::VertexIterator& lrLionBlendVertexIterator =
        static_cast<LionBlendVertex::VertexIterator&>(arIterator);

    // asm 0x82282B50..0x82282B5C.
    const cParticleDescriptor& lrDescriptor = *apEmitter->GetDescriptor();
    cParticleMaterial* lpMaterial = lrDescriptor.Material();
    BrnEffects::Utils::BuildUVData lUVData;
    lUVData.SetupFromMaterial(*lpMaterial);

    // asm 0x82282B60..0x82282B74.
    const cParticleBehaviour& lrBehaviour = *apEmitter->GetCurrentBehaviour();
    const cVector& lrPivot = lrBehaviour.mPivotPoint;

    for (U32 luIndex = 0; luIndex < auCount; ++luIndex)
    {
        const RenderedParticle& lrPart = apParticle[luIndex];

        // asm 0x82282C04..0x82282EC0 -- identical to the sprite path.
        rw::math::vpu::Vector3 laPoints[4];
        BuildBillboardCorners(laPoints, lrPart, lrPivot);

        // asm 0x82282EF0..0x82282EFC (DWARF: Vector3 lConvertedPos, :404).
        rw::math::vpu::Vector3 lConvertedPos;
        lConvertedPos.x = lrPart.mPos.x; lConvertedPos.y = lrPart.mPos.y;
        lConvertedPos.z = lrPart.mPos.z; lConvertedPos.w = 0.0f;
        for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
        {
            laPoints[luCorner].x += lConvertedPos.x;
            laPoints[luCorner].y += lConvertedPos.y;
            laPoints[luCorner].z += lConvertedPos.z;
        }

        // asm 0x82282F18..0x82282F94 (DWARF: Matrix44Affine lConvertedXform, :410).
        const rw::math::vpu::Matrix44Affine lConvertedXform = MatrixConvert(apMatrix[luIndex]);
        for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
        {
            laPoints[luCorner] = rw::math::vpu::TransformPoint(lConvertedXform, laPoints[luCorner]);
        }

        QuadDraw(lrLionBlendVertexIterator, lUVData, lrPart, laPoints, *apEmitter);
    }
}

// =================================================================================================
// BrnGraphics::LionBlendRenderer::RenderTilts  @ 0x82282FC8   (DWARF BrnLionBlendRenderer.cpp:430)
//
// Draw a run of MOTION-ALIGNED particles: each one spans the segment from its previous position
// (mPos) to its current one (mPos1), and the shape drawn along that segment depends on two
// authoring flags. 639 instructions and TWO complete draw loops that share only a prologue.
//
// ⭐⭐ THE FLAGS ARE NAMED BY THE GAME'S OWN AUTHORING TOKEN TABLE, NOT INFERRED. The X360
// cLionTokenTable (transcribed in LionParticleParser.cpp) binds, on cParticleBehaviour::mFlags
// (+708 == +0x2C4):   DO_ENDON_SPRITE 0x00400000   DO_ENDON_ACTIVE 0x00800000
// and on the four floats this function broadcasts in its prologue:
//   +1128 (0x468) END_ON_ALPHA_FADE   +1132 (0x46C) END_ON_SCALE
//   +1136 (0x470) END_ON_START_ANGLE  +1140 (0x474) END_ON_END_ANGLE
// The DecFIGS DWARF independently names this function's four locals lvfEndOnAlphaFade /
// lvfEndOnScale / lvfEndOnStartAngle / lvfEndOnEndAngle, and the committed cParticleBehaviour
// record already had those members at exactly those offsets. THREE independent derivations of
// the same four fields -- which is what made this function decodable at all, because "end on"
// explains every branch in it.
//
//   * BOTH flags set  -> the END-ON SPRITE loop (0x82283148). A flat, z-spun, camera-facing
//     quad planted at the MIDPOINT of the segment, whose alpha ramps UP from 0 to 1 as the
//     view direction lines up with the segment. That is the point of the name: when you look
//     down the ribbon it degenerates, so a round sprite fades in to replace it.
//   * otherwise       -> the RIBBON loop (0x822835B8). A quad stretched along the segment,
//     its width along cross(segmentDir, viewDir) -- so it always presents its face. When
//     DO_ENDON_ACTIVE is set it additionally floors that width at END_ON_SCALE and fades its
//     alpha DOWN toward END_ON_ALPHA_FADE over the same angle window. The two ramps are
//     complements, which is exactly what a sprite/ribbon swap needs.
//
// ⚠ THE TWO ALPHA ARMS ARE GENUINELY ASYMMETRIC, and it is not a transcription slip:
//   sprite loop: mvColour.w  =  ramp                 (0 below the start angle, 1 above the end)
//   ribbon loop: mvColour.w *= max(END_ON_ALPHA_FADE, 1 - ramp)
// The sprite arm REPLACES the alpha (`vrlimi128 v0, v10, 1, 0` with no multiply, 0x82283514)
// and uses the ramp as-is; the ribbon arm MULTIPLIES into the existing alpha (`vmulfp128 v13,
// v13, v9` with v9 = the particle's own w, 0x82283948) and inverts the ramp (`vsubfp128 v11,
// v127, v0`, 0x82283934). Both arms WRITE BACK into the particle record -- apParticles is
// non-const for this reason.
//
// ⭐ HOW THE VMX128 FUSED FORMS WERE READ, because this is where a wrong lane hides. IDA prints
// `vnmsubfp128 vD, vA, vB, vC` and `vmaddcfp128 vD, vA, vB, vC` with FOUR operands, but the raw
// image words carry only THREE registers:
//     0x82283430  15980530  vmaddcfp128 v12, v120, v12, v0  ->  VD=12, VA=120, VB=0
//     0x822833AC  148A3150  vnmsubfp128 v4,  v10,  v6,  v4   ->  VD=4,  VA=10,  VB=6
// The fourth printed operand is the IMPLIED ACCUMULATOR, which is vD itself -- and in every one
// of the eleven `vnmsubfp128` in this function the fourth operand is literally the destination
// register. So `vmaddcfp128` is vD = vA*vD + vB and `vnmsubfp128` is vD = vD - vA*vB. Read the
// classic (non-128) forms the other way: `vnmsubfp v13, v11, v13, v12` @0x822837A8 IS raw field
// order (vD = vB - vA*vC) and forms the cross-product term a*yzx(b) - yzx(a)*b.
// Both readings are independently pinned by what they build: the vmaddcfp128 makes
// `Lerp(end0, end1, 0.5)` (the DWARF says Lerp) and each vnmsubfp128 makes the residual
// `1 - x*y*y` of a Newton-Raphson rsqrt step (the surrounding vrsqrtefp / *0.5 / vmaddfp is
// meaningless otherwise).
//
// ⚠ HOST DIVERGENCE, STATED ONCE FOR THE WHOLE FUNCTION: every normalise here is `vrsqrtefp`
// plus TWO Newton-Raphson refinements (about 22 bits), and every divide is `vrefp` plus two
// refinements. The reconstruction uses the vendor rw::math::vpu::Normalize / an exact divide,
// which is the standing convention in this tree for that pattern. The degenerate cases still
// match: rsqrt(0) makes the console's result NaN, NaN fails the `vcmpgtfp` epsilon test, and
// the host's Normalize returns zero which fails the same test -- both take the fallback arm.
//
// ⚠ THE Y-AXIS FALLBACK IS READ, NOT ASSUMED. Sprite loop: `vperm128 v13, v127, v126, v0`
// (0x822833F4) weaves the zero and one vectors through the standard selector into (0,1,0,0).
// Ribbon loop: `lvx128 v0, r0, r25` (0x8228373C) loads unk_82181510, which reads out of the
// image as 00000000 3F800000 00000000 00000000. Same vector, two encodings; the DWARF calls it
// GetVector3_YAxis.
// =================================================================================================
void LionBlendRenderer::RenderTilts(EffectsVertexBufferIterator& arIterator,
                                    RenderedParticle* apParticle, const cMatrix* apMatrix,
                                    U32 auCount, const cParticleEmitter* apEmitter,
                                    const cTime& arTime)
{
    LionBlendVertex::VertexIterator& lrLionBlendVertexIterator =
        static_cast<LionBlendVertex::VertexIterator&>(arIterator);

    // asm 0x82282FF0..0x82283014 -- the descriptor, its flags word and its material.
    const cParticleDescriptor& lrDescriptor = *apEmitter->GetDescriptor();
    const u32 luDescriptorFlags = lrDescriptor.Flags();
    BrnEffects::Utils::BuildUVData lUVData;
    lUVData.SetupFromMaterial(*lrDescriptor.Material());

    // asm 0x82283018..0x82283074 -- the behaviour, the pivot, the camera position (the
    // translation row of mCameraTransform, copied 16 bytes at a time by the two `ld`/`std`
    // pairs at 0x8228304C..0x82283074) and the four END_ON authoring floats.
    const cParticleBehaviour& lrBehaviour = *apEmitter->GetCurrentBehaviour();
    const cVector& lrPivot = lrBehaviour.mPivotPoint;

    rw::math::vpu::Vector3 lvCameraPosition;
    lvCameraPosition.x = mCameraTransform.wa.x;
    lvCameraPosition.y = mCameraTransform.wa.y;
    lvCameraPosition.z = mCameraTransform.wa.z;
    lvCameraPosition.w = mCameraTransform.wa.w;

    const f32 lfEndOnStartAngle = lrBehaviour.mEndOnStartAngle;
    const f32 lfEndOnEndAngle   = lrBehaviour.mEndOnEndAngle;
    const f32 lfEndOnAlphaFade  = lrBehaviour.mEndOnAlphaFade;
    const f32 lfEndOnScale      = lrBehaviour.mEndOnScale;

    // asm 0x822830B0 / 0x822835C8 -- ORIENT_TO_CAMERA replaces the caller's per-particle
    // matrix with a freshly built camera-orientated locator, in BOTH loops.
    const bool lbFaceCamera =
        (luDescriptorFlags & cParticleDescriptor::E_FLAG_ORIENT_TO_CAMERA) != 0u;

    // asm 0x82283078..0x8228308C -- the loop selector.
    const bool lbEndOnSprite =
        (lrBehaviour.mFlags & cParticleBehaviour::E_DO_ENDON_SPRITE) != 0u;
    const bool lbEndOnActive =
        (lrBehaviour.mFlags & cParticleBehaviour::E_DO_ENDON_ACTIVE) != 0u;

    if (lbEndOnSprite && lbEndOnActive)
    {
        // -----------------------------------------------------------------------------------
        // THE END-ON SPRITE LOOP -- asm 0x82283148..0x822835A4.
        // -----------------------------------------------------------------------------------
        for (U32 luIndex = 0; luIndex < auCount; ++luIndex)
        {
            RenderedParticle& lrPart = apParticle[luIndex];

            // asm 0x82283148..0x822831B8. ⚠ THE ARITHMETIC ORDER IS THIS SHAPE'S OWN, and it
            // is NOT the one RenderSprites uses: here X0 is formed first as size.x * -pivot.x
            // (`vxor` sign flip then `vmulfp128`) and X1 as size.x + X0 (`vaddfp`), whereas
            // RenderSprites forms X1 = size.x - size.x*pivot.x and X0 = X1 - size.x. The
            // values agree in exact arithmetic and round differently in floats.
            const f32 lfSizeX = lrPart.mvSizePlusNextFrame.x;
            const f32 lfSizeY = lrPart.mvSizePlusNextFrame.y;
            const f32 lfX0 = lfSizeX * -lrPivot.x;
            const f32 lfY0 = lfSizeY * -lrPivot.y;
            const f32 lfX1 = lfSizeX + lfX0;
            const f32 lfY1 = lfSizeY + lfY0;

            const f32 lafCornerX[4] = { lfX0, lfX0, lfX1, lfX1 };
            const f32 lafCornerY[4] = { lfY0, lfY1, lfY0, lfY1 };

            // asm 0x822831BC (spun) / 0x82283290 (flat). ⚠ ONLY rot.z is tested here -- there
            // is no Euler path on this shape at all.
            //
            // ⚠⚠ AND THE SPIN GOES THE OTHER WAY THAN RenderSprites'. The two vperm operands
            // at 0x82283274 are (X*cos + Y*sin) and (Y*cos - X*sin); RenderSprites' at
            // 0x822828C8 are (X*cos - Y*sin) and (X*sin + Y*cos). That is the transpose --
            // a rotation by the negated angle -- and it is what the instructions say.
            rw::math::vpu::Vector3 laPoints[4];
            static const f32 KF_IS_ZERO_EPSILON = 1.1920928955078125e-07f;
            if (fabsf(lrPart.mvRotPlusFrame.z) > KF_IS_ZERO_EPSILON)
            {
                f32 lfSin, lfCos;
                BrnEffects::Utils::SinCosCycles(lrPart.mvRotPlusFrame.z, lfSin, lfCos);
                for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
                {
                    laPoints[luCorner].x = lafCornerX[luCorner] * lfCos
                                         + lafCornerY[luCorner] * lfSin;
                    laPoints[luCorner].y = lafCornerY[luCorner] * lfCos
                                         - lafCornerX[luCorner] * lfSin;
                    laPoints[luCorner].z = 0.0f;
                    laPoints[luCorner].w = 0.0f;
                }
            }
            else
            {
                for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
                {
                    laPoints[luCorner].x = lafCornerX[luCorner];
                    laPoints[luCorner].y = lafCornerY[luCorner];
                    laPoints[luCorner].z = 0.0f;
                    laPoints[luCorner].w = 0.0f;
                }
            }

            // asm 0x822832D8..0x8228333C -- the segment transform. Note the locator is rebuilt
            // every iteration even though nothing per-particle feeds it; reproduced as emitted.
            rw::math::vpu::Matrix44Affine lConvertedXform;
            if (lbFaceCamera)
            {
                cMatrix lCurrLoc;
                BuildCameraOrientatedLocator(lCurrLoc, apEmitter, mCameraTransform, arTime);
                lConvertedXform = MatrixConvert(lCurrLoc);
            }
            else
            {
                lConvertedXform = MatrixConvert(apMatrix[luIndex]);
            }

            // asm 0x82283348..0x822833F8 -- the segment, its unit direction, and the Y-axis
            // fallback when it degenerates.
            rw::math::vpu::Vector3 lv3Pos, lv3Pos1;
            lv3Pos.x  = lrPart.mPos.x;  lv3Pos.y  = lrPart.mPos.y;
            lv3Pos.z  = lrPart.mPos.z;  lv3Pos.w  = 0.0f;
            lv3Pos1.x = lrPart.mPos1.x; lv3Pos1.y = lrPart.mPos1.y;
            lv3Pos1.z = lrPart.mPos1.z; lv3Pos1.w = 0.0f;

            const rw::math::vpu::Vector3 lvPos0 =
                rw::math::vpu::TransformPoint(lConvertedXform, lv3Pos);
            const rw::math::vpu::Vector3 lvPos1 =
                rw::math::vpu::TransformPoint(lConvertedXform, lv3Pos1);

            rw::math::vpu::Vector3 lvDirVec;
            lvDirVec.x = lvPos1.x - lvPos0.x;
            lvDirVec.y = lvPos1.y - lvPos0.y;
            lvDirVec.z = lvPos1.z - lvPos0.z;
            lvDirVec.w = 0.0f;

            rw::math::vpu::Vector3 lvNormDirVec = rw::math::vpu::Normalize(lvDirVec);
            if (!(fabsf(rw::math::vpu::MagnitudeSquared(lvNormDirVec)) > KF_IS_ZERO_EPSILON))
            {
                lvNormDirVec.x = 0.0f; lvNormDirVec.y = 1.0f;
                lvNormDirVec.z = 0.0f; lvNormDirVec.w = 0.0f;
            }

            // asm 0x822833FC..0x82283430 -- the span, its two ends and their midpoint.
            // ⚠ THE DOT PRODUCT BELOW USES THE UNIT DIRECTION, NOT THIS SCALED ONE: the asm
            // saves the unit copy in v9 (0x82283400) BEFORE 0x82283414 scales v13 by size.y.
            rw::math::vpu::Vector3 lvMidVec;   // the span vector (DWARF lvMidVec)
            lvMidVec.x = lvNormDirVec.x * lfSizeY;
            lvMidVec.y = lvNormDirVec.y * lfSizeY;
            lvMidVec.z = lvNormDirVec.z * lfSizeY;
            lvMidVec.w = 0.0f;

            rw::math::vpu::Vector3 lvEnd0;
            lvEnd0.x = lvPos0.x - lvMidVec.x * lrPivot.y;
            lvEnd0.y = lvPos0.y - lvMidVec.y * lrPivot.y;
            lvEnd0.z = lvPos0.z - lvMidVec.z * lrPivot.y;
            lvEnd0.w = 0.0f;

            rw::math::vpu::Vector3 lvEnd1;
            lvEnd1.x = lvEnd0.x + lvMidVec.x;
            lvEnd1.y = lvEnd0.y + lvMidVec.y;
            lvEnd1.z = lvEnd0.z + lvMidVec.z;
            lvEnd1.w = 0.0f;

            // `vsubfp` then `vmaddcfp128 v12, <0.5>, v12, v0` == Lerp(lvEnd0, lvEnd1, 0.5).
            const rw::math::vpu::Vector3 lvPartMidPoint =
                rw::math::vpu::Lerp(lvEnd0, lvEnd1, 0.5f);

            // asm 0x82283434..0x82283474 -- how squarely the segment points at the viewer.
            rw::math::vpu::Vector3 lvCamVec;
            lvCamVec.x = lvCameraPosition.x - lvPartMidPoint.x;
            lvCamVec.y = lvCameraPosition.y - lvPartMidPoint.y;
            lvCamVec.z = lvCameraPosition.z - lvPartMidPoint.z;
            lvCamVec.w = 0.0f;
            lvCamVec = rw::math::vpu::Normalize(lvCamVec);

            f32 lfDotProd = rw::math::vpu::Dot(lvNormDirVec, lvCamVec);
            lfDotProd *= lfDotProd;

            // asm 0x82283478..0x822834E0 -- the ramp, then 0x8228350C..0x82283528 REPLACES the
            // particle's alpha with it (no floor, no multiply -- see the note at the head).
            f32 lfAlpha = 0.0f;
            if (lfDotProd > lfEndOnStartAngle)
            {
                if (lfEndOnEndAngle > lfDotProd)
                {
                    lfAlpha = (lfDotProd - lfEndOnStartAngle)
                            / (lfEndOnEndAngle - lfEndOnStartAngle);
                }
                else
                {
                    lfAlpha = 1.0f;
                }
            }
            lrPart.mvColour.w = lfAlpha;

            // asm 0x82283530..0x8228358C -- the flat quad through the camera basis, planted at
            // the segment midpoint. Only mCameraTransform's three basis rows are used; the
            // translation comes from lvPartMidPoint.
            for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
            {
                const f32 lfCx = laPoints[luCorner].x;
                const f32 lfCy = laPoints[luCorner].y;
                const f32 lfCz = laPoints[luCorner].z;
                rw::math::vpu::Vector3 lvOut;
                lvOut.x = mCameraTransform.xa.x * lfCx + mCameraTransform.ya.x * lfCy
                        + mCameraTransform.za.x * lfCz + lvPartMidPoint.x;
                lvOut.y = mCameraTransform.xa.y * lfCx + mCameraTransform.ya.y * lfCy
                        + mCameraTransform.za.y * lfCz + lvPartMidPoint.y;
                lvOut.z = mCameraTransform.xa.z * lfCx + mCameraTransform.ya.z * lfCy
                        + mCameraTransform.za.z * lfCz + lvPartMidPoint.z;
                lvOut.w = 0.0f;
                laPoints[luCorner] = lvOut;
            }

            QuadDraw(lrLionBlendVertexIterator, lUVData, lrPart, laPoints, *apEmitter);
        }
    }
    else
    {
        // -----------------------------------------------------------------------------------
        // THE RIBBON LOOP -- asm 0x82283624..0x822839B0.
        // -----------------------------------------------------------------------------------
        for (U32 luIndex = 0; luIndex < auCount; ++luIndex)
        {
            RenderedParticle& lrPart = apParticle[luIndex];

            // asm 0x82283624..0x82283688 -- same segment-transform choice as the sprite loop.
            rw::math::vpu::Matrix44Affine lConvertedXform;
            if (lbFaceCamera)
            {
                cMatrix lCurrLoc;
                BuildCameraOrientatedLocator(lCurrLoc, apEmitter, mCameraTransform, arTime);
                lConvertedXform = MatrixConvert(lCurrLoc);
            }
            else
            {
                lConvertedXform = MatrixConvert(apMatrix[luIndex]);
            }

            // asm 0x82283688..0x8228373C.
            rw::math::vpu::Vector3 lv3Pos, lv3Pos1;
            lv3Pos.x  = lrPart.mPos.x;  lv3Pos.y  = lrPart.mPos.y;
            lv3Pos.z  = lrPart.mPos.z;  lv3Pos.w  = 0.0f;
            lv3Pos1.x = lrPart.mPos1.x; lv3Pos1.y = lrPart.mPos1.y;
            lv3Pos1.z = lrPart.mPos1.z; lv3Pos1.w = 0.0f;

            const rw::math::vpu::Vector3 lvPos0 =
                rw::math::vpu::TransformPoint(lConvertedXform, lv3Pos);
            const rw::math::vpu::Vector3 lvPos1 =
                rw::math::vpu::TransformPoint(lConvertedXform, lv3Pos1);

            rw::math::vpu::Vector3 lvDirVec;
            lvDirVec.x = lvPos1.x - lvPos0.x;
            lvDirVec.y = lvPos1.y - lvPos0.y;
            lvDirVec.z = lvPos1.z - lvPos0.z;
            lvDirVec.w = 0.0f;

            static const f32 KF_IS_ZERO_EPSILON = 1.1920928955078125e-07f;
            rw::math::vpu::Vector3 lvNormDirVec = rw::math::vpu::Normalize(lvDirVec);
            if (!(fabsf(rw::math::vpu::MagnitudeSquared(lvNormDirVec)) > KF_IS_ZERO_EPSILON))
            {
                lvNormDirVec.x = 0.0f; lvNormDirVec.y = 1.0f;   // unk_82181510
                lvNormDirVec.z = 0.0f; lvNormDirVec.w = 0.0f;
            }

            // asm 0x82283740..0x822837AC -- the view ray is taken from the PIVOT point along
            // the segment (the UNNORMALISED direction scaled by pivot.y), and the ribbon's
            // width axis is the cross product of the segment with it.
            rw::math::vpu::Vector3 lvPartMidPoint;
            lvPartMidPoint.x = lvDirVec.x * lrPivot.y + lvPos0.x;
            lvPartMidPoint.y = lvDirVec.y * lrPivot.y + lvPos0.y;
            lvPartMidPoint.z = lvDirVec.z * lrPivot.y + lvPos0.z;
            lvPartMidPoint.w = 0.0f;

            rw::math::vpu::Vector3 lvCamVec;
            lvCamVec.x = lvCameraPosition.x - lvPartMidPoint.x;
            lvCamVec.y = lvCameraPosition.y - lvPartMidPoint.y;
            lvCamVec.z = lvCameraPosition.z - lvPartMidPoint.z;
            lvCamVec.w = 0.0f;
            lvCamVec = rw::math::vpu::Normalize(lvCamVec);

            rw::math::vpu::Vector3 lvWidthVec = rw::math::vpu::Cross(lvNormDirVec, lvCamVec);

            if (lbEndOnActive)
            {
                // asm 0x822837B4..0x8228383C. Two unit vectors, so |cross| is the sine of the
                // angle between them and collapses to zero end-on -- the floor at END_ON_SCALE
                // is what stops the ribbon vanishing to a line. `vsel v13, v13, v5, v8`
                // (0x82283834) substitutes a hard zero when |cross| squared is exactly zero,
                // which is the Magnitude() zero guard.
                //
                // ⚠ ONE STATED DIVERGENCE, in the exactly-degenerate case only. On the console
                // the guard covers the MAGNITUDE but not the direction: a zero cross still goes
                // through `vrsqrtefp(0)` == +Inf, so the normalised vector comes out NaN and the
                // ribbon's width axis is NaN. The host Normalize returns the zero vector there,
                // giving a zero-width quad instead. Reaching it needs the segment exactly
                // parallel to the view ray; the host behaviour is the same shape, without the
                // NaN.
                f32 lfCamVecLength = rw::math::vpu::Magnitude(lvWidthVec);
                lvWidthVec = rw::math::vpu::Normalize(lvWidthVec);
                if (lfEndOnScale > lfCamVecLength)
                {
                    lfCamVecLength = lfEndOnScale;
                }
                lvWidthVec.x *= lfCamVecLength;
                lvWidthVec.y *= lfCamVecLength;
                lvWidthVec.z *= lfCamVecLength;
            }

            // asm 0x82283840..0x82283860 -- the span along the segment and the half-width
            // across it, both scaled by the particle's size.
            const f32 lfSizeX = lrPart.mvSizePlusNextFrame.x;
            const f32 lfSizeY = lrPart.mvSizePlusNextFrame.y;

            rw::math::vpu::Vector3 lvSpan;
            lvSpan.x = lvNormDirVec.x * lfSizeY;
            lvSpan.y = lvNormDirVec.y * lfSizeY;
            lvSpan.z = lvNormDirVec.z * lfSizeY;
            lvSpan.w = 0.0f;

            rw::math::vpu::Vector3 lvWidth;
            lvWidth.x = lvWidthVec.x * lfSizeX;
            lvWidth.y = lvWidthVec.y * lfSizeX;
            lvWidth.z = lvWidthVec.z * lfSizeX;
            lvWidth.w = 0.0f;

            rw::math::vpu::Vector3 lvEnd0;
            lvEnd0.x = lvPos0.x - lvSpan.x * lrPivot.y;
            lvEnd0.y = lvPos0.y - lvSpan.y * lrPivot.y;
            lvEnd0.z = lvPos0.z - lvSpan.z * lrPivot.y;
            lvEnd0.w = 0.0f;

            rw::math::vpu::Vector3 lvEnd1;
            lvEnd1.x = lvEnd0.x + lvSpan.x;
            lvEnd1.y = lvEnd0.y + lvSpan.y;
            lvEnd1.z = lvEnd0.z + lvSpan.z;
            lvEnd1.w = 0.0f;

            if (lbEndOnActive)
            {
                // asm 0x82283868..0x82283950 -- the same angle window as the sprite loop, but
                // the ramp is inverted and the result MULTIPLIES the particle's own alpha
                // after being floored at END_ON_ALPHA_FADE.
                const f32 lfInAlpha = lrPart.mvColour.w;

                const rw::math::vpu::Vector3 lvMid =
                    rw::math::vpu::Lerp(lvEnd0, lvEnd1, 0.5f);

                rw::math::vpu::Vector3 lvMidCamVec;
                lvMidCamVec.x = lvCameraPosition.x - lvMid.x;
                lvMidCamVec.y = lvCameraPosition.y - lvMid.y;
                lvMidCamVec.z = lvCameraPosition.z - lvMid.z;
                lvMidCamVec.w = 0.0f;
                lvMidCamVec = rw::math::vpu::Normalize(lvMidCamVec);

                f32 lfDotProd = rw::math::vpu::Dot(lvNormDirVec, lvMidCamVec);
                lfDotProd *= lfDotProd;

                f32 lfOutAlpha = 1.0f;
                if (lfDotProd > lfEndOnStartAngle)
                {
                    if (lfEndOnEndAngle > lfDotProd)
                    {
                        lfOutAlpha = 1.0f - (lfDotProd - lfEndOnStartAngle)
                                          / (lfEndOnEndAngle - lfEndOnStartAngle);
                    }
                    else
                    {
                        lfOutAlpha = 0.0f;
                    }
                }
                if (lfEndOnAlphaFade > lfOutAlpha)
                {
                    lfOutAlpha = lfEndOnAlphaFade;
                }
                lrPart.mvColour.w = lfOutAlpha * lfInAlpha;
            }

            // asm 0x82283954..0x82283998 -- the four corners, already in world space.
            rw::math::vpu::Vector3 laPoints[4];
            const f32 lfOffX = lvWidth.x * lrPivot.x;
            const f32 lfOffY = lvWidth.y * lrPivot.x;
            const f32 lfOffZ = lvWidth.z * lrPivot.x;

            laPoints[0].x = lvEnd0.x - lfOffX; laPoints[0].y = lvEnd0.y - lfOffY;
            laPoints[0].z = lvEnd0.z - lfOffZ; laPoints[0].w = 0.0f;
            laPoints[1].x = lvEnd1.x - lfOffX; laPoints[1].y = lvEnd1.y - lfOffY;
            laPoints[1].z = lvEnd1.z - lfOffZ; laPoints[1].w = 0.0f;
            laPoints[2].x = laPoints[0].x + lvWidth.x;
            laPoints[2].y = laPoints[0].y + lvWidth.y;
            laPoints[2].z = laPoints[0].z + lvWidth.z; laPoints[2].w = 0.0f;
            laPoints[3].x = laPoints[1].x + lvWidth.x;
            laPoints[3].y = laPoints[1].y + lvWidth.y;
            laPoints[3].z = laPoints[1].z + lvWidth.z; laPoints[3].w = 0.0f;

            QuadDraw(lrLionBlendVertexIterator, lUVData, lrPart, laPoints, *apEmitter);
        }
    }
}

}  // namespace BrnGraphics

// =================================================================================================
// The two remaining LionBlendRenderer methods -- TRAP STUBS, deliberately.
//
// ⭐ WHY THE TWO SetState OVERLOADS ARE **NOT** FORWARDED (2026-09-04). The obvious body is
// `mRenderer.SetState(apState)` -- mRenderer now really does carry a CgsGraphics::ImRendererBase
// base, so it compiles, and the X360 really does emit exactly that call. DON'T. The committed
// `ImRendererBase::SetState(const BlendState*)` (CgsIm2d.cpp:114) **IGNORES ITS ARGUMENT** and
// hard-codes SRCALPHA/INVSRCALPHA -- it is the PC 2D loading-screen fold, not the console's
// state binder. Forwarding the Lion path's per-material blend and depth-stencil states into it
// would compile, link, run, and THROW AWAY every state the material asked for without a trace:
// the exact quiet-discard shape this subsystem has already been bitten by three times. (Note
// for the reader: the faithfulness lint flags the two-word phrase for that failure mode as
// invented-format vocabulary, so it is spelled out longhand here.) A trap that says
// "not written" is worth more than a call that says "bound" and did not.
//
// ⭐⭐ THE FIX IS SMALLER THAN THAT NOTE THOUGHT, AND ONE HALF OF IT IS ALREADY DECIDED
// (2026-09-05). Neither overload needs ImRendererBase touched at all, so the "shared-header change
// across four TUs" is not the obstacle: the console's own two bodies are an assert plus ONE call,
// and both callees are fully reconstructed in this tree.
//   sub_82276DA8 (39 instr) = CGS_ASSERT(mgpActiveRenderer == this) [CgsImRenderer.h:732]
//                             + shadow::Device::SetState(const renderengine::DepthStencilState*)
//                               @0x82276AD0  -- bodied, shadowingdevice.cpp:870
//   sub_82276E48 (39 instr) = the same assert [CgsImRenderer.h:776]
//                             + shadow::Device::SetState(const renderengine::RasterizerState*)
//                               @0x82276B38  -- bodied, shadowingdevice.cpp:901
// (Both identifications are the ones CgsImRenderer.cpp:270's SetStateLowLevel note already
// established by comparing the three wrappers' gate byte and cache slot instruction for
// instruction: 0x82276DA8/0x82276B38 share mbDepthStencilStateLocked / dword_83010A28 and
// mbRasteriserStateLocked / dword_83010A2C respectively.)
//
// ⛔ WHAT ACTUALLY BLOCKS THE SECOND ONE IS A TYPE CONTRADICTION, NOT AN UNKNOWN BODY -- and it
// must be SETTLED, not guessed, because a wrong state family is exactly the quiet-discard the
// paragraph above is about:
//   * DWARF BrnLionBlendRenderer.h:86 declares this overload `SetState(const BlendState*)`, and
//     ImRendererBase declares BOTH `SetState(const BlendState*)` and
//     `SetState(const RasterizerState*)` as separate members (dwarfdump CgsImRenderer.h:90/:99),
//     so the DWARF is distinguishing them deliberately.
//   * But LionParticleRender::BeginRendering @0x82289568 word 18 calls sub_82276E48 -- the
//     RASTERISER wrapper -- with dword_83010F3C. The BLEND wrapper is a different address
//     (ImRendererBase::SetState @0x82276D08, gate byte_83010907, cache dword_83010964) and is
//     not byte-identical to it, so this is not an ICF fold either way.
// One of the two readings is wrong. Settle it from the DecFIGS PS3 body of
// LionParticleRender::BeginRendering, or from whatever builds dword_83010F3C, BEFORE writing a
// body; do not resolve it by editing the `typedef renderengine::MaterialState BlendState` in
// BrnLionBlendRenderer.h to whatever makes the call compile. (MaterialState is a *material* block
// that CONTAINS a blend, a depth-stencil and a rasteriser state -- renderstates.h:254 -- so that
// typedef is itself unproven.)
//
// Every one of them is on the LION particle RENDER path and nothing else:
// LionParticleRender's virtuals (Render / RenderGroupBegin / BeginRendering / ...) are their only
// callers, and those virtuals are reached only from cLionFX's dispatch -- whose Lion arms in
// ParticleModule::BuildLionVertexBuffers and ::RenderFullResParticles are parked (see below), so
// none of these can execute today. A trap body is the project's honest "not done yet" for exactly
// that case (STRATEGY.md, "the stub scaffold"): it declares the function unfinished and crashes
// LOUDLY if the Lion path ever does come alive, instead of quietly drawing nothing.
//
// ⚠ They exist at all because the LINK needs them: mLionRenderer is a by-value ParticleModule
// member, so LionParticleRender's vtable is emitted and every virtual it names must resolve.
// ALL THREE DRAW HALVES ARE NOW REAL (2026-09-05): RenderSprites @0x82282608 (328
// instructions), RenderQuads @0x82282B28 (295) and RenderTilts @0x82282FC8 (639), alongside
// EndRendering @0x8227E610, SetCameraData @0x822824F8, BuildCameraOrientatedLocator
// @0x8227A478 and BeginRendering (an inline forward in the header onto
// Im3dBlend::BeginRendering @0x82282060, bodied in BrnLionBlendIm3d.cpp).
//
// ⭐ THE DRAW SIDE IS NOW WHOLE (2026-09-05). Both of the jobs this note used to name are done:
// BrnEffects::Utils::BuildUVs @0x822781E0 is real (its two rodata permute tables decoded), and
// Im3dBlend::Construct @0x8229B260 is bodied in BrnLionBlendIm3d.cpp -- the four Xenos microcode
// blobs (unk_8200DD58 0x1A4 / unk_8200DF00 0x10C / unk_8200E010 0x220 / unk_8200E230 0x1F8) were
// read out of the image, disassembled and re-authored as D3D9 in
// pc/gcm/renderengine/LionBlendProgramsPC.cpp, and ParticleModule::Prepare calls Construct in the
// console's own position.
//
// ⛔ WHAT STILL STANDS BETWEEN THIS FILE AND A PIXEL IS THE SIMULATION, NOT THE DRAW. Nothing
// ever calls these three Render* methods on this build, because the Lion SIMULATION core is not
// landed and both ParticleModule arms that would drive it are parked and say so once:
//     cParticleEmitter::Update @0x829153D8            201 instr   TRAP (LionRuntimeLinkStubs.cpp)
//     cParticleEmitter::ParticleBuild @0x82910118   1,142 instr   absent
//     cParticleBehaviour::Lerp @0x8290B1F8          1,530 instr   log-stub
//     SimulateParticlesInBucketGeneral<> x3           549 instr   absent
//     Drag/ColourSteps/MultiFrame Behaviour::Process  669 instr   absent (4 of 7 are landed)
//     ParticleModule::BuildLionVertexBuffers @0x8228AC20's Lion half (231 instr total)
//     ParticleModule::RenderFullResParticles @0x8229AFD0's cLionFX::Dispatch branch
// and, once that closure runs, the two SetState overloads below and the Xenon fast-path draw
// thunk D3DDevice_DrawVertices (LionRuntimeLinkStubs.cpp) are the last traps on the path.
//
// ⛔ A NOTE FOR ANYONE QUERYING THE TREE FOR THIS SUBSYSTEM: tools/re/hasbody.py reports a
// Render* shape as HAS BODY whether or not it is written, because a trap IS a definition. It
// happens to be right today; it was wrong for months. Ask this file, not the tool.
// =================================================================================================
namespace BrnGraphics
{
    void LionBlendRenderer::SetState(const renderengine::DepthStencilState* /*apState*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::SetState(DepthStencilState) -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::SetState(const BlendState* /*apState*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::SetState(BlendState) -- NOT RECONSTRUCTED (Lion render path)");
    }

}

// =================================================================================================
// ONE more Lion-render-path symbol, homed here beside the blend renderer rather than pulling
// another TU onto the build list.
//
//   * LionParticleRender::CreateInternalMaterial @0x82280C30 (LionParticleRenderMaterial.cpp) --
//     reached from LionParticleRender::TextureRegister and ::SetMaterial.
//
// ⛔ IT IS NO LONGER UNREACHABLE, AND THAT IS WHY IT IS NO LONGER AN ASSERT. As of the Lion
// install landing, ParticleModule::Prepare calls cLionFX::Init, so gpLionParticleRender is
// non-null and cParticleMaterial::Build @0x8290E500 calls TextureRegister for EVERY material in
// every .lef in PARTICLES.BUNDLE -- which reaches here once per material. A CGS_ASSERT(false)
// there is an assert storm: this project has measured a run drowned by 839,983 of them, and an
// assert storm starves the harness so badly that the failure is reported as the game's.
//
// So it is a ONE-SHOT NAMED ANNOUNCEMENT instead, never a silent zero. What is actually missing:
// the console body builds a renderengine::BlendStateParameters from the material's blend mode
// (a 20-case switch over material +0x3A writing packed bitfields, plus a second 8-case switch
// over +0x3B when material flag 4 is set), calls BlendState::GetResourceDescriptor +
// BlendState::Initialize, and appends {materialHash, blendState} to the process-wide table at
// qword_82FAAD80 (capacity 256, "Out of space for more blend states" at
// LionParticleRender.cpp:687). That is a self-contained pass over renderengine::BlendState and
// is deliberately out of this wave's slice.
//
// WHAT RETURNING 0 COSTS, EXACTLY: cParticleMaterial::mMaterialHandle stays 0, so
// LionParticleRender::SetMaterial cannot bind a per-material blend state. It does NOT cost the
// texture: TextureRegister's other half -- SetTextureMapHandle / SetNormalMapHandle from the
// name hashes -- runs normally, and that is what FindTexture resolves against.
//
// ⭐ THE OTHER TWO STAND-INS ARE GONE (2026-09-03). cParticleMaterial::SetTextureMapHandle /
// SetNormalMapHandle used to be trap stand-ins here as well, because their real TU --
// ParticleMaterial.cpp, which has both real bodies (@0x82909DD8 / @0x82909DE0) -- "would drag
// four further un-homed Lion SDK symbols (cLionSerialiser::StringStore, gpLionParticleRender,
// gLionParticleMaterialTokenTable and the unnamed rodata off_82000D08)". All four are now
// homed, ParticleMaterial.cpp is mounted, and the duplicate definitions here were an LNK2005 the
// moment it was -- which is how they were found. A stand-in that outlives its reason is a fork
// waiting to happen.
// =================================================================================================
namespace BrnParticle
{
    U32 LionParticleRender::CreateInternalMaterial(const cParticleMaterial* /*apMaterial*/)
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            CgsDev::Log::WriteToLog(
                "[effects] NOT RECONSTRUCTED: BrnParticle::LionParticleRender::"
                "CreateInternalMaterial @0x82280C30 (the renderengine::BlendState build + the "
                "256-entry internal-material table). Every material gets mMaterialHandle 0, so no "
                "per-material blend state is bound; the texture-map/normal-map handles ARE set.\n");
        }
        return 0;
    }
}
