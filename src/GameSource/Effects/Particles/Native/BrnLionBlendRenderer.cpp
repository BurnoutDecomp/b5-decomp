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
// The five remaining LionBlendRenderer methods -- TRAP STUBS, deliberately.
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
// "not written" is worth more than a call that says "bound" and did not. The real fix is one
// level up (CgsGraphics::BlendState / DepthStencilState are opaque forward-declared tags that
// should be the renderengine:: types the DWARF actually names, and the base overloads should be
// the console's shadow-cache binders) -- a shared-header change across four TUs, out of slice.
//
// Every one of them is on the LION particle RENDER path and nothing else:
// LionParticleRender's virtuals (Render / RenderGroupBegin / BeginRendering / ...) are their only
// callers, and those virtuals are reached only from cLionFX's dispatch. The Lion core is not
// landed on this build -- cLionFX::Init is announced, not called (ParticleModule_Lifecycle.cpp),
// StartLionEffect always returns KU_HANDLE_INVALID, and no LionEffect slot is ever claimed -- so
// none of these can execute. A trap body is the project's honest "not done yet" for exactly that
// case (STRATEGY.md, "the stub scaffold"): it declares the function unfinished and crashes LOUDLY
// if the Lion path ever does come alive, instead of quietly drawing nothing.
//
// ⚠ They exist at all because the LINK needs them: mLionRenderer is a by-value ParticleModule
// member, so LionParticleRender's vtable is emitted and every virtual it names must resolve.
// Real bodies now: EndRendering @0x8227E610 and SetCameraData @0x822824F8 and
// BuildCameraOrientatedLocator @0x8227A478 above, plus BeginRendering (an inline forward in the
// header onto Im3dBlend::BeginRendering @0x82282060, bodied in BrnLionBlendIm3d.cpp).
//
// ⛔ A NOTE FOR ANYONE QUERYING THE TREE FOR THIS SUBSYSTEM: tools/re/hasbody.py reports all
// three Render* shapes as HAS BODY, because a trap IS a definition. The three draw halves
// (RenderSprites 328 / RenderQuads 295 / RenderTilts 639 instructions) are still OPEN. Ask this
// file, not the tool, and corroborate any "already done" claim about them here.
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

    void LionBlendRenderer::RenderSprites(EffectsVertexBufferIterator& /*arIterator*/,
                                          RenderedParticle* /*apParticle*/, const cMatrix* /*apMatrix*/,
                                          U32 /*auCount*/, const cParticleEmitter* /*apEmitter*/,
                                          const cTime& /*arTime*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::RenderSprites -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::RenderQuads(EffectsVertexBufferIterator& /*arIterator*/,
                                        RenderedParticle* /*apParticle*/, const cMatrix* /*apMatrix*/,
                                        U32 /*auCount*/, const cParticleEmitter* /*apEmitter*/,
                                        const cTime& /*arTime*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::RenderQuads -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::RenderTilts(EffectsVertexBufferIterator& /*arIterator*/,
                                        RenderedParticle* /*apParticle*/, const cMatrix* /*apMatrix*/,
                                        U32 /*auCount*/, const cParticleEmitter* /*apEmitter*/,
                                        const cTime& /*arTime*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::RenderTilts -- NOT RECONSTRUCTED (Lion render path)");
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
