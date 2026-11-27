#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <math.h>
#include <cstring>

#include <stdlib.h>
#include <vectorintrinsics.h>



// ARTIST 0x82824048, which IDA names "MaybeDrawText", is no global helper: it is the member
// Debug2DImmediateRender::DrawText(const char*, f32, f32, f32, RGBA) (DWARF CgsDebug2DImmediateRender.h:114,
// cpp:409; PS3 DecFIGS 0xB37B4C, called through the trampolines 0x12970/0x4C09B8 by every HUD caller).
// Its body is below with the other DrawText overload.





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
        mVectorFont.SetSize(rw::math::vpu::Vector2(10.0f,10.0f));
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

    // X360 0x82818838 -- screen-bounds test for a virtual-screen point: four single-lane
    // rwmath compares (DecFIGS cpp:1027: operator>=<VectorAxisX,VectorAxisX>, <Y,Y>, then
    // operator<=<X,X>, <Y,Y>; locals lZero :1029, lScreen :1030), each a vcmpgefp. that
    // short-circuits to a 0 return.
    bool Debug2DImmediateRender::Is2DPointOnScreen(const Vector2 lPosition) const
    {
        Vector2 lZero;
        lZero.SetZero();
        Vector2 lScreen = GetVirtualScreenSize();
        // ARTIST's tail (li r3,1 / bnelr / li r3,0) is the if-return-true form.
        if (lPosition.X() >= lZero.X() && lPosition.Y() >= lZero.Y() &&
            lPosition.X() <= lScreen.X() && lPosition.Y() <= lScreen.Y())
        {
            return true;
        }
        return false;
    }

    // ARTIST 82818988: the two segment parameters in rwmath VecFloats (DecFIGS cpp:1214-1238: lfAX..lfDY,
    // lfDenominator, lfR, lfS; VecFloat<VectorAxisX/Y> from the lane references, operator/=, == and the
    // >= / <= compares against float literals, each a { f } fill: 0.0f 0x82001CC0, 1.0f 0x82001C98).
    bool Debug2DImmediateRender::Do2DLinesIntersect(const Vector2 lLineOneStartPos, const Vector2 lLineOneEndPos,
                                                    const Vector2 lLineTwoStartPos, const Vector2 lLineTwoEndPos) const
    {
        VecFloat lfAX = lLineOneStartPos.X();
        VecFloat lfAY = lLineOneStartPos.Y();

        VecFloat lfBX = lLineOneEndPos.X();
        VecFloat lfBY = lLineOneEndPos.Y();

        VecFloat lfCX = lLineTwoStartPos.X();
        VecFloat lfCY = lLineTwoStartPos.Y();

        VecFloat lfDX = lLineTwoEndPos.X();
        VecFloat lfDY = lLineTwoEndPos.Y();

        VecFloat lfDenominator = (lfBX - lfAX) * (lfDY - lfCY) - (lfBY - lfAY) * (lfDX - lfCX);
        if (lfDenominator == 0.0f)
        {
            return false;
        }

        VecFloat lfR = (lfAY - lfCY) * (lfDX - lfCX) - (lfAX - lfCX) * (lfDY - lfCY);
        lfR /= lfDenominator;
        VecFloat lfS = (lfAY - lfCY) * (lfBX - lfAX) - (lfAX - lfCX) * (lfBY - lfAY);
        lfS /= lfDenominator;

        return lfR >= 0.0f && lfR <= 1.0f && lfS >= 0.0f && lfS <= 1.0f;
    }

    // ARTIST 8281A318: test endpoints, then the four viewport edges in order.
    // The screen edges walk from the origin through lEdgeStart/lEdgeEnd, one lane set per edge (DecFIGS
    // cpp:1079-1086: lZero, lScreen, lEdgeStart, lEdgeEnd; SetY<VectorAxisY> / SetX<VectorAxisX>, the
    // vrlimi128 lane inserts at 0x8281A39C/0x8281A3DC/0x8281A410/0x8281A444).
    bool Debug2DImmediateRender::Is2DLineOnScreen(const Vector2 lPoint1, const Vector2 lPoint2) const
    {
        if (Is2DPointOnScreen(lPoint1) || Is2DPointOnScreen(lPoint2))
        {
            return true;
        }

        Vector2 lZero;
        lZero.SetZero();
        Vector2 lScreen = GetVirtualScreenSize();

        // Both edge points start from their own SetZero (ARTIST builds lEdgeEnd from the zero register before
        // the screen size, 0x8281A36C, and passes the zero itself as the first lEdgeStart).
        Vector2 lEdgeStart;
        lEdgeStart.SetZero();
        Vector2 lEdgeEnd;
        lEdgeEnd.SetZero();
        lEdgeEnd.SetY(lScreen.Y());
        if (Do2DLinesIntersect(lPoint1, lPoint2, lEdgeStart, lEdgeEnd))
        {
            return true;
        }

        lEdgeStart = lEdgeEnd;
        lEdgeEnd.SetX(lScreen.X());
        if (Do2DLinesIntersect(lPoint1, lPoint2, lEdgeStart, lEdgeEnd))
        {
            return true;
        }

        lEdgeStart = lEdgeEnd;
        lEdgeEnd.SetY(lZero.Y());
        if (Do2DLinesIntersect(lPoint1, lPoint2, lEdgeStart, lEdgeEnd))
        {
            return true;
        }

        lEdgeStart = lEdgeEnd;
        lEdgeEnd.SetX(lZero.X());
        if (Do2DLinesIntersect(lPoint1, lPoint2, lEdgeStart, lEdgeEnd))
        {
            return true;
        }
        return false;
    }

    bool Debug2DImmediateRender::Is2DBoxOnScreen(Vector2 lv2Min, Vector2 lv2Max) const
    {
        return lv2Max.mV.x >= 0.0f && lv2Max.mV.y >= 0.0f &&
               lv2Min.mV.x <= mfVirtualScreenWidth && lv2Min.mV.y <= mfVirtualScreenHeight;
    }

    // ARTIST 0x828188F8 reads the first vertex unconditionally. The draw callers
    // require at least two vertices. The second bound uses the updated minimum
    // and the current vertex, then the original adds the two vectors.
    bool Debug2DImmediateRender::Is2DPolygonOnScreen(const Vector2* lpaPoints, u32 luCount) const
    {
        Vector2 lv2Minimum = lpaPoints[0];
        Vector2 lv2Upper = lv2Minimum;
        if (luCount > 1)
        {
            const Vector2* lpPoint = lpaPoints + 1;
            u32 luRemaining = luCount - 1;
            do
            {
                const __vector4 lvPoint = lpPoint->mV;
                lv2Minimum.mV = __vminfp(lv2Minimum.mV, lvPoint);
                lv2Upper.mV = __vmaxfp(lv2Minimum.mV, lvPoint);
                ++lpPoint;
            }
            while (--luRemaining != 0);
        }

        Vector2 lv2ScreenMin;
        lv2ScreenMin.SetZero();
        const Vector2 lv2ScreenMax = GetVirtualScreenSize();
        lv2Upper.mV = __vaddfp(lv2Minimum.mV, lv2Upper.mV);
        return Do2DBoxesIntersect(lv2ScreenMin, lv2ScreenMax, lv2Minimum, lv2Upper);
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
#if defined(_XBOX)
        miIm2dVertsHead = 0;
        CGS_ASSERT(mpRenderBuffer && !mpRenderBuffer->IsInARenderingBlock(),
                   "mpRenderBuffer && !mpRenderBuffer->IsInARenderingBlock()");
        if (mpRenderBuffer)
            mpRenderBuffer->BeginRendering();
        SetDebugRenderStates();
        meDrawingMode = E_DRAWING_COUNT;
#else
        mpRenderBuffer->BeginRendering();
        SetDebugRenderStates();
        miIm2dVertsHead = 0;
        meDrawingMode   = E_DRAWING_COUNT;
#endif
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
#if defined(_XBOX)
        if (mpRenderBuffer)
        {
            CgsGraphics::Im2dTransform transform;
            transform.mOriginXYZ = rw::math::vpu::Vector4(-1.0f, 1.0f, 0.0f, 0.0f);
            transform.mRightUp = rw::math::vpu::Vector4(2.0f / mfVirtualScreenWidth, 0.0f,
                                                        0.0f, -2.0f / mfVirtualScreenHeight);
            transform.mColourShift = rw::math::vpu::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            transform.mColourScale = rw::math::vpu::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            mpRenderBuffer->SetTransform(transform);
            const CgsGraphics::ImRendererBase::StateLibrary& states = CgsGraphics::ImRendererBase::GetStateLibrary();
            mpRenderBuffer->SetState(states.mpBlendState_Standard);
            mpRenderBuffer->SetState(states.mpRasterizerState_CullNone);
            mpRenderBuffer->SetState(states.mpDepthStencilState_ZBufferOff);
        }
#else
        mpRenderBuffer->SetState(static_cast<const renderengine::BlendState*>(0));

        // CRITICAL: the debug prims are solid-coloured, but the Im2d stage modulates texture x diffuse
        // and the loading screen leaves its texture bound - so without unbinding it the boxes sample
        // that texture at UV(0,0) instead of drawing their colour (the squares were invisible). Drop the
        // texture; an unbound D3D stage defaults to white, leaving pure vertex colour.
        mpRenderBuffer->SetTexture(0);
#endif
    }

    void Debug2DImmediateRender::DispatchVertices()
    {
        // ARTIST 8281B6F4 tests the signed short for a nonzero vertex count.
        if (miIm2dVertsHead != 0)
        {
            if (mpRenderBuffer)
            {
                // ARTIST 8281B718..8281B740: restore the shared untextured state
                // before copying the batch; preserve the selected drawing mode.
#if defined(_XBOX)
                mpRenderBuffer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpTextureState_Untextured);
#endif
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
        // The vertex carries the rw::RGBA word as its RGBA8 (same 4 bytes).
        *reinterpret_cast<RGBA*>(&lrVertex.mv4Colour) = lColour;
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

    // ARTIST 82816E50: reject only a strict separating comparison per lane (DecFIGS cpp:1257-1259:
    // the loop index liIndex and rwmath operator< / operator> on the lane references). Reversed
    // corners are not sorted, and an unordered comparison does not separate.
    bool Debug2DImmediateRender::Do2DBoxesIntersect(const Vector2 lBox0Min, const Vector2 lBox0Max,
                                                   const Vector2 lBox1Min, const Vector2 lBox1Max) const
    {
        for (int32_t liIndex = 0; liIndex < 2; ++liIndex)
        {
            if (lBox0Max[liIndex] < lBox1Min[liIndex] || lBox0Min[liIndex] > lBox1Max[liIndex])
            {
                return false;
            }
        }
        return true;
    }

    // ARTIST 8281C2D8 adds the SIZE vector to POSITION before culling and AddQuad.
    // EmitQuad retains the existing native triangle-strip adapter.
    void Debug2DImmediateRender::DrawBox(Vector2 lv2Position, Vector2 lv2Size, RGBA lColour)
    {
        Vector2 lv2End;
        lv2End.mV = __vaddfp(lv2Position.mV, lv2Size.mV);   // ARTIST vaddfp128: all four lanes
        if (!Do2DBoxesIntersect(rw::math::vpu::Vector2(0, 0), GetVirtualScreenSize(), lv2Position, lv2End))
            return;
        EmitQuad(lv2Position.mV.x, lv2Position.mV.y, lv2End.mV.x, lv2End.mV.y, lColour);
    }

    // ARTIST export hole 8281C3E0: f3-f1 and f4-f2 produce the vector size.
    void Debug2DImmediateRender::DrawBox(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        DrawBox(rw::math::vpu::Vector2(lfX0, lfY0), rw::math::vpu::Vector2(lfX1 - lfX0, lfY1 - lfY0), lColour);
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

        DrawBox(rw::math::vpu::Vector2(lfOuterX0, lfY0 - lfBorderSize),
                rw::math::vpu::Vector2(lfOuterWidth, lfBorderSize), lColour); // top
        DrawBox(rw::math::vpu::Vector2(lfOuterX0, lfY1),
                rw::math::vpu::Vector2(lfOuterWidth, lfBorderSize), lColour); // bottom
        DrawBox(rw::math::vpu::Vector2(lfOuterX0, lfY0),
                rw::math::vpu::Vector2(lfBorderSize, lfY1 - lfY0), lColour); // left
        DrawBox(rw::math::vpu::Vector2(lfX1, lfY0),
                rw::math::vpu::Vector2(lfBorderSize, lfY1 - lfY0), lColour); // right
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
        AddVertex(lv2Start.mV.x, lv2Start.mV.y, lColour);
        AddVertex(lv2End.mV.x, lv2End.mV.y, lColour);
    }

    void Debug2DImmediateRender::DrawFrame(Vector2 lv2Min, Vector2 lv2Max, RGBA lColour)
    {
        DrawFrame(lv2Min.mV.x, lv2Min.mV.y, lv2Max.mV.x, lv2Max.mV.y, lColour, 1.0f);
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
            AddVertex(lpaPoints[0].mV.x, lpaPoints[0].mV.y, lColour);
            AddVertex(lpaPoints[luIndex].mV.x, lpaPoints[luIndex].mV.y, lColour);
            AddVertex(lpaPoints[luIndex + 1].mV.x, lpaPoints[luIndex + 1].mV.y, lColour);
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
        Vector2 lPrevious = rw::math::vpu::Vector2(lv2Centre.mV.x + lfRadius, lv2Centre.mV.y);
        for (s32 liIndex = 1; liIndex <= liSegments; ++liIndex)
        {
            const f32 lfAngle = lfStep * static_cast<f32>(liIndex);
            Vector2 lCurrent = rw::math::vpu::Vector2(lv2Centre.mV.x + cosf(lfAngle) * lfRadius,
                lv2Centre.mV.y + sinf(lfAngle) * lfRadius);
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

        DrawBox(lv2Min.mV.x, lv2Min.mV.y, lv2Max.mV.x, lv2Max.mV.y, lBackColour);
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
        DrawBox(lv2Min.mV.x, lfTop, lfRight, lfBottom, lForeColour);
    }

    // ARTIST 0x82823DE0 (DWARF cpp:361; locals lTextObject cpp:364, lBottomRight cpp:365). The font
    // path builds a TextObject and renders it through the bitmap-font TextRenderer (CgsResource::Font
    // glyph atlas) when a font bundle is loaded (mpFont set by SetDebugFont); otherwise the built-in
    // debug VECTOR font draws it. lfSize is the text height in px.
    void Debug2DImmediateRender::DrawText(const char* lpcText, Vector2 lPosition, f32 lfSize, RGBA lColourRGBA, bool lbCull)
    {
        // PS3 culls through Is2DTextOnScreen (cpp:1056); ARTIST 0x82823E14 calls the point test.
        if (lbCull && !Is2DPointOnScreen(lPosition))
            return;

        if (mpFont != CgsResource::NULLResourceHandle)    // DWARF: SafeResourceHandle<Font>::operator!=
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
            lTextObject.meAlignment  = CgsGraphics::TextObject::E_ALIGNMENT_LEFT;
            lTextObject.mfFontHeight = lfSize;
            // TextObject::SetText (DWARF CgsFontRenderer.h:146, inlined: ARTIST 82823E94..82823EC4).
            lTextObject.mpUtf8String = lpUtf8;
            if (lTextObject.mbAutosize)
                lTextObject.CalculateAutosizing();
            lTextObject.mTextColour  = lColourRGBA;

            // The box is exactly one font-height tall; its width is the scaled string width.
            const f32 lfWidth = mpFont->GetStringWidth(lpUtf8) * lfSize;
            lTextObject.mv2TopLeft.mX = lPosition.mV.x;
            lTextObject.mv2TopLeft.mY = lPosition.mV.y;
            lTextObject.mv2BottomRight.mX = lPosition.mV.x + lfWidth;
            lTextObject.mv2BottomRight.mY = lPosition.mV.y + lfSize;

            // TextObject::SetStringWidth (DWARF CgsFontRenderer.h:153, inlined: ARTIST 82823F78..82823F98):
            // the cached width is unscaled.
            lTextObject.mfStringWidth = mpFont->GetStringWidth(lTextObject.mpUtf8String);
            if (lTextObject.mbAutosize)
                lTextObject.CalculateAutosizing();

            mTextRenderer.RenderString(mpRenderBuffer, lTextObject);
        }
        else
        {
            // No bundle: the built-in vector font.
            if (meDrawingMode != E_DRAWING_LINES)
            {
                DispatchVertices();
                meDrawingMode = E_DRAWING_LINES;
                mePrimitiveType = static_cast<renderengine::PrimitiveType>(2);
            }
            mVectorFont.SetSize(rw::math::vpu::Vector2(lfSize, lfSize));
            mVectorFont.Print(lPosition.mV.x, lPosition.mV.y, lpcText, lColourRGBA);
        }
    }

    // ARTIST 0x82824048 (DWARF cpp:409, local lPosition cpp:411): pack (x, y, 0, 0) and draw
    // without culling.
    void Debug2DImmediateRender::DrawText(const char* lpcText, f32 lfX, f32 lfY, f32 lfSize, RGBA lColourRGBA)
    {
        const Vector2 lPosition(lfX, lfY);
        DrawText(lpcText, lPosition, lfSize, lColourRGBA, false);
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
        DrawBox(rw::math::vpu::Vector2(lfX - lfBorder, lfY - lfBorder),
                rw::math::vpu::Vector2(lExtent.mV.x + lfBorder * 2.0f, lExtent.mV.y + lfBorder * 2.0f),
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

        const Vector2 lv2Position = rw::math::vpu::Vector2(lfX, lfY);
        DrawText(lacSize, lv2Position, lfScale, lColour, false);
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
            lpcText, rw::math::vpu::Vector2( lfScale, lfScale)).mV.x;
    }

    // X360 0x82824308. Bitmap-font height is exactly the requested text size; vector-font
    // measurement handles embedded newlines and tabs itself.
    Vector2 Debug2DImmediateRender::CalcTextExtent(const char* lpcText, f32 lfScale) const
    {
        if (HasResourceFont())
        {
            const CgsResource::CgsUtf8* lpUtf8 =
                reinterpret_cast<const CgsResource::CgsUtf8*>(lpcText);
            return rw::math::vpu::Vector2(mpFont->GetStringWidth(lpUtf8) * lfScale, lfScale);
        }

        return VectorFont::ComputeTextExtent(
            lpcText, rw::math::vpu::Vector2( lfScale, lfScale));
    }
}
