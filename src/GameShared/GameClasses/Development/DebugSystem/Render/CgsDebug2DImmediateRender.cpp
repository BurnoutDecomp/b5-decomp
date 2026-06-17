#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <math.h>   // sqrtf (DrawLine thickness)

namespace
{
    // PLACEHOLDER debug font. The X360 DrawText renders through a VectorFont / TextRenderer loaded as a
    // resource (CgsResource::Font); that font-load path is not reconstructed yet, so - to get the debug
    // text on screen now - DrawText rasterises this built-in 5x7 dot-matrix font as small solid quads.
    // Column-major: 5 bytes per glyph (one per column, left->right), bit N (0=top..6=bottom) = pixel on.
    // Covers ASCII 0x20 (space) .. 0x5A ('Z'); lower-case is upper-cased, anything else advances blank.
    const unsigned char KAU8_DEBUG_FONT_5X7[59][5] =
    {
        {0x00,0x00,0x00,0x00,0x00}, // 0x20 space
        {0x00,0x00,0x5F,0x00,0x00}, // !
        {0x00,0x07,0x00,0x07,0x00}, // "
        {0x14,0x7F,0x14,0x7F,0x14}, // #
        {0x24,0x2A,0x7F,0x2A,0x12}, // $
        {0x23,0x13,0x08,0x64,0x62}, // %
        {0x36,0x49,0x55,0x22,0x50}, // &
        {0x00,0x05,0x03,0x00,0x00}, // '
        {0x00,0x1C,0x22,0x41,0x00}, // (
        {0x00,0x41,0x22,0x1C,0x00}, // )
        {0x14,0x08,0x3E,0x08,0x14}, // *
        {0x08,0x08,0x3E,0x08,0x08}, // +
        {0x00,0x50,0x30,0x00,0x00}, // ,
        {0x08,0x08,0x08,0x08,0x08}, // -
        {0x00,0x60,0x60,0x00,0x00}, // .
        {0x20,0x10,0x08,0x04,0x02}, // /
        {0x3E,0x51,0x49,0x45,0x3E}, // 0
        {0x00,0x42,0x7F,0x40,0x00}, // 1
        {0x42,0x61,0x51,0x49,0x46}, // 2
        {0x21,0x41,0x45,0x4B,0x31}, // 3
        {0x18,0x14,0x12,0x7F,0x10}, // 4
        {0x27,0x45,0x45,0x45,0x39}, // 5
        {0x3C,0x4A,0x49,0x49,0x30}, // 6
        {0x01,0x71,0x09,0x05,0x03}, // 7
        {0x36,0x49,0x49,0x49,0x36}, // 8
        {0x06,0x49,0x49,0x29,0x1E}, // 9
        {0x00,0x36,0x36,0x00,0x00}, // :
        {0x00,0x56,0x36,0x00,0x00}, // ;
        {0x08,0x14,0x22,0x41,0x00}, // <
        {0x14,0x14,0x14,0x14,0x14}, // =
        {0x00,0x41,0x22,0x14,0x08}, // >
        {0x02,0x01,0x51,0x09,0x06}, // ?
        {0x32,0x49,0x79,0x41,0x3E}, // @
        {0x7E,0x11,0x11,0x11,0x7E}, // A
        {0x7F,0x49,0x49,0x49,0x36}, // B
        {0x3E,0x41,0x41,0x41,0x22}, // C
        {0x7F,0x41,0x41,0x22,0x1C}, // D
        {0x7F,0x49,0x49,0x49,0x41}, // E
        {0x7F,0x09,0x09,0x09,0x01}, // F
        {0x3E,0x41,0x49,0x49,0x7A}, // G
        {0x7F,0x08,0x08,0x08,0x7F}, // H
        {0x00,0x41,0x7F,0x41,0x00}, // I
        {0x20,0x40,0x41,0x3F,0x01}, // J
        {0x7F,0x08,0x14,0x22,0x41}, // K
        {0x7F,0x40,0x40,0x40,0x40}, // L
        {0x7F,0x02,0x0C,0x02,0x7F}, // M
        {0x7F,0x04,0x08,0x10,0x7F}, // N
        {0x3E,0x41,0x41,0x41,0x3E}, // O
        {0x7F,0x09,0x09,0x09,0x06}, // P
        {0x3E,0x41,0x51,0x21,0x5E}, // Q
        {0x7F,0x09,0x19,0x29,0x46}, // R
        {0x46,0x49,0x49,0x49,0x31}, // S
        {0x01,0x01,0x7F,0x01,0x01}, // T
        {0x3F,0x40,0x40,0x40,0x3F}, // U
        {0x1F,0x20,0x40,0x20,0x1F}, // V
        {0x7F,0x20,0x18,0x20,0x7F}, // W
        {0x63,0x14,0x08,0x14,0x63}, // X
        {0x03,0x04,0x78,0x04,0x03}, // Y
        {0x61,0x51,0x49,0x45,0x43}, // Z
    };
}

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
        mVectorFont.Construct();
        mVectorFont.SetRenderer(this);   // glyph strokes draw through this renderer's DrawLine
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

        // CRITICAL: the debug prims are solid-coloured, but the Im2d stage modulates texture x diffuse
        // and the loading screen leaves its texture bound - so without unbinding it the boxes sample
        // that texture at UV(0,0) instead of drawing their colour (the squares were invisible). Drop the
        // texture; an unbound D3D stage defaults to white, leaving pure vertex colour.
        mpRenderBuffer->SetTexture(nullptr);
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

    // X360 DrawText renders the debug VECTOR font; reconstructed via mVectorFont (the real glyph strokes
    // carved from the XEX - CgsVectorFontData.h). lfScale is the text height in px. The resource-font
    // (CgsResource::Font / TextRenderer) path is the follow-on; the vector font needs no font resource.
    void Debug2DImmediateRender::DrawText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        if (!lpcText)
            return;
        mVectorFont.SetSize(Vector2{ lfScale, lfScale, 0.0f, 0.0f });
        mVectorFont.Print(lfX, lfY, lpcText, lColour);
    }

    // Positioned overload (optionally centred about lv2Position.x). rw Vector2 is read via .x/.y only.
    void Debug2DImmediateRender::DrawText(const char* lpcText, Vector2 lv2Position, f32 lfScale, RGBA lColour, bool lbCentred)
    {
        f32 lfX = lv2Position.x;
        if (lbCentred && lpcText)
        {
            s32 liLength = 0;
            for (const char* lpc = lpcText; *lpc; ++lpc)
                ++liLength;
            // Approximate centring (mean vector-glyph advance ~ 0.375 * height); exact ComputeTextWidth
            // over KAN_CHARWIDTH is the follow-on.
            lfX -= static_cast<f32>(liLength) * lfScale * 0.375f * 0.5f;
        }
        DrawText(lpcText, lfX, lv2Position.y, lfScale, lColour);
    }
}
