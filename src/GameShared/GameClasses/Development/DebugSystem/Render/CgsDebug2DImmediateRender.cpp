#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <math.h>   // sqrtf (DrawLine thickness)

// CgsDev::Debug2DImmediateRender box-path bodies - what actually draws the on-screen debug squares.
// Reconstructed against the existing immediate-mode 2D path the loading screen already renders
// through (CgsGraphics::Im2d / ImRenderer<V>): a box is a 4-vertex triangle-strip quad submitted via
// mpRenderBuffer->Render, exactly like BrnGame::LoadingScreenRenderer's EmitQuad. The X360
// (CgsDebug2DImmediateRender.cpp) batches into maIm2dVertsArray and flushes in DispatchVertices; that
// batching is preserved here, flushed per primitive (the current ImRenderer<V>::Render hardcodes a
// triangle-strip + ignores the PrimitiveType arg, so each box must be its own strip). The input
// Vector2 (rw::math::vpu::Vector2) is only read via .x/.y - it has no (f32,f32) ctor.
//
// Text/poly bodies (DrawText/DrawFrame/DrawCircle/DrawWire|SolidConvexPolygon/DrawHorizontalBar) +
// GetVirtualScreenSize (returns an rw Vector2, needs its construction path) are the follow-on -
// declared in the header, not defined here.

namespace CgsDev
{
    void Debug2DImmediateRender::Construct(rw::IResourceAllocator* /*lpAllocator*/, f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight)
    {
        meDrawingMode         = E_DRAWING_COUNT;
        mpRenderBuffer        = nullptr;
        mfVirtualScreenWidth  = lfVirtualScreenWidth;
        mfVirtualScreenHeight = lfVirtualScreenHeight;
        miIm2dVertsHead       = 0;
        // ImRenderer<V>::Render ignores the topology arg (hardcodes triangle-strip); the loading
        // screen passes the same placeholder value 6.
        mePrimitiveType       = static_cast<renderengine::PrimitiveType>(6);
    }

    void Debug2DImmediateRender::Destruct()
    {
        mpRenderBuffer = nullptr;
    }

    void Debug2DImmediateRender::SetRenderBuffer(CgsGraphics::Im2d* lpRenderBuffer)
    {
        mpRenderBuffer = lpRenderBuffer;
    }

    // X360 Begin: open the render block, set the debug render states, reset the vertex batch.
    void Debug2DImmediateRender::Begin()
    {
        mpRenderBuffer->BeginRendering();
        SetDebugRenderStates();
        miIm2dVertsHead = 0;
        meDrawingMode   = E_DRAWING_COUNT;
    }

    // X360 End: flush any batched verts, then close the render block.
    void Debug2DImmediateRender::End()
    {
        DispatchVertices();
        mpRenderBuffer->EndRendering();
    }

    // Alpha-blended, no-texture 2D state - matches the loading screen's SetState(nullptr blend) before
    // an untextured coloured quad. The full debug state set (depth/raster/sampler) is the follow-on.
    void Debug2DImmediateRender::SetDebugRenderStates()
    {
        mpRenderBuffer->SetState(static_cast<const CgsGraphics::BlendState*>(nullptr));
    }

    void Debug2DImmediateRender::DispatchVertices()
    {
        if (miIm2dVertsHead > 0)
        {
            mpRenderBuffer->Render(mePrimitiveType, maIm2dVertsArray, static_cast<u32>(miIm2dVertsHead));
            miIm2dVertsHead = 0;
        }
    }

    void Debug2DImmediateRender::AddVertex(f32 lfX, f32 lfY, RGBA lColour)
    {
        if (miIm2dVertsHead >= KI_VERTEX_BUFFER_SIZE)
            DispatchVertices();

        CgsGraphics::Basic2dColouredTexturedVertex& lrVertex = maIm2dVertsArray[miIm2dVertsHead];
        lrVertex.mv2Pos    = { lfX, lfY };
        lrVertex.mv2Tex0UV = { 0.0f, 0.0f };
        // RGBA is the packed u32 colour; the vertex carries it as RGBA8 (same 4 bytes).
        *reinterpret_cast<u32*>(&lrVertex.mv4Colour) = lColour;
        ++miIm2dVertsHead;
    }

    // A filled box as a 4-vertex triangle strip in TL,TR,BL,BR order, flushed as its own strip.
    void Debug2DImmediateRender::EmitQuad(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        DispatchVertices();   // each quad is its own triangle strip
        AddVertex(lfX0, lfY0, lColour);   // TL
        AddVertex(lfX1, lfY0, lColour);   // TR
        AddVertex(lfX0, lfY1, lColour);   // BL
        AddVertex(lfX1, lfY1, lColour);   // BR
        DispatchVertices();
    }

    // X360 DrawBox (pseudocode is VPU-garbage; reconstructed as the quad it emits).
    void Debug2DImmediateRender::DrawBox(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour)
    {
        EmitQuad(lv2Min.x, lv2Min.y, lv2Max.x, lv2Max.y, lColour);
    }

    void Debug2DImmediateRender::DrawBox(f32 lfX, f32 lfY, f32 lfWidth, f32 lfHeight, RGBA lColour)
    {
        EmitQuad(lfX, lfY, lfX + lfWidth, lfY + lfHeight, lColour);
    }

    // A line is drawn as a thin (1px) quad so it survives the triangle-strip-only Im2d path.
    void Debug2DImmediateRender::DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour)
    {
        const f32 lfDeltaX = lv2End.x - lv2Start.x;
        const f32 lfDeltaY = lv2End.y - lv2Start.y;
        const f32 lfLength = sqrtf(lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY);
        f32 lfNormalX = 0.0f;
        f32 lfNormalY = 0.5f;
        if (lfLength > 0.0f)
        {
            lfNormalX = -lfDeltaY * 0.5f / lfLength;   // perpendicular, half a pixel each side
            lfNormalY =  lfDeltaX * 0.5f / lfLength;
        }

        DispatchVertices();
        AddVertex(lv2Start.x + lfNormalX, lv2Start.y + lfNormalY, lColour);
        AddVertex(lv2Start.x - lfNormalX, lv2Start.y - lfNormalY, lColour);
        AddVertex(lv2End.x   + lfNormalX, lv2End.y   + lfNormalY, lColour);
        AddVertex(lv2End.x   - lfNormalX, lv2End.y   - lfNormalY, lColour);
        DispatchVertices();
    }
}
