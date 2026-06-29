#pragma once

#include "BrnCommonTypes.h"  // Matrix44 (rw::math::vpu::Matrix44)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"            // CgsGraphics::ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"  // CgsGraphics::BasicColouredVertex

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

    // The untextured 3D render buffer the renderers feed. On the X360/PS3 this is a distinct
    // double-buffered vertex buffer; on the PC target it folds onto the one Im3dUntex renderer
    // (exactly as Im2dRenderBuffer folds onto Im2d in CgsImRenderBuffer.h). FLAG: a placeholder
    // empty Im3dRenderBufferUntex still exists in GameSource/Graphics/BrnRendererModule.h (a
    // forward-only opaque-storage stub for that off-path renderer module); this is the real
    // typed home -- the two are never included in the same TU.
    typedef Im3dUntex Im3dRenderBufferUntex;
}
