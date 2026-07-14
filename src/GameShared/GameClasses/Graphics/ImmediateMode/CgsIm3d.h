#pragma once

#include "BrnCommonTypes.h"  // Matrix44 (rw::math::vpu::Matrix44)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"            // CgsGraphics::ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"  // CgsGraphics::BasicColouredVertex
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"  // CgsGraphics::BasicColouredTexturedVertex (Im3d)

// CgsGraphics::Im3d* - the untextured immediate-mode 3D render hierarchy. Mirrors the 2D
// fold in CgsIm2d.h: Im3dBase<V> adds the world transform on top of ImRenderer<V>, and
// Im3dUntex specialises it for the position+colour vertex (BasicColouredVertex). Hierarchy
// from the DecFIGS DWARF (CgsIm3d.h:56/199):
//   Im3dUntex : Im3dBase<BasicColouredVertex> : ImRenderer<BasicColouredVertex> : ImRendererBase
//
// In-scope callers (BrnGui::ProgressBarRenderer::RenderQuadUntex @ 0x8245C828) only:
//   - set the current transform (SetTransform(Matrix44)), and
//   - submit a static vertex run (the inherited ImRenderer<V>::Render).
// so only those two entry points are bodied/declared; the rest of the X360 Im3dBase API
// (Construct, the program/state-handle table, the two-matrix SetTransform) is OMITTED as
// uncommitted out-of-scope state. FLAG: minimal-slice immediate-3D hierarchy.
namespace CgsGraphics
{
    template <typename V>
    struct Im3dBase : public ImRenderer<V>
    {
        // DWARF CgsIm3d.h:74 -- install the world->view->proj transform used by the next
        // Render submissions. (The X360 takes the Matrix44 by value; the RenderQuadUntex
        // call passes the identity matrix.)
        void SetTransform(Matrix44 lTransform);

        // DWARF CgsIm3d.h:85 -- the active transform the immediate batches are drawn with.
        Matrix44 mCurrentTransform;
    };

    // DWARF CgsIm3d.h:199.
    struct Im3dUntex : public Im3dBase<BasicColouredVertex>
    {
    };

    // CgsGraphics::Im3d - the concrete TEXTURED immediate-mode 3D renderer (the smoke / spark /
    // blobby-shadow / above-car / debug-3D textured paths). It IS the
    // ImRenderer<BasicColouredTexturedVertex> instantiation bodied in CgsIm3d.cpp, grown with the
    // stencil-mask state the Apt/GUI mask path drives (PushMask / PopMask / SaveMaskShaderConstants
    // / SetMaskPixelShaderState). Mirrors the 2D fold CgsGraphics::Im2d (CgsIm2d.h), which likewise
    // derives from its ImRenderer<V> and adds the mask API.
    //
    // Only PushMask (X360 @0x827DCF78) is X360-ARTIST-attested for THIS ledger key and bodied here.
    // Its three collaborators are separate (still-unhomed) ledger keys, declared here as members so
    // the class interface is complete and PushMask can reach them BY NAME; their bodies land with
    // their own TUs. FLAG: SaveMaskShaderConstants / SetMaskPixelShaderState are declaration-only
    // (no X360 body attested for this key), and their parameter TYPES are width-only (4-byte,
    // no DecFIGS DWARF for this TU) -- modelled as opaque handles, not fabricated.
    struct Im3d : public ImRenderer<BasicColouredTexturedVertex>
    {
        // V_IM3D_MAX_MASK_COUNT -- the mask-stack ceiling PushMask asserts against (X360 immediate
        // `cmplwi 2`). CgsIm3d.h:374 in the X360 source.
        static const u32 KU_MAX_MASK_COUNT = 2;

        // PushMask @0x827DCF78. Open one stencil-mask region: on the FIRST mask of the stack, bind
        // the next program slot (mi8CurrentProgram + 1) and install the current world transform;
        // then save this mask's shader constants, bump the live mask count, and push the mask pixel-
        // shader state. Returns the renderengine shader-state result (X360 r3 tail-passthrough).
        void* PushMask(const void* lpMaskParam0, const void* lpMaskParam1, const void* lpMaskParam2);

        // ---- mask collaborators: own ledger keys, declaration-only here (see FLAG above) ----------
        // X360 call site stores this mask's constants; PushMask forwards (lpMaskParam2, lpMaskParam0,
        // lpMaskParam1) -- i.e. the X360 arg order SaveMaskShaderConstants(this, a4, a2, a3).
        void  SaveMaskShaderConstants(const void* lpParam0, const void* lpParam1, const void* lpParam2);
        // Bind the mask pixel-shader state for the current mask count. Returns the X360 r3 result.
        void* SetMaskPixelShaderState();

        // The live number of pushed masks (X360 mu32NumMasks @ this+0x180). FLAG: the preceding
        // per-mask shader-constant storage members (written by SaveMaskShaderConstants) are unhomed
        // -- they arrive with that collaborator's TU; PushMask only ever reaches mu32NumMasks, so it
        // is the only Im3d-specific member modelled here (absolute X360 +0x180 offset is not
        // LLP64-invariant and is deliberately not pinned with padding).
        u32 mu32NumMasks;
    };

    // The untextured 3D render buffer the renderers feed. On the X360/PS3 this is a distinct
    // double-buffered vertex buffer; on the PC target it folds onto the one Im3dUntex renderer
    // (exactly as Im2dRenderBuffer folds onto Im2d in CgsImRenderBuffer.h). FLAG: a placeholder
    // empty Im3dRenderBufferUntex still exists in GameSource/Graphics/BrnRendererModule.h (a
    // forward-only opaque-storage stub for that off-path renderer module); this is the real
    // typed home -- the two are never included in the same TU.
    typedef Im3dUntex Im3dRenderBufferUntex;
}
