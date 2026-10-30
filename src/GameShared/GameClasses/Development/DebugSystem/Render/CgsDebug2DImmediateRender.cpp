#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf

#include <math.h>
#include <cstring>

#if defined(_XBOX)
#include <vectorintrinsics.h>
#endif

namespace
{
    rw::math::vpu::Vector2 MakeDebugVector2(float x, float y, float z, float w)
    {
        const rw::math::vpu::Vector2 value = { x, y, z, w };
        return value;
    }
}

namespace CgsDev
{
    void Debug2DImmediateRender::Construct(rw::IResourceAllocator* /*lpAllocator*/, f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight)
    {
        meDrawingMode         = E_DRAWING_COUNT;
        mpRenderBuffer        = 0;
        mfVirtualScreenWidth  = lfVirtualScreenWidth;
        mfVirtualScreenHeight = lfVirtualScreenHeight;
        miIm2dVertsHead       = 0;
        mpFont.Clear(); // ARTIST 8281A2D0..8281A2D4: restore the null font handle.
        mVectorFont.Construct();
        mVectorFont.SetSize(MakeDebugVector2(10.0f,10.0f,0.0f,0.0f));
        mVectorFont.SetRenderer(this);   // glyph strokes draw through this renderer's DrawLine
        mTextRenderer.Construct(); // ARTIST 8281A304
    }

    void Debug2DImmediateRender::Destruct()
    {
        mpRenderBuffer = 0;
    }

    void Debug2DImmediateRender::SetRenderBuffer(CgsGraphics::Im2dRenderBuffer* lpRenderBuffer)
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
        mpRenderBuffer->SetState(static_cast<const renderengine::BlendState*>(0));

        // CRITICAL: the debug prims are solid-coloured, but the Im2d stage modulates texture x diffuse
        // and the loading screen leaves its texture bound - so without unbinding it the boxes sample
        // that texture at UV(0,0) instead of drawing their colour (the squares were invisible). Drop the
        // texture; an unbound D3D stage defaults to white, leaving pure vertex colour.
        mpRenderBuffer->SetTexture(0);
    }

    void Debug2DImmediateRender::DispatchVertices()
    {
        if (miIm2dVertsHead > 0)
        {
            if (mpRenderBuffer)
            {
                // ARTIST 8281B718..8281B740: restore the shared untextured state
                // before copying the batch; preserve the selected drawing mode.
                mpRenderBuffer->Render(mePrimitiveType, maIm2dVertsArray, static_cast<u32>(miIm2dVertsHead));
            }
            miIm2dVertsHead = 0;
        }
    }

    void Debug2DImmediateRender::AddVertex(f32 lfX, f32 lfY, RGBA lColour)
    {
        if (miIm2dVertsHead >= KI_VERTEX_BUFFER_SIZE)
            DispatchVertices();

        CgsGraphics::Basic2dColouredTexturedVertex& lrVertex = maIm2dVertsArray[miIm2dVertsHead];
        lrVertex.mv2Pos.x = lfX;
        lrVertex.mv2Pos.y = lfY;
        lrVertex.mv2Tex0UV.x = 0.0f;
        lrVertex.mv2Tex0UV.y = 0.0f;
        // RGBA is the packed u32 colour; the vertex carries it as RGBA8 (same 4 bytes).
        *reinterpret_cast<u32*>(&lrVertex.mv4Colour) = lColour;
        ++miIm2dVertsHead;
    }

    // A filled box as a 4-vertex triangle strip in TL,TR,BL,BR order, flushed as its own strip.
    void Debug2DImmediateRender::EmitQuad(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        DispatchVertices();   // each quad is its own triangle strip
        mePrimitiveType = static_cast<renderengine::PrimitiveType>(6);
        AddVertex(lfX0, lfY0, lColour);   // TL
        AddVertex(lfX1, lfY0, lColour);   // TR
        AddVertex(lfX0, lfY1, lColour);   // BL
        AddVertex(lfX1, lfY1, lColour);   // BR
        DispatchVertices();
    }

    // ARTIST 82816E50: reject only a strict separating comparison. Reversed
    // corners are not sorted, and an unordered comparison does not separate.
    bool Debug2DImmediateRender::Do2DBoxesIntersect(Vector2 lv2MinA, Vector2 lv2MaxA,
                                                   Vector2 lv2MinB, Vector2 lv2MaxB) const
    {
        if (lv2MinB.x > lv2MaxA.x || lv2MinA.x > lv2MaxB.x)
            return false;
        if (lv2MinB.y > lv2MaxA.y || lv2MinA.y > lv2MaxB.y)
            return false;
        return true;
    }

    // ARTIST 8281C2D8 adds the SIZE vector to POSITION before culling and AddQuad.
    // EmitQuad retains the existing native triangle-strip adapter.
    void Debug2DImmediateRender::DrawBox(Vector2 lv2Position, Vector2 lv2Size, RGBA lColour)
    {
        const Vector2 lv2End = {lv2Position.x + lv2Size.x, lv2Position.y + lv2Size.y,
                               lv2Position.z + lv2Size.z, lv2Position.w + lv2Size.w};
        if (!Do2DBoxesIntersect(MakeDebugVector2(0, 0, 0, 0), GetVirtualScreenSize(), lv2Position, lv2End))
            return;
        EmitQuad(lv2Position.x, lv2Position.y, lv2End.x, lv2End.y, lColour);
    }

    // ARTIST export hole 8281C3E0: f3-f1 and f4-f2 produce the vector size.
    void Debug2DImmediateRender::DrawBox(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        DrawBox(MakeDebugVector2(lfX0, lfY0, 0, 0), MakeDebugVector2(lfX1 - lfX0, lfY1 - lfY0, 0, 0), lColour);
    }

    // X360 @0x8281C960 (the bordered frame around (x0,y0)-(x1,y1)): four DrawBox strips - the
    // top and bottom span the full frame width INCLUDING the corners (x0-border .. x1+border,
    // border tall), the left and right fill between them (border wide, y0 .. y1). The VPU
    // pseudocode is register soup, but each of the four DrawBox(position, size) argument
    // sets reads off it: box 1 = {x0-b, y0-b, (x1+b)-(x0-b), b}, box 2 = the same strip at y1,
    // boxes 3/4 = the side fills.
    void Debug2DImmediateRender::DrawFrame(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1,
                                           RGBA lColour, f32 lfBorderSize)
    {
        const f32 lfOuterX0    = lfX0 - lfBorderSize;
        const f32 lfOuterWidth = (lfX1 + lfBorderSize) - lfOuterX0;

        DrawBox(MakeDebugVector2(lfOuterX0, lfY0 - lfBorderSize, 0, 0),
                MakeDebugVector2(lfOuterWidth, lfBorderSize, 0, 0), lColour); // top
        DrawBox(MakeDebugVector2(lfOuterX0, lfY1, 0, 0),
                MakeDebugVector2(lfOuterWidth, lfBorderSize, 0, 0), lColour); // bottom
        DrawBox(MakeDebugVector2(lfOuterX0, lfY0, 0, 0),
                MakeDebugVector2(lfBorderSize, lfY1 - lfY0, 0, 0), lColour); // left
        DrawBox(MakeDebugVector2(lfX1, lfY0, 0, 0),
                MakeDebugVector2(lfBorderSize, lfY1 - lfY0, 0, 0), lColour); // right
    }

    // ARTIST 8281C1F0: line-list mode and the original two screen-space endpoints.
    void Debug2DImmediateRender::DrawLine(Vector2 lv2Start, Vector2 lv2End, RGBA lColour)
    {
        if (!Is2DLineOnScreen(lv2Start, lv2End))
            return;

        if (meDrawingMode != E_DRAWING_LINES)
        {
            DispatchVertices();
            meDrawingMode = E_DRAWING_LINES;
            mePrimitiveType = static_cast<renderengine::PrimitiveType>(2);
        }
        if (miIm2dVertsHead >= KI_VERTEX_BUFFER_SIZE - 2)
            DispatchVertices();
        AddVertex(lv2Start.x, lv2Start.y, lColour);
        AddVertex(lv2End.x, lv2End.y, lColour);
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

        DispatchVertices();
        meDrawingMode = E_DRAWING_TRIANGLES;
        mePrimitiveType = static_cast<renderengine::PrimitiveType>(4);

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

    // ARTIST 8281CAC0. The second color fills the complete background first;
    // the first color is the original asymmetric inset, including a one-unit
    // foreground at value zero. Its right endpoint uses a fused multiply-add.
    void Debug2DImmediateRender::DrawHorizontalBar(Vector2 lv2Min, Vector2 lv2Max,
                                                    f32 lfValue, f32 lfMax,
                                                    RGBA lForeColour, RGBA lBackColour)
    {
        // The first branch is not-GT (so unordered max rejects); the value
        // branches are strict LT/GT (so unordered value proceeds).
        if (!(lfMax > 0.0f) || lfValue < 0.0f || lfValue > lfMax)
            return;

        DrawBox(lv2Min.x, lv2Min.y, lv2Max.x, lv2Max.y, lBackColour);
#if defined(_XBOX)
        // ARTIST 8281CB48..CC30: splat input lanes; perform each inset
        // operation in VMX before the fused multiply-add.
        const __vector4 lMin = __lvx(&lv2Min, 0);
        const __vector4 lMax = __lvx(&lv2Max, 0);
        const __vector4 lMinX = __vspltw(lMin, 0);
        const __vector4 lMinY = __vspltw(lMin, 1);
        const __vector4 lMaxX = __vspltw(lMax, 0);
        const __vector4 lMaxY = __vspltw(lMax, 1);
        const __vector4 lTwo = __vcfsx(__vspltisw(2), 0);
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lSpan = __vsubfp(__vsubfp(lMaxX, lMinX), lTwo);
        const __vector4 lOrigin = __vaddfp(lMinX, lOne);
        const __vector4 lTop = __vaddfp(lMinY, lTwo);
        const __vector4 lBottom = __vsubfp(lMaxY, lTwo);
        __declspec(align(16)) f32 laFraction[4] = { lfValue / lfMax, 0, 0, 0 };
        const __vector4 lFraction = __vspltw(__lvx(laFraction, 0), 0);
        const __vector4 lRight = __vmaddfp(lSpan, lOrigin, lFraction);
        __declspec(align(16)) f32 laResult[4];
        __stvx(lRight, laResult, 0);
        const f32 lfRight = laResult[0];
        __stvx(lTop, laResult, 0);
        const f32 lfTop = laResult[0];
        __stvx(lBottom, laResult, 0);
        const f32 lfBottom = laResult[0];
#else
        const f32 lfSpan = (lv2Max.x - lv2Min.x) - 2.0f;
        const f32 lfRight = fmaf(lfSpan, lv2Min.x + 1.0f, lfValue / lfMax);
        const f32 lfTop = lv2Min.y + 2.0f;
        const f32 lfBottom = lv2Max.y - 2.0f;
#endif
        DrawBox(lv2Min.x, lfTop, lfRight, lfBottom, lForeColour);
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

            // ARTIST 82823EAC writes alignment at +0x2C, not multiline at +0x30.
            // The object remains single-line. Its box is exactly one font-height
            // tall; the cached string width is unscaled (82823F84..82823F88).
            const f32 lfWidth = mpFont->GetStringWidth(lpUtf8) * lfScale;
            lTextObject.mv2TopLeft.mX = lfX;
            lTextObject.mv2TopLeft.mY = lfY;
            lTextObject.mv2BottomRight.mX = lfX + lfWidth;
            lTextObject.mv2BottomRight.mY = lfY + lfScale;
            lTextObject.mfStringWidth  = mpFont->GetStringWidth(lpUtf8);

            if (lTextObject.mbAutosize)
                lTextObject.CalculateAutosizing();

            mTextRenderer.RenderString(mpRenderBuffer, lTextObject);
            return;
        }

        // No bundle: the built-in vector font.
        if (meDrawingMode != E_DRAWING_LINES)
        {
            DispatchVertices();
            meDrawingMode = E_DRAWING_LINES;
            mePrimitiveType = static_cast<renderengine::PrimitiveType>(2);
        }
        mVectorFont.SetSize(MakeDebugVector2( lfScale, lfScale, 0.0f, 0.0f ));
        mVectorFont.Print(lfX, lfY, lpcText, lColour);
    }

    // ARTIST 82823E08..82823E20: the final bool requests origin culling.
    void Debug2DImmediateRender::DrawText(const char* lpcText, Vector2 lv2Position, f32 lfScale, RGBA lColour, bool lbCheckScreen)
    {
        if (lbCheckScreen && !Is2DPointOnScreen(lv2Position))
            return;
        DrawText(lpcText, lv2Position.x, lv2Position.y, lfScale, lColour);
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
        DrawBox(MakeDebugVector2(lfX - lfBorder, lfY - lfBorder, 0, 0),
                MakeDebugVector2(lExtent.x + lfBorder * 2.0f, lExtent.y + lfBorder * 2.0f, 0, 0),
                lBackgroundColour);
        DrawText(lpcText, lfX, lfY, lfScale, lTextColour);
    }

    // ARTIST 0x82824098.
    void Debug2DImmediateRender::DrawValue(s32 liValue, f32 lfX, f32 lfY,
                                            f32 lfScale, RGBA lColour)
    {
        char lacValue[256];
        CgsCore::SPrintf(lacValue, sizeof(lacValue), "%d", liValue);
        DrawText(lacValue, lfX, lfY, lfScale, lColour);
    }

    // ARTIST 0x82824140..0x82824248 (export hole); DecFIGS h:126.
    void Debug2DImmediateRender::DrawDataSize(s32 liBytes, f32 lfX, f32 lfY,
                                               f32 lfScale, RGBA lColour)
    {
        char lacSize[256];
        const s32 liMegabytes = liBytes / 0x100000;
        const s32 liKilobytes = liBytes % 0x100000 / 1024;
        const s32 liRemainder = liBytes % 0x100000 % 1024;

        if (liMegabytes <= 0)
        {
            if (liKilobytes <= 0)
                CgsCore::SPrintf(lacSize, sizeof(lacSize), "%dB", liRemainder);
            else
                CgsCore::SPrintf(lacSize, sizeof(lacSize), "%dKB, %dB", liKilobytes, liRemainder);
        }
        else
        {
            CgsCore::SPrintf(lacSize, sizeof(lacSize), "%dMB, %dKB, %dB",
                             liMegabytes, liKilobytes, liRemainder);
        }

        const Vector2 lv2Position = {lfX, lfY, 0.0f, 0.0f};
        DrawText(lacSize, lv2Position, lfScale, lColour, false);
    }

    Vector2 Debug2DImmediateRender::GetVirtualScreenSize() const
    {
        return MakeDebugVector2(mfVirtualScreenWidth, mfVirtualScreenHeight, 0.0f, 0.0f);
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
            lpcText, MakeDebugVector2( lfScale, lfScale, 0.0f, 0.0f )).x;
    }

    // X360 0x82824308. Bitmap-font height is exactly the requested text size; vector-font
    // measurement handles embedded newlines and tabs itself.
    Vector2 Debug2DImmediateRender::CalcTextExtent(const char* lpcText, f32 lfScale) const
    {
        if (HasResourceFont())
        {
            const CgsResource::CgsUtf8* lpUtf8 =
                reinterpret_cast<const CgsResource::CgsUtf8*>(lpcText);
            return MakeDebugVector2(mpFont->GetStringWidth(lpUtf8) * lfScale, lfScale, 0.0f, 0.0f);
        }

        return VectorFont::ComputeTextExtent(
            lpcText, MakeDebugVector2( lfScale, lfScale, 0.0f, 0.0f ));
    }
}
