#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // IncrementUtf8Pointer

#include <cmath>   // fabsf

// CgsGraphics::TextObject + TextRenderer bodies (faithful X360 ARTIST ports):
//   TextObject::Construct       0x827EEE70
//   TextRenderer::RenderString  0x82801998
//   TextRenderer::RenderStringInternal  0x827FF670  (translated from the PPC asm -- the Hex-Rays
//       output was VPU-garbled; see progress/scratch_dossiers/rsi_notes.md for the full decode)

namespace CgsGraphics
{
    // X360 dword_82F30FDC -- the default text/background colour the TextObject ctor fills in
    // (opaque white). NOTE: value assumed from the global's default; verify against init data.
    static const RGBA KU_DEFAULT_COLOUR = 0xFFFFFFFFu;

    // Faithful port of X360 0x827EEE70. Fills the default state: alignment LEFT, char-spacing 1.0,
    // string-width -1 (unmeasured), current-height -> mfFontHeight, all colours = default white,
    // font = the default (invalid) handle; the alternate-colour table args are stored as given.
    void TextObject::Construct(const RGBA* lpAlternateColours, s32 liNumAlternateColours)
    {
        // X360 default font handle (dword_83011A50/A54): the invalid/null SafeResourceHandle.
        mpFont.mpResource = 0;
        mpFont.muSafetyId = 0;

        mfFontHeight          = 0.0f;
        mfAutosizedFontHeight = 0.0f;
        mpfCurrentFontHeight  = &mfFontHeight;

        mTextColour = KU_DEFAULT_COLOUR;

        mv2TopLeft.mX = 0.0f;
        mv2TopLeft.mY = 0.0f;
        mv2BottomRight.mX = 0.0f;
        mv2BottomRight.mY = 0.0f;

        mfCharSpacingMultiplier = 1.0f;
        meAlignment = E_ALIGNMENT_LEFT;

        mbMultiLine  = 0;
        mbWordWrap   = 0;
        mbItalic     = 0;
        mbBackground = 0;
        mbBorder     = 0;
        mbGradient   = false;
        mbDropShadow = false;
        mbEmbossed   = false;

        mBackgroundColours.mTopLeft     = KU_DEFAULT_COLOUR;
        mBackgroundColours.mTopRight    = KU_DEFAULT_COLOUR;
        mBackgroundColours.mBottomLeft  = KU_DEFAULT_COLOUR;
        mBackgroundColours.mBottomRight = KU_DEFAULT_COLOUR;
        mDropShadowColour = KU_DEFAULT_COLOUR;
        mBorderColour     = KU_DEFAULT_COLOUR;
        mGradientColour1  = KU_DEFAULT_COLOUR;
        mGradientColour2  = KU_DEFAULT_COLOUR;

        mpAlternateTextColours = lpAlternateColours;
        miNumAlternateColours  = liNumAlternateColours;
        mfStringWidth = -1.0f;
        mpUtf8String  = 0;
        mbAutosize    = false;
    }

    // X360 0x827EEF58: shrink the font height so the string fits mv2TopLeft..mv2BottomRight. The full
    // fit-search is deferred; this keeps the requested height and points the current-height at the
    // autosized field (as the X360 does). Debug text leaves mbAutosize=false, so this path is inert.
    void TextObject::CalculateAutosizing()
    {
        mfAutosizedFontHeight = mfFontHeight;
        mpfCurrentFontHeight  = &mfAutosizedFontHeight;
    }

    // Faithful port of X360 0x82801998: render through the Im2d buffer, then clear it.
    void TextRenderer::RenderString(Im2dRenderBuffer* lpRenderBuffer, const TextObject& lrTextObject)
    {
        mpIm2dRenderBuffer = lpRenderBuffer;
        mpIm3dRenderBuffer = 0;
        RenderStringInternal(lrTextObject, EImRenderingType_Buffered);
        mpIm2dRenderBuffer = 0;
    }

    namespace
    {
        // renderengine::PrimitiveType for a triangle strip (X360 RenderBufferRenderEnd arg = 6).
        const u32 KU_PRIMITIVE_TRIANGLE_STRIP = 6u;

        // Write one Im2d vertex. PC: the packed RGBA colour is stored directly (the X360 byte-swaps
        // it for big-endian; little-endian PC keeps it as-is -- see rsi_notes.md).
        void lEmitVertex(CgsGraphics::Basic2dColouredTexturedVertex& lrVtx,
                         f32 lfX, f32 lfY, f32 lfU, f32 lfV, CgsGraphics::RGBA lColour)
        {
            lrVtx.mv2Pos.x    = lfX;
            lrVtx.mv2Pos.y    = lfY;
            lrVtx.mv2Tex0UV.x = lfU;
            lrVtx.mv2Tex0UV.y = lfV;
            *reinterpret_cast<u32*>(&lrVtx.mv4Colour) = lColour;
        }
    }

    // Translated from the X360 PPC asm (0x827FF670) per rsi_notes.md. Lays each line of the text out as
    // a run of glyph quads (a triangle strip with degenerate connectors), scaled by the font height,
    // coloured by mTextColour / the alternate-colour table (selected by '^N' codes), and submitted to
    // the Im2d buffer with the font's texture state bound.
    //
    // CORE path implemented: scale/layout setup, the per-line loop, the per-glyph quad emit, and the
    // colour codes. NOTE (asm branches not yet ported): the background/border/drop-shadow/emboss passes
    // and the gradient colour interpolation are guarded features -- added in a follow-on; the common
    // (plain coloured text) path is complete. The line measurer Font::GetStringStartAndEnd and the
    // RenderBuffer* helpers are declared; their bodies are the next pass (so this displays once those land).
    void TextRenderer::RenderStringInternal(const TextObject& lrTextObject, EImRenderingType leType)
    {
        using CgsResource::CgsUtf8;     // these live in CgsResource (this TU is in CgsGraphics)
        using CgsResource::FontChar;

        const CgsResource::Font* lpFont = lrTextObject.mpFont.operator->();
        // (X360 asserts here: font set, string set, font->mpTextureState set, valid UTF-8 string.)

        mauVertexCount[leType] = 0;

        const f32 lfFontHeight = *lrTextObject.mpfCurrentFontHeight;
        const f32 lfGlyphX     = lpFont->mScaleUV.mX * lfFontHeight;                    // X360 f29
        const f32 lfGlyphY     = lpFont->mScaleUV.mY * lfFontHeight;                    // X360 f28
        const f32 lfWidthInEm  = (lrTextObject.mv2BottomRight.mX - lrTextObject.mv2TopLeft.mX) / lfFontHeight;  // f25

        // Alignment factor (X360 table unk_820D408C+0xBC0[meAlignment]): START/LEFT 0, CENTER 0.5, RIGHT 1.
        static const f32 skafAlignFactor[TextObject::E_ALIGNMENT_COUNT] = { 0.0f, 0.0f, 0.5f, 1.0f };
        const u32 luAlign = (lrTextObject.meAlignment >= 0 && lrTextObject.meAlignment < TextObject::E_ALIGNMENT_COUNT)
                            ? static_cast<u32>(lrTextObject.meAlignment)
                            : static_cast<u32>(TextObject::E_ALIGNMENT_LEFT);
        const f32 lfAlignFactor = skafAlignFactor[luAlign];

        f32 lfPenY = lrTextObject.mv2TopLeft.mY;
        s32 liColourIndex = -1;
        const CgsUtf8* lpCursor = lrTextObject.mpUtf8String;

        while (*lpCursor != 0 && lfPenY < lrTextObject.mv2BottomRight.mY)
        {
            // Measure the next line (word-wrapping); GetStringStartAndEnd gives [start,end) + its width.
            const CgsUtf8* lpLineStart = lpCursor;
            const CgsUtf8* lpLineEnd   = lpCursor;
            const f32 lfLineWidth = lpFont->GetStringStartAndEnd(lpCursor, lfWidthInEm, &lpLineStart, &lpLineEnd,
                                                                 lrTextObject.mbWordWrap != 0);

            f32 lfPenX = lrTextObject.mv2TopLeft.mX + (lfAlignFactor * (lfWidthInEm - lfLineWidth)) * lfFontHeight;

            // Reserve the strip: 6 verts per non-control char (a slight over-reserve for '^' codes, which
            // is harmless -- mauVertexCount tracks the actual emitted count for submission).
            u32 luGlyphCount = 0;
            for (const CgsUtf8* lpScan = lpLineStart; lpScan < lpLineEnd;
                 lpScan = CgsUnicode::IncrementUtf8Pointer(lpScan))
            {
                if (*lpScan != '^')
                    ++luGlyphCount;
            }

            Im2dVertex* const lpVtxBase = RenderBufferRenderStart(6u * luGlyphCount, leType);
            Im2dVertex* lpVtx = lpVtxBase;
            mauVertexCount[leType] = 0;
            RenderBufferSetTextureState(lpFont->mpTextureState, leType);

            for (const CgsUtf8* lpChar = lpLineStart; lpChar < lpLineEnd; )
            {
                // Colour codes: "^^" ends a colour run (literal caret); "^<digits>" selects an alternate colour.
                if (*lpChar == '^')
                {
                    if (liColourIndex > -1 && lpChar[1] == '^')
                    {
                        liColourIndex = -1;
                        lpChar += 2;
                        continue;
                    }
                    if (lpChar[1] >= '0' && lpChar[1] <= '9')
                    {
                        s32 liNumber = 0;
                        s32 liDigits = 0;
                        for (const CgsUtf8* lpDigit = lpChar + 1; lpDigit < lpLineEnd && *lpDigit != '^';
                             lpDigit = CgsUnicode::IncrementUtf8Pointer(lpDigit))
                        {
                            liNumber = liNumber * 10 + (*lpDigit - '0');
                            ++liDigits;
                        }
                        if (liDigits > 0)
                        {
                            liColourIndex = (liNumber >= 0 && liNumber < lrTextObject.miNumAlternateColours) ? liNumber : -1;
                            lpChar += liDigits + 2;   // skip "^<digits>^"
                            continue;
                        }
                    }
                }

                const FontChar* lpFc = lpFont->GetFontChar(lpChar);

                if (lpFc->mbIsRenderable != 0 && lpVtx != 0)
                {
                    const f32 lfLeftU   = lpFc->mTopLeftUV.mX;
                    const f32 lfTopV    = lpFc->mTopLeftUV.mY;
                    const f32 lfRightU  = lfLeftU + lpFc->mDimensionsUV.mX;
                    const f32 lfBottomV = lfTopV  + lpFc->mDimensionsUV.mY;
                    const f32 lfLeftX   = lfPenX + lpFc->mStart.mX * lfGlyphX;
                    const f32 lfTopY    = lfPenY + lpFc->mStart.mY * lfGlyphY;
                    const f32 lfRightX  = lfLeftX + lpFc->mDimensionsUV.mX * lfGlyphX;
                    const f32 lfBottomY = lfTopY  + lpFc->mDimensionsUV.mY * lfGlyphY;

                    const CgsGraphics::RGBA lColour =
                        (liColourIndex >= 0 && lrTextObject.mpAlternateTextColours != 0)
                            ? lrTextObject.mpAlternateTextColours[liColourIndex]
                            : lrTextObject.mTextColour;

                    // 6 verts: BL, BL, BR, TL, TR, TR (triangle strip; the dup first/last are connectors).
                    lEmitVertex(lpVtx[0], lfLeftX,  lfBottomY, lfLeftU,  lfBottomV, lColour);
                    lEmitVertex(lpVtx[1], lfLeftX,  lfBottomY, lfLeftU,  lfBottomV, lColour);
                    lEmitVertex(lpVtx[2], lfRightX, lfBottomY, lfRightU, lfBottomV, lColour);
                    lEmitVertex(lpVtx[3], lfLeftX,  lfTopY,    lfLeftU,  lfTopV,    lColour);
                    lEmitVertex(lpVtx[4], lfRightX, lfTopY,    lfRightU, lfTopV,    lColour);
                    lEmitVertex(lpVtx[5], lfRightX, lfTopY,    lfRightU, lfTopV,    lColour);
                    lpVtx += 6;
                    mauVertexCount[leType] += 6;
                }

                // Advance the pen by the glyph's advance, scaled the same way as the glyph WIDTH
                // (mScaleUV.x * fontHeight = lfGlyphX) and the char-spacing multiplier. X360 0x82800718:
                // penX += (mfAdvance * mfCharSpacingMultiplier) * f29, where f29 == lfGlyphX. (Using
                // lfScale here, the 0.73 line metric, made the advance far too small -> glyphs stacked.)
                lfPenX += lpFc->mfAdvance * lrTextObject.mfCharSpacingMultiplier * lfGlyphX;
                lpChar = CgsUnicode::IncrementUtf8Pointer(lpChar);
            }

            // Submit the buffer the glyphs were written into (the RenderStart return). The X360 2D path
            // does NOT stash this in mapaVertices (that member is only the 3D temp buffer), so passing
            // mapaVertices[leType] here submitted a null/empty buffer -> invisible text.
            RenderBufferRenderEnd(KU_PRIMITIVE_TRIANGLE_STRIP, lpVtxBase, mauVertexCount[leType], leType);

            // Advance to the next line (ensure forward progress, then skip a trailing newline).
            lpCursor = (lpLineEnd > lpCursor) ? lpLineEnd : CgsUnicode::IncrementUtf8Pointer(lpCursor);
            if (*lpCursor == '\n' || *lpCursor == '\r')
                lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);
            lfPenY += lfFontHeight;
        }
    }

    // --- Render-buffer helpers (faithful X360 ports). Each dispatches to whichever immediate buffer
    //     the TextRenderer holds: mpIm2dRenderBuffer for 2D text (the debug-text path), or
    //     mpIm3dRenderBuffer for 3D text. The X360 reaches the buffer methods at +4 (the render-buffer
    //     subobject), exactly one buffer is set at a time, and leType must be Buffered (asserts elided
    //     -- they would pull in CgsDev::Assert). The 3D branches are a follow-on: the 2D debug-text
    //     path never sets mpIm3dRenderBuffer, and the 3D vertex widen needs the 3D buffer/vertex. ---

    // X360 0x827F7D80: reserve luVertexCount vertices and return the write pointer.
    TextRenderer::Im2dVertex* TextRenderer::RenderBufferRenderStart(u32 luVertexCount, EImRenderingType leType)
    {
        if (mpIm2dRenderBuffer != 0)
            return mpIm2dRenderBuffer->RenderStart(luVertexCount);

        if (mpIm3dRenderBuffer != 0)
        {
            // 3D: hand back the temp-vertex scratch (filled as 2D, widened to 3D in RenderBufferRenderEnd).
            mabTempVerticesInUse[leType] = true;
            return maaTempVertices[leType];
        }
        return 0;   // no render buffer set
    }

    // X360 0x827FAC28: bind the texture state (the font atlas) for the next submission.
    void TextRenderer::RenderBufferSetTextureState(const renderengine::TextureState* lpTextureState,
                                                   EImRenderingType /*leType*/)
    {
        if (mpIm2dRenderBuffer != 0)
            mpIm2dRenderBuffer->SetState(lpTextureState);
        // else if (mpIm3dRenderBuffer) -> 3D buffer SetState (follow-on, 3D text path)
    }

    // X360 0x827FA988: submit luVertexCount vertices as one primitive of the given topology.
    void TextRenderer::RenderBufferRenderEnd(u32 luPrimitiveType, const Im2dVertex* lpVertices,
                                             u32 luVertexCount, EImRenderingType leType)
    {
        if (mpIm2dRenderBuffer != 0)
        {
            mpIm2dRenderBuffer->RenderEnd(static_cast<renderengine::PrimitiveType>(luPrimitiveType),
                                          lpVertices, luVertexCount);
            return;
        }
        if (mpIm3dRenderBuffer != 0)
        {
            // 3D text path: widen each 2D vertex (pos.xy/colour/uv) to a 3D vertex (pos.xyz with z=0,
            // colour, uv) and submit on the 3D buffer. [Follow-on -- VPU widen at X360 0x827FA9F8; the
            // debug-2D path never reaches here.]
            mabTempVerticesInUse[leType] = false;
        }
    }
}
