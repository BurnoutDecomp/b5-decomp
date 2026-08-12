#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // IncrementUtf8Pointer
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (the buffered Apt string path)

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
        // X360 default font handle (dword_83011A50/A54): the invalid/null SafeResourceHandle (no resource
        // memory -> IsNull() true -> HasResourceFont() false until SetDebugFont points it at a pool entry).
        mpFont.mpResourceMemory = 0;
        mpFont.mpSourceEntry    = 0;

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

    // Faithful port of X360 0x827EEF58: pick the autosized font height so the string fits the box
    // width. If the box is already wide enough (boxWidth >= mfStringWidth * mfFontHeight) the height
    // is kept; otherwise it is scaled down and floored at 15.0. Only mfAutosizedFontHeight (+0x0C) is
    // written -- the asm does NOT touch mpfCurrentFontHeight (+0x10).
    void TextObject::CalculateAutosizing()
    {
        const f32 lfBoxWidth = mv2BottomRight.mX - mv2TopLeft.mX;   // f12
        const f32 lfStringSpan = mfStringWidth * mfFontHeight;      // f11

        if (lfBoxWidth >= lfStringSpan)
        {
            mfAutosizedFontHeight = mfFontHeight;
            return;
        }

        const f32 lfScaled = mfFontHeight / ((lfStringSpan + 5.0f) / lfBoxWidth);
        mfAutosizedFontHeight = (15.0f >= lfScaled) ? 15.0f : lfScaled;   // fsel: max(15.0, lfScaled)
    }

    // Faithful port of X360 0x827F7C10. Counts the wrapped lines of mpUtf8String laid out in
    // this object's box and reports the start of line luStartLine (1-based) via *lppLine. When
    // the object is not both multiline AND word-wrapped (mbWordWrap / mbMultiLine), it is a
    // single line (returns 1). Otherwise it walks the string line-by-line with
    // Font::GetStringStartAndEnd (the same line measurer RenderStringInternal uses), counting a
    // line per embedded newline (when multiline) and a final line per wrapped segment (when
    // word-wrapped), recording the requested line's start pointer along the way.
    u32 TextObject::GetNumLinesAndStartLine(u32 luStartLine, const CgsResource::CgsUtf8** lppLine) const
    {
        using CgsResource::CgsUtf8;

        // mbWordWrap (+0x34) and mbMultiLine (+0x30) must both be set to do line counting.
        if (mbWordWrap == 0 || mbMultiLine == 0)
            return 1;

        const f32 lfWidthInEm =
            (mv2BottomRight.mX - mv2TopLeft.mX) / *mpfCurrentFontHeight;

        const CgsUtf8* lpCursor = mpUtf8String;
        u32 luNumLines = 1;

        CGS_ASSERT(lppLine != 0, "lppLine != NULL");
        *lppLine = lpCursor;

        const CgsResource::Font* lpFont = mpFont.operator->();

        while (*lpCursor != 0)
        {
            bool lbSawNewline = false;
            const CgsUtf8* lpLineStart = lpCursor;
            const CgsUtf8* lpLineEnd   = lpCursor;
            lpFont->GetStringStartAndEnd(lpCursor, lfWidthInEm, &lpLineStart, &lpLineEnd, true);
            lpCursor = lpLineEnd;

            // Multiline: consume the run of CR/LF after the measured line, counting a new line
            // for each LF and recording the requested line's start when it is reached.
            if (mbMultiLine != 0)
            {
                while (true)
                {
                    const CgsUtf8 lcCh = *lpCursor;
                    if (lcCh != 13 && lcCh != 10)
                        break;
                    if (lcCh == 10)
                    {
                        ++luNumLines;
                        lbSawNewline = true;
                        if (luStartLine == luNumLines)
                            *lppLine = lpCursor + 1;
                    }
                    ++lpCursor;
                }
            }

            // Word-wrap: a wrapped (non-newline-terminated) segment also begins a new line.
            if (mbWordWrap != 0)
            {
                if (!lbSawNewline && luStartLine == ++luNumLines)
                    *lppLine = lpCursor;
            }
        }

        return luNumLines;
    }

    // Construct @0x827E9D98 -- zero-init the render-buffer pointers + the single per-type vertex
    // slot state (console: `stw 0` at +0/+4/+8/+0xC, `stb 0` at +0x10). The temp-vertex scratch
    // (maaTempVertices) is deliberately left uninitialised, exactly as the console leaves it.
    void TextRenderer::Construct()
    {
        mpIm2dRenderBuffer      = 0;   // +0x00
        mpIm3dRenderBuffer      = 0;   // +0x04
        mauVertexCount[0]       = 0;   // +0x08
        mapaVertices[0]         = 0;   // +0x0C
        mabTempVerticesInUse[0] = false;   // +0x10
    }

    // Faithful port of X360 0x82801998: render through the Im2d buffer, then clear it.
    void TextRenderer::RenderString(Im2dRenderBuffer* lpRenderBuffer, const TextObject& lrTextObject)
    {
        mpIm2dRenderBuffer     = lpRenderBuffer;
        mpIm3dRenderBuffer     = 0;
        mpBufferedRenderBuffer = 0;   // (PC fold: guarantee the immediate path regardless of init)
        RenderStringInternal(lrTextObject, EImRenderingType_Buffered);
        mpIm2dRenderBuffer = 0;
    }

    // FLAG (PC fold; see the header note): the Apt string path -- the glyphs ride the SAME
    // dispatched command stream as the Apt shapes (per-batch SET_TRANSFORM + walk order), which
    // is exactly what the console's buffered Im2dRenderBuffer::RenderStart/RenderEnd did. Same
    // layout/glyph path as RenderString; only the RenderBuffer* helpers' target differs.
    void TextRenderer::RenderStringBuffered(ImRenderBuffer<Im2dVertex>* lpBufferedBuffer,
                                            const TextObject& lrTextObject)
    {
        mpIm2dRenderBuffer     = 0;
        mpIm3dRenderBuffer     = 0;
        mpBufferedRenderBuffer = lpBufferedBuffer;
        RenderStringInternal(lrTextObject, EImRenderingType_Buffered);
        mpBufferedRenderBuffer = 0;
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

        // X360 0x827FF9C0 / 0x827FF9CC: the per-line MEASURE is only reached when the object is
        // multiline or word-wrapped. A plain single-line field takes the fast path below instead --
        // whole string, no measure, no wrap. That gate is what stops an over-long single line from
        // breaking at a space: the apt dynamic-text fields routinely author a box NARROWER than the
        // string (e.g. BrnIntro's $DMV_INTRO_1 "DEPARTMENT OF MOTOR VEHICLES", bMultiLine=0 /
        // bWordWrap=0 in a 286.8-wide box), and they are meant to overflow it, not wrap.
        const bool lbLineWalk = (lrTextObject.mbMultiLine != 0 || lrTextObject.mbWordWrap != 0);

        while (*lpCursor != 0 && lfPenY < lrTextObject.mv2BottomRight.mY)
        {
            const CgsUtf8* lpLineStart = lpCursor;
            const CgsUtf8* lpLineEnd   = lpCursor;
            f32 lfLineWidth;
            if (lbLineWalk)
            {
                // Measure the next line; GetStringStartAndEnd gives [start,end) + its width. The
                // console passes word-wrap TRUE unconditionally here (`li r8, 1` @0x827FFB18) --
                // this call is only reached for a multiline / word-wrapped object, so the flag is
                // already known set.
                lfLineWidth = lpFont->GetStringStartAndEnd(lpCursor, lfWidthInEm, &lpLineStart, &lpLineEnd, true);
            }
            else
            {
                // Single-line fast path (X360 0x827FF9D8): the line IS the whole remaining string
                // (CgsUnicode::ByteLength), and its width is the one Prepare already measured
                // (mfStringWidth, +0x70) -- no line measurer, so no wrap is possible.
                lpLineEnd   = lpCursor + CgsUnicode::ByteLength(lpCursor);
                lfLineWidth = lrTextObject.mfStringWidth;
            }

            // Right-aligned text aligns against the SPACED width (X360 0x827FFB28 / 0x827FF9F8: the
            // only alignment for which the console folds mfCharSpacingMultiplier into the measured
            // line width -- the measurers themselves are spacing-unaware).
            if (lrTextObject.meAlignment == TextObject::E_ALIGNMENT_RIGHT)
                lfLineWidth *= lrTextObject.mfCharSpacingMultiplier;

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
        // FLAG (PC fold): the buffered Apt path reserves from the command buffer's vertex
        // stream (the console Im2dRenderBuffer::RenderStart @0x57E0A0 == AllocVertices).
        if (mpBufferedRenderBuffer != 0)
            return mpBufferedRenderBuffer->RenderStart(luVertexCount);

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

    // X360 0x82800798. Render the text object's string with a per-glyph vertical fade (the
    // RenderStringInternal layout/glyph/colour-code path with the vertex alpha scaled by the
    // line's Y within the fade band). NOTE (RenderStringInternal precedent): the CORE path is
    // implemented; the guarded background/border/drop-shadow/emboss/gradient passes are a
    // follow-on (the credits caller never enables them). Shares the line measurer + RenderBuffer*
    // helpers with RenderStringInternal (defined above).
    void TextRenderer::RenderStringFadingY(Im2dRenderBuffer* lpRenderBuffer, const TextObject& lrTextObject,
                                           f32 lfFadeTopY, f32 lfFadeTopExtent,
                                           f32 lfFadeBottomStart, f32 lfFadeBottomEnd)
    {
        using CgsResource::CgsUtf8;
        using CgsResource::FontChar;

        mpIm2dRenderBuffer     = lpRenderBuffer;
        mpIm3dRenderBuffer     = 0;
        mpBufferedRenderBuffer = 0;   // (PC fold: guarantee the immediate path regardless of init)

        const CgsResource::Font* lpFont = lrTextObject.mpFont.operator->();

        mauVertexCount[EImRenderingType_Buffered] = 0;

        const f32 lfFontHeight = *lrTextObject.mpfCurrentFontHeight;
        const f32 lfGlyphX     = lpFont->mScaleUV.mX * lfFontHeight;
        const f32 lfGlyphY     = lpFont->mScaleUV.mY * lfFontHeight;
        const f32 lfWidthInEm  = (lrTextObject.mv2BottomRight.mX - lrTextObject.mv2TopLeft.mX) / lfFontHeight;

        static const f32 skafAlignFactor[TextObject::E_ALIGNMENT_COUNT] = { 0.0f, 0.0f, 0.5f, 1.0f };
        const u32 luAlign = (lrTextObject.meAlignment >= 0 && lrTextObject.meAlignment < TextObject::E_ALIGNMENT_COUNT)
                            ? static_cast<u32>(lrTextObject.meAlignment)
                            : static_cast<u32>(TextObject::E_ALIGNMENT_LEFT);
        const f32 lfAlignFactor = skafAlignFactor[luAlign];

        const f32 lfTopRampLen    = lfFadeTopExtent;
        const f32 lfBottomRampLen = (lfFadeBottomEnd - lfFadeBottomStart);

        f32 lfPenY = lrTextObject.mv2TopLeft.mY;
        s32 liColourIndex = -1;
        const CgsUtf8* lpCursor = lrTextObject.mpUtf8String;

        // Same single-line gate as RenderStringInternal (X360 0x827FF9C0/0x827FF9CC).
        const bool lbLineWalk = (lrTextObject.mbMultiLine != 0 || lrTextObject.mbWordWrap != 0);

        while (*lpCursor != 0 && lfPenY < lrTextObject.mv2BottomRight.mY)
        {
            const CgsUtf8* lpLineStart = lpCursor;
            const CgsUtf8* lpLineEnd   = lpCursor;
            f32 lfLineWidth;
            if (lbLineWalk)
            {
                lfLineWidth = lpFont->GetStringStartAndEnd(lpCursor, lfWidthInEm, &lpLineStart, &lpLineEnd, true);
            }
            else
            {
                lpLineEnd   = lpCursor + CgsUnicode::ByteLength(lpCursor);
                lfLineWidth = lrTextObject.mfStringWidth;
            }

            if (lrTextObject.meAlignment == TextObject::E_ALIGNMENT_RIGHT)
                lfLineWidth *= lrTextObject.mfCharSpacingMultiplier;

            f32 lfPenX = lrTextObject.mv2TopLeft.mX + (lfAlignFactor * (lfWidthInEm - lfLineWidth)) * lfFontHeight;

            u32 luGlyphCount = 0;
            for (const CgsUtf8* lpScan = lpLineStart; lpScan < lpLineEnd;
                 lpScan = CgsUnicode::IncrementUtf8Pointer(lpScan))
            {
                if (*lpScan != '^')
                    ++luGlyphCount;
            }

            Im2dVertex* const lpVtxBase = RenderBufferRenderStart(6u * luGlyphCount, EImRenderingType_Buffered);
            Im2dVertex* lpVtx = lpVtxBase;
            mauVertexCount[EImRenderingType_Buffered] = 0;
            RenderBufferSetTextureState(lpFont->mpTextureState, EImRenderingType_Buffered);

            f32 lfFade = 1.0f;
            if (lfTopRampLen > 0.0f && lfPenY < (lfFadeTopY + lfTopRampLen))
            {
                lfFade = (lfPenY - lfFadeTopY) / lfTopRampLen;
            }
            else if (lfBottomRampLen > 0.0f && lfPenY > lfFadeBottomStart)
            {
                lfFade = 1.0f - (lfPenY - lfFadeBottomStart) / lfBottomRampLen;
            }
            if (lfFade < 0.0f) lfFade = 0.0f;
            if (lfFade > 1.0f) lfFade = 1.0f;
            const u32 luFadeAlpha = static_cast<u32>(lfFade * 255.0f + 0.5f) & 0xFFu;

            for (const CgsUtf8* lpChar = lpLineStart; lpChar < lpLineEnd; )
            {
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
                            lpChar += liDigits + 2;
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

                    const CgsGraphics::RGBA lBase =
                        (liColourIndex >= 0 && lrTextObject.mpAlternateTextColours != 0)
                            ? lrTextObject.mpAlternateTextColours[liColourIndex]
                            : lrTextObject.mTextColour;

                    const u32 luBaseAlpha = (lBase >> 24) & 0xFFu;
                    const u32 luAlpha = (luBaseAlpha * luFadeAlpha) / 255u;
                    const CgsGraphics::RGBA lColour = (lBase & 0x00FFFFFFu) | (luAlpha << 24);

                    lEmitVertex(lpVtx[0], lfLeftX,  lfBottomY, lfLeftU,  lfBottomV, lColour);
                    lEmitVertex(lpVtx[1], lfLeftX,  lfBottomY, lfLeftU,  lfBottomV, lColour);
                    lEmitVertex(lpVtx[2], lfRightX, lfBottomY, lfRightU, lfBottomV, lColour);
                    lEmitVertex(lpVtx[3], lfLeftX,  lfTopY,    lfLeftU,  lfTopV,    lColour);
                    lEmitVertex(lpVtx[4], lfRightX, lfTopY,    lfRightU, lfTopV,    lColour);
                    lEmitVertex(lpVtx[5], lfRightX, lfTopY,    lfRightU, lfTopV,    lColour);
                    lpVtx += 6;
                    mauVertexCount[EImRenderingType_Buffered] += 6;
                }

                lfPenX += lpFc->mfAdvance * lrTextObject.mfCharSpacingMultiplier * lfGlyphX;
                lpChar = CgsUnicode::IncrementUtf8Pointer(lpChar);
            }

            RenderBufferRenderEnd(KU_PRIMITIVE_TRIANGLE_STRIP, lpVtxBase,
                                  mauVertexCount[EImRenderingType_Buffered], EImRenderingType_Buffered);

            lpCursor = (lpLineEnd > lpCursor) ? lpLineEnd : CgsUnicode::IncrementUtf8Pointer(lpCursor);
            if (*lpCursor == '\n' || *lpCursor == '\r')
                lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);
            lfPenY += lfFontHeight;
        }

        mpIm2dRenderBuffer = 0;
    }

    // X360 0x827FAC28: bind the texture state (the font atlas) for the next submission.
    void TextRenderer::RenderBufferSetTextureState(const renderengine::TextureState* lpTextureState,
                                                   EImRenderingType /*leType*/)
    {
        // FLAG (PC fold): the buffered Apt path appends a SET_STATE_TEXTURE command
        // (Dispatch binds the state's raster -- the font atlas -- with modulate stages).
        if (mpBufferedRenderBuffer != 0)
        {
            mpBufferedRenderBuffer->SetTextureState(lpTextureState);
            return;
        }

        if (mpIm2dRenderBuffer != 0)
            mpIm2dRenderBuffer->SetState(lpTextureState);
        // else if (mpIm3dRenderBuffer) -> 3D buffer SetState (follow-on, 3D text path)
    }

    // X360 0x827FA988: submit luVertexCount vertices as one primitive of the given topology.
    void TextRenderer::RenderBufferRenderEnd(u32 luPrimitiveType, const Im2dVertex* lpVertices,
                                             u32 luVertexCount, EImRenderingType leType)
    {
        // FLAG (PC fold): the buffered Apt path appends the RENDER_PRIMITIVES command over the
        // RenderStart-reserved run (the console Im2dRenderBuffer::RenderEnd @0x57E5DC).
        if (mpBufferedRenderBuffer != 0)
        {
            mpBufferedRenderBuffer->RenderEnd(static_cast<renderengine::PrimitiveType>(luPrimitiveType),
                                              lpVertices, luVertexCount);
            return;
        }

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
