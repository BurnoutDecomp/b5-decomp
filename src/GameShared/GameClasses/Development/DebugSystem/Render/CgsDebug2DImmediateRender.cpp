#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include <math.h>   // sqrtf (DrawLine thickness)
#include <cstring>

// X360 0x82824048. Shared screen-space text wrapper used throughout the game debug HUDs.
// ARTIST builds a Vector2 from the two scalar coordinates and forwards every remaining
// argument to Debug2DImmediateRender::DrawText.
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred)
{
    Vector2 lv2Position;
    lv2Position.x = lfX;
    lv2Position.y = lfY;
    lv2Position.z = 0.0f;
    lv2Position.w = 0.0f;
    lpDisplay->DrawText(lpcText, lv2Position, lfScale, lColour, lbCentred);
    return 0;
}
#include <cstdio>

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

    // X360 0x82818838 -- screen-bounds test for a virtual-screen point. The asm runs four
    // sequential single-lane vcmpgefp. compares, each short-circuiting to a 0 return on
    // failure, and returns 1 only if all four pass:
    //   1) point.x >= 0            (vcmpgefp v12[=splat point.x], 0)
    //   2) point.y >= 0            (vcmpgefp splat point.y, 0)
    //   3) mfVirtualScreenWidth  >= point.x   (member @0x34 vs point.x)
    //   4) mfVirtualScreenHeight >= point.y   (member @0x38 vs point.y)
    // The two compared members are the virtual-screen extents, reached here by name
    // (mfVirtualScreenWidth / mfVirtualScreenHeight) rather than by the X360 0x34/0x38 offsets.
    bool Debug2DImmediateRender::Is2DPointOnScreen(Vector2 lv2Point) const
    {
        if (!(lv2Point.x >= 0.0f))
            return false;
        if (!(lv2Point.y >= 0.0f))
            return false;
        if (!(mfVirtualScreenWidth >= lv2Point.x))
            return false;
        if (!(mfVirtualScreenHeight >= lv2Point.y))
            return false;
        return true;
    }

    // ARTIST 0x8281A318. A segment is visible when either endpoint is on-screen or
    // its axis-aligned bounds overlap the virtual-screen rectangle.
    bool Debug2DImmediateRender::Is2DLineOnScreen(Vector2 lv2Start, Vector2 lv2End) const
    {
        if (Is2DPointOnScreen(lv2Start) || Is2DPointOnScreen(lv2End))
            return true;

        const f32 lfMinX = (lv2Start.x < lv2End.x) ? lv2Start.x : lv2End.x;
        const f32 lfMinY = (lv2Start.y < lv2End.y) ? lv2Start.y : lv2End.y;
        const f32 lfMaxX = (lv2Start.x > lv2End.x) ? lv2Start.x : lv2End.x;
        const f32 lfMaxY = (lv2Start.y > lv2End.y) ? lv2Start.y : lv2End.y;
        return lfMaxX >= 0.0f && lfMaxY >= 0.0f &&
               lfMinX <= mfVirtualScreenWidth && lfMinY <= mfVirtualScreenHeight;
    }

    bool Debug2DImmediateRender::Is2DBoxOnScreen(Vector2 lv2Min, Vector2 lv2Max) const
    {
        return lv2Max.x >= 0.0f && lv2Max.y >= 0.0f &&
               lv2Min.x <= mfVirtualScreenWidth && lv2Min.y <= mfVirtualScreenHeight;
    }

    // ARTIST 0x828188F8. Build the polygon bounds and intersect them with the
    // virtual-screen rectangle.
    bool Debug2DImmediateRender::Is2DPolygonOnScreen(const Vector2* lpaPoints, u32 luCount) const
    {
        if (!lpaPoints || luCount == 0)
            return false;

        Vector2 lMin = lpaPoints[0];
        Vector2 lMax = lpaPoints[0];
        for (u32 luIndex = 1; luIndex < luCount; ++luIndex)
        {
            if (lpaPoints[luIndex].x < lMin.x) lMin.x = lpaPoints[luIndex].x;
            if (lpaPoints[luIndex].y < lMin.y) lMin.y = lpaPoints[luIndex].y;
            if (lpaPoints[luIndex].x > lMax.x) lMax.x = lpaPoints[luIndex].x;
            if (lpaPoints[luIndex].y > lMax.y) lMax.y = lpaPoints[luIndex].y;
        }
        return Is2DBoxOnScreen(lMin, lMax);
    }

    // Faithful port of X360 0x823B13A8 -- the debug-font handoff. The game's GamePrepare loads the
    // "Language\Fonts\Default.font" bundle, calls Font::CreateTextureState on the resolved font, then
    // DebugManager::SetDebugFont -> here. Storing a non-null handle flips DrawText off the vector-font
    // fallback onto the resource-font TextRenderer path. (X360 asserts lrFont != NULLResourceHandle.)
    void Debug2DImmediateRender::SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
    {
        // (X360 asserts lrFont != CgsResource::NULLResourceHandle, CgsDebug2DImmediateRender.h:263.)
        mpFont = lrFont;
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

    // X360 @0x8281C960 (the bordered frame around (x0,y0)-(x1,y1)): four DrawBox strips - the
    // top and bottom span the full frame width INCLUDING the corners (x0-border .. x1+border,
    // border tall), the left and right fill between them (border wide, y0 .. y1). The VPU
    // pseudocode is register soup, but each of the four DrawBox(x, y, width, height) argument
    // sets reads off it: box 1 = {x0-b, y0-b, (x1+b)-(x0-b), b}, box 2 = the same strip at y1,
    // boxes 3/4 = the side fills.
    void Debug2DImmediateRender::DrawFrame(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1,
                                           RGBA lColour, f32 lfBorderSize)
    {
        const f32 lfOuterX0    = lfX0 - lfBorderSize;
        const f32 lfOuterWidth = (lfX1 + lfBorderSize) - lfOuterX0;

        DrawBox(lfOuterX0, lfY0 - lfBorderSize, lfOuterWidth, lfBorderSize, lColour);   // top
        DrawBox(lfOuterX0, lfY1,               lfOuterWidth, lfBorderSize, lColour);   // bottom
        DrawBox(lfOuterX0, lfY0,               lfBorderSize, lfY1 - lfY0,  lColour);   // left
        DrawBox(lfX1,      lfY0,               lfBorderSize, lfY1 - lfY0,  lColour);   // right
    }

    // A line is drawn as a thin (1px) quad so it survives the triangle-strip-only Im2d path.
    void Debug2DImmediateRender::DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour)
    {
        if (!Is2DLineOnScreen(lv2Start, lv2End))
            return;

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

    void Debug2DImmediateRender::DrawFrame(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour)
    {
        DrawFrame(lv2Min.x, lv2Min.y, lv2Max.x, lv2Max.y, lColour, 1.0f);
    }

    // ARTIST 0x8281C440. The wire polygon closes the final point back to the first.
    void Debug2DImmediateRender::DrawWirePolygon(const Vector2* lpaPoints, u32 luCount, RGBA lColour)
    {
        if (luCount < 2 || !Is2DPolygonOnScreen(lpaPoints, luCount))
            return;

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            DrawLine(lpaPoints[luIndex], lpaPoints[(luIndex + 1) % luCount], lColour);
    }

    // ARTIST 0x8281C4F0. Emit the convex polygon as the original triangle fan.
    void Debug2DImmediateRender::DrawSolidConvexPolygon(const Vector2* lpaPoints, u32 luCount, RGBA lColour)
    {
        if (luCount < 3 || !Is2DPolygonOnScreen(lpaPoints, luCount))
            return;

        for (u32 luIndex = 1; luIndex + 1 < luCount; ++luIndex)
        {
            DispatchVertices();
            AddVertex(lpaPoints[0].x, lpaPoints[0].y, lColour);
            AddVertex(lpaPoints[luIndex].x, lpaPoints[luIndex].y, lColour);
            AddVertex(lpaPoints[luIndex + 1].x, lpaPoints[luIndex + 1].y, lColour);
            DispatchVertices();
        }
    }

    // ARTIST 0x8281C688. Advance a fixed angular step and join each sample to
    // the next, including the closing segment.
    void Debug2DImmediateRender::DrawCircle(Vector2 lv2Centre, f32 lfRadius,
                                             s32 liSegments, RGBA lColour)
    {
        if (liSegments < 3 || lfRadius <= 0.0f)
            return;

        const f32 lfTwoPi = 6.2831853071795864769f;
        const f32 lfStep = lfTwoPi / static_cast<f32>(liSegments);
        Vector2 lPrevious = { lv2Centre.x + lfRadius, lv2Centre.y, 0.0f, 0.0f };
        for (s32 liIndex = 1; liIndex <= liSegments; ++liIndex)
        {
            const f32 lfAngle = lfStep * static_cast<f32>(liIndex);
            Vector2 lCurrent = {
                lv2Centre.x + cosf(lfAngle) * lfRadius,
                lv2Centre.y + sinf(lfAngle) * lfRadius,
                0.0f,
                0.0f
            };
            DrawLine(lPrevious, lCurrent, lColour);
            lPrevious = lCurrent;
        }
    }

    // ARTIST 0x8281CAC0. A two-pixel frame surrounds the track; the filled
    // portion occupies its interior in proportion to value/max.
    void Debug2DImmediateRender::DrawHorizontalBar(Vector2 lv2Min, Vector2 lv2Max,
                                                    f32 lfValue, f32 lfMax,
                                                    RGBA lBackColour, RGBA lBarColour)
    {
        if (lfMax <= 0.0f || lfValue < 0.0f || lfValue > lfMax)
            return;

        DrawFrame(lv2Min.x, lv2Min.y, lv2Max.x, lv2Max.y, lBackColour, 2.0f);
        const f32 lfWidth = (lv2Max.x - lv2Min.x - 4.0f) * (lfValue / lfMax);
        DrawBox(lv2Min.x + 2.0f, lv2Min.y + 2.0f,
                lfWidth, lv2Max.y - lv2Min.y - 4.0f, lBarColour);
    }

    // Faithful port of X360 0x82823DE0 (the pseudocode is VPU-garbled -- the rect setup is reconstructed
    // from the asm). DrawText branches on whether a font BUNDLE is loaded: if mpFont is set (via
    // SetDebugFont), it builds a TextObject and renders the string through the bitmap-font TextRenderer
    // (CgsResource::Font glyph atlas); otherwise it falls back to the built-in debug VECTOR font
    // (mVectorFont -- the real glyph strokes from the XEX, needing no font resource). lfScale is the
    // text height in px.
    void Debug2DImmediateRender::DrawText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        if (!lpcText)
            return;

        if (HasResourceFont())
        {
            // X360: flush the box batch + switch the renderer to font mode before emitting glyphs.
            if (meDrawingMode != E_DRAWING_FONT)
            {
                DispatchVertices();
                meDrawingMode = E_DRAWING_FONT;
            }

            const CgsResource::CgsUtf8* lpUtf8 = reinterpret_cast<const CgsResource::CgsUtf8*>(lpcText);

            CgsGraphics::TextObject lTextObject;
            lTextObject.Construct(0, 0);                       // defaults: LEFT, white, spacing 1, etc.
            lTextObject.mpFont       = mpFont;                 // the loaded bitmap font handle
            lTextObject.mfFontHeight = lfScale;               // *mpfCurrentFontHeight (set by Construct) -> this
            lTextObject.mTextColour  = lColour;
            lTextObject.meAlignment  = CgsGraphics::TextObject::E_ALIGNMENT_LEFT;
            lTextObject.mpUtf8String = lpUtf8;

            // [PC reconstruction of the X360 VPU rect setup] the box's left/top is the draw position
            // and its height spans the text's lines so RenderStringInternal's per-line loop
            // (penY < mv2BottomRight.y) renders every line. mbMultiLine selects that per-line loop
            // (without it the single-line fast path would swallow the embedded newlines into one
            // line); the box width is deliberately SLACK rather than the exact measured width,
            // because the line walk word-wraps unconditionally and an exact-width box makes the
            // last glyph compare equal to the limit -- wrapping the final word off debug text.
            const f32 lfWidth = mpFont->GetStringWidth(lpUtf8) * lfScale;
            s32 liLines = 1;
            for (const char* lpc = lpcText; *lpc; ++lpc)
                if (*lpc == '\n')
                    ++liLines;
            lTextObject.mbMultiLine    = 1;
            lTextObject.mv2TopLeft     = { lfX, lfY };
            lTextObject.mv2BottomRight = { lfX + lfWidth * 2.0f + lfScale,
                                           lfY + lfScale * static_cast<f32>(liLines) };
            lTextObject.mfStringWidth  = lfWidth;

            if (lTextObject.mbAutosize)
                lTextObject.CalculateAutosizing();

            mTextRenderer.RenderString(mpRenderBuffer, lTextObject);
            return;
        }

        // No bundle: the built-in vector font.
        mVectorFont.SetSize(Vector2{ lfScale, lfScale, 0.0f, 0.0f });
        mVectorFont.Print(lfX, lfY, lpcText, lColour);
    }

    // Positioned overload (optionally centred about lv2Position.x). rw Vector2 is read via .x/.y only.
    void Debug2DImmediateRender::DrawText(const char* lpcText, Vector2 lv2Position, f32 lfScale, RGBA lColour, bool lbCentred)
    {
        f32 lfX = lv2Position.x;
        if (lbCentred && lpcText)
            lfX -= CalcTextWidth(lpcText, lfScale) * 0.5f;
        DrawText(lpcText, lfX, lv2Position.y, lfScale, lColour);
    }

    // ARTIST 0x8282C4C0. Alignment is expressed as 0=left, .5=centre, 1=right.
    void Debug2DImmediateRender::DrawAlignedText(const char* lpcText, f32 lfX, f32 lfY,
                                                 f32 lfScale, RGBA lColour, f32 lfAlignment)
    {
        DrawText(lpcText, lfX - CalcTextWidth(lpcText, lfScale) * lfAlignment,
                 lfY, lfScale, lColour);
    }

    // ARTIST 0x8282C6A8. Size the background from CalcTextExtent, expand it by
    // the caller's border, then render the text at the requested origin.
    void Debug2DImmediateRender::DrawTextWithBackground(const char* lpcText,
                                                         f32 lfX, f32 lfY, f32 lfScale,
                                                         RGBA lTextColour,
                                                         RGBA lBackgroundColour,
                                                         f32 lfBorder)
    {
        const Vector2 lExtent = CalcTextExtent(lpcText, lfScale);
        DrawBox(lfX - lfBorder, lfY - lfBorder,
                lExtent.x + lfBorder * 2.0f,
                lExtent.y + lfBorder * 2.0f,
                lBackgroundColour);
        DrawText(lpcText, lfX, lfY, lfScale, lTextColour);
    }

    // ARTIST 0x82824098.
    void Debug2DImmediateRender::DrawValue(s32 liValue, f32 lfX, f32 lfY,
                                            f32 lfScale, RGBA lColour)
    {
        char lacValue[256];
        std::snprintf(lacValue, sizeof(lacValue), "%d", liValue);
        DrawText(lacValue, lfX, lfY, lfScale, lColour);
    }

    Vector2 Debug2DImmediateRender::GetVirtualScreenSize() const
    {
        return { mfVirtualScreenWidth, mfVirtualScreenHeight, 0.0f, 0.0f };
    }

    // ARTIST 0x8282C548. This is a fixed-width line splitter (rather than word wrapping): derive
    // the maximum character count from the width of "A", honour explicit newlines, align every
    // emitted line independently inside the box, and stop before another line would cross y1.
    void Debug2DImmediateRender::DrawTextInBox(const char* lpcText,
                                                f32 lfX0, f32 lfY0,
                                                f32 lfX1, f32 lfY1,
                                                f32 lfSize, RGBA lColour,
                                                f32 lfAlign)
    {
        if (!lpcText)
            return;

        const f32 lfWidth = lfX1 - lfX0;
        const f32 lfCentreX = lfX0 + lfWidth * lfAlign;
        s32 liMaxCharacters = static_cast<s32>(lfWidth / CalcTextWidth("A", lfSize));
        if (liMaxCharacters > 511)
            liMaxCharacters = 511;

        const char* lpcRead = lpcText;
        f32 lfY = lfY0;
        while (*lpcRead)
        {
            const char* lpcEnd = lpcRead;
            while (*lpcEnd && *lpcEnd != '\n')
                ++lpcEnd;

            s32 liCount = static_cast<s32>(lpcEnd - lpcRead);
            if (liCount > liMaxCharacters)
                liCount = liMaxCharacters;
            if (liCount <= 0 && *lpcRead != '\n')
                break;

            char lacLine[568];
            std::strncpy(lacLine, lpcRead, static_cast<size_t>(liCount));
            lacLine[liCount] = '\0';

            const f32 lfLineWidth = CalcTextWidth(lacLine, lfSize);
            DrawText(lacLine, lfCentreX - lfLineWidth * lfAlign, lfY, lfSize, lColour);

            lfY += lfSize;
            if (lfY + lfSize > lfY1)
                break;

            lpcRead += liCount;
            if (*lpcRead == '\n')
                ++lpcRead;
        }
    }

    // X360 0x82824250. The resource font stores advances in em-like units and is scaled by
    // the requested height; the no-resource path measures the original vector-font table.
    f32 Debug2DImmediateRender::CalcTextWidth(const char* lpcText, f32 lfScale) const
    {
        if (HasResourceFont())
        {
            const CgsResource::CgsUtf8* lpUtf8 =
                reinterpret_cast<const CgsResource::CgsUtf8*>(lpcText);
            return mpFont->GetStringWidth(lpUtf8) * lfScale;
        }

        return VectorFont::ComputeTextExtent(
            lpcText, Vector2{ lfScale, lfScale, 0.0f, 0.0f }).x;
    }

    // X360 0x82824308. Bitmap-font height is exactly the requested text size; vector-font
    // measurement handles embedded newlines and tabs itself.
    Vector2 Debug2DImmediateRender::CalcTextExtent(const char* lpcText, f32 lfScale) const
    {
        if (HasResourceFont())
        {
            const CgsResource::CgsUtf8* lpUtf8 =
                reinterpret_cast<const CgsResource::CgsUtf8*>(lpcText);
            return { mpFont->GetStringWidth(lpUtf8) * lfScale, lfScale, 0.0f, 0.0f };
        }

        return VectorFont::ComputeTextExtent(
            lpcText, Vector2{ lfScale, lfScale, 0.0f, 0.0f });
    }
}
