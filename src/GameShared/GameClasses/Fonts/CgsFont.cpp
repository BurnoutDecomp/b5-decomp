#include "GameShared/GameClasses/Fonts/CgsFont.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // ConvertUtf8CharToUtf16Char
#include "pc/gcm/renderengine/renderstates.h"          // renderengine::TextureState (CreateTextureState)
#include "rw/rwcore_structs.h"                          // rw::IResourceAllocator

// CgsResource::Font glyph lookup. Faithful ports of the X360 ARTIST bodies:
//   FindFontChar          0x827E6D40  (hash + binary search over the bucket)
//   GetFontChar(CgsUtf8*) 0x827EC588  (UTF-8 -> UTF-16, then FindFontChar + fallback)
// The debug-text renderer (CgsGraphics::TextRenderer) drives these per character.

namespace CgsResource
{
    // Faithful port of X360 0x827E6D40. The glyph ids (mpaFontCharIds) are sorted within each of
    // the 128 hash buckets; mauHashOffsets[hash]..mauHashOffsets[hash+1] bound this char's bucket.
    // Returns the matching FontChar, or null if the char is not in the font.
    const FontChar* Font::FindFontChar(CgsUtf16 lu16Char) const
    {
        const u32 luHash = lu16Char & KU_HASH_MASK;   // char & 127
        // X360 asserts (debug-only bounds checks, CgsFont.h):
        //   luHash + 1 < KU_HASH_TABLE_SIZE        (line 280)
        //   luEndIndex >= luBeginIndex             (line 284)
        //   luEndIndex <= muNumChars               (line 285)
        u32 luBeginIndex = mauHashOffsets[luHash];
        u32 luEndIndex   = mauHashOffsets[luHash + 1];

        while (luBeginIndex < luEndIndex)
        {
            const u32      luMid     = luBeginIndex + ((luEndIndex - luBeginIndex) >> 1);
            const CgsUtf16 lu16MidId = mpaFontCharIds[luMid];
            if (lu16MidId == lu16Char)
                return &mpaFontChars[luMid];
            if (lu16MidId > lu16Char)
                luEndIndex = luMid;
            else
                luBeginIndex = luMid + 1;
        }
        return 0;
    }

    // The UTF-16 overload (CgsFont.h:116). Its X360 body is inlined/folded; the fallback mirrors
    // the UTF-8 overload (0x827EC588): try the char, then '-', then the first glyph as a last
    // resort so the renderer always has something to draw.
    const FontChar* Font::GetFontChar(CgsUtf16 lu16Char) const
    {
        const FontChar* lpFontChar = FindFontChar(lu16Char);
        if (lpFontChar == 0)
        {
            lpFontChar = FindFontChar(static_cast<CgsUtf16>(KUTF16_DEFAULT_CHARACTER));  // '-'
            if (lpFontChar == 0)
                return mpaFontChars;  // X360: *(font + 0x20) -> &mpaFontChars[0]
        }
        return lpFontChar;
    }

    // Faithful port of X360 0x827EC588 (the UTF-8 overload the text renderer calls per character).
    const FontChar* Font::GetFontChar(const CgsUtf8* lpUtf8Char) const
    {
        // (X360 asserts lpUtf8Char != NULL, CgsFont.h:206.)
        const CgsUtf16 lu16Char = CgsUnicode::ConvertUtf8CharToUtf16Char(lpUtf8Char);
        const FontChar* lpFontChar = FindFontChar(lu16Char);
        if (lpFontChar == 0)
        {
            lpFontChar = FindFontChar(static_cast<CgsUtf16>(KUTF16_DEFAULT_CHARACTER));  // '-'
            if (lpFontChar == 0)
                return mpaFontChars;  // X360: *(font + 0x20) -> &mpaFontChars[0]
        }
        return lpFontChar;
    }

    // Faithful port of X360 0x82834F20. Walks one line of the string (stops at '\n'), accumulating
    // each glyph's mfAdvance into the running width, then adds the last glyph's actual extent
    // (mStart.x + mDimensionsUV.x) and scales by mScaleUV.x. '\r' is skipped; Burnout '^' colour
    // codes are parsed and excluded: "^<digit>" begins a code that runs to the next '^' (or end),
    // and "^^" while a code is active is the literal-caret escape. (Empty string -> 0.)
    float Font::GetStringWidth(const CgsUtf8* lpUtf8String) const
    {
        // (X360 asserts lpUtf8String != NULL, CgsFont.cpp:59.)
        if (*lpUtf8String == 0)
            return 0.0f;  // X360 flt_82001CC0

        const FontChar* lpFontChar = GetFontChar(lpUtf8String);
        const CgsUtf8*  lpCursor   = CgsUnicode::IncrementUtf8Pointer(lpUtf8String);
        float           lfWidth    = lpFontChar->mStart.mX;
        bool            lbColourActive = false;

        while (*lpCursor != 0)
        {
            const u8 lu8Char = *lpCursor;
            if (lu8Char == '\n')
                break;

            if (lu8Char == '^')
            {
                if (lpCursor[1] == '^' && lbColourActive)
                {
                    // "^^" while a colour code is active: the literal-caret escape -- consume both.
                    lbColourActive = false;
                    lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);
                    lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);
                    continue;
                }
                const u8 lu8Next = lpCursor[1];
                if (lu8Next >= '0' && lu8Next <= '9')
                {
                    // "^<digit>": a colour code -- skip to the next '^' (or the end of the string).
                    const CgsUtf8* lpScan = CgsUnicode::IncrementUtf8Pointer(lpCursor);
                    while (*lpScan != 0 && *lpScan != '^')
                        lpScan = CgsUnicode::IncrementUtf8Pointer(lpScan);
                    lpCursor = CgsUnicode::IncrementUtf8Pointer(lpScan);
                    lbColourActive = true;
                    continue;
                }
                // otherwise a literal '^' -- fall through and treat it as a normal glyph.
            }

            if (lu8Char != '\r')
            {
                lfWidth += lpFontChar->mfAdvance;
                lpFontChar = GetFontChar(lpCursor);
            }
            lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);
        }

        return (lpFontChar->mStart.mX + lpFontChar->mDimensionsUV.mX + lfWidth) * mScaleUV.mX;
    }

    // Faithful port of X360 0x828352F0. Finds the [start,end) of the next line of lpString that fits
    // lfMaxWidthInEm and returns that line's rendered width (in em). Skips leading whitespace, ends a
    // line on '\n'/'\r', and -- when lbWordWrap -- wraps at the last space (or hard-breaks an over-long
    // word). Burnout '^<digits>...^' colour codes are skipped. Width is accumulated in pre-scale units
    // (a running sum of glyph advances) and multiplied by mScaleUV.mX on return, like GetStringWidth.
    // [The IDA prototype mistypes the return as float* (the r3 pointer); the value callers use is the
    // width returned in f1 -- see RenderStringInternal. Reconstructed from the clean Hex-Rays output +
    // the asm for the per-exit width formula.]
    float Font::GetStringStartAndEnd(const CgsUtf8* lpString, float lfMaxWidthInEm,
                                     const CgsUtf8** lppLineStart, const CgsUtf8** lppLineEnd,
                                     bool lbWordWrap) const
    {
        const f32 lfMax = lfMaxWidthInEm / mScaleUV.mX;   // f30: the width limit in pre-scale units
        f32  lfCommitted = 0.0f;                          // f29: width at the last valid wrap candidate
        bool lbSpaceSeen = false;                         // r26

        const CgsUtf8* lpCursor = lpString;               // r31

        // Skip leading whitespace; an all-whitespace remainder is a zero-width (empty) line.
        while (*lpCursor == ' ' || *lpCursor == '\n' || *lpCursor == '\r')
        {
            lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);
            if (*lpCursor == 0)
            {
                *lppLineStart = lpCursor;
                *lppLineEnd   = lpCursor;
                return 0.0f;   // X360 flt_82001CC0
            }
        }

        *lppLineStart = lpCursor;
        if (*lpCursor == 0)
        {
            *lppLineEnd = lpCursor;
            return 0.0f;
        }

        const FontChar* lpGlyph = GetFontChar(lpCursor);   // r29
        const CgsUtf8*  lpLastSpace = 0;                    // r28: last space (the wrap point)
        f32 lfWidth = lpGlyph->mStart.mX;                  // f31: running advance accumulator

        // End-of-line resolution (asm 0x828354D0): the loop falls here with lpCursor at the line's end
        // (a NUL, or a space that overran with no earlier space). Mirrors the asm exits exactly.
        struct Tail
        {
            const Font* lpFont; const FontChar* lpGlyph; const CgsUtf8* lpCursor;
            const CgsUtf8* lpLastSpace; f32 lfWidth; f32 lfCommitted; f32 lfMax; bool lbWordWrap;
            const CgsUtf8** lppLineStart; const CgsUtf8** lppLineEnd;

            f32 Resolve()
            {
                const f32 lfScaleX = lpFont->mScaleUV.mX;
                // 0x828354D8: no wrap candidate and no word-wrap -> the whole remainder is one line.
                if (lpLastSpace == 0 && !lbWordWrap)
                {
                    *lppLineEnd = lpCursor;                                                   // 0x82835638
                    return (lpGlyph->mDimensionsUV.mX + lpGlyph->mStart.mX + lfWidth) * lfScaleX;
                }
                // 0x828354E4 (LABEL_29): does the line (incl. the current glyph's ink) still fit?
                const f32 lfWithGlyph = lpGlyph->mStart.mX + lpGlyph->mDimensionsUV.mX + lfWidth;
                if (lfWithGlyph < lfMax)
                {
                    *lppLineEnd = lpCursor;                                                   // 0x82835638
                    return (lpGlyph->mDimensionsUV.mX + lpGlyph->mStart.mX + lfWidth) * lfScaleX;
                }
                if (lpLastSpace != 0)
                {
                    *lppLineEnd = lpLastSpace;                                                // 0x82835504
                    return lfCommitted * lfScaleX;
                }
                // 0x828355A4: a single over-long word (word-wrap is true here). Back up to a char start.
                if (*lpCursor != 0)
                {
                    do { --lpCursor; } while ((*lpCursor & 0xC0) == 0x80);                    // 0x828355BC
                    if (*lppLineStart != lpCursor)
                    {
                        *lppLineEnd = CgsUnicode::IncrementUtf8Pointer(lpCursor);             // 0x828355DC
                        return lfCommitted * lfScaleX;
                    }
                    // backed up to the line start -> fall to the one-more-char-back exit.
                }
                do { --lpCursor; } while ((*lpCursor & 0xC0) == 0x80);                        // 0x82835604
                *lppLineEnd = lpCursor;
                return lfCommitted * lfScaleX;
            }
        };

        while (*lpCursor != 0)
        {
            // "^<digit>...^" colour code: skip it (to the closing '^' or the end of the string).
            if (*lpCursor == '^' && lpCursor[1] >= '0' && lpCursor[1] <= '9')
            {
                const CgsUtf8* lpScan = CgsUnicode::IncrementUtf8Pointer(lpCursor);
                while (*lpScan != 0 && *lpScan != '^')
                    lpScan = CgsUnicode::IncrementUtf8Pointer(lpScan);
                lpCursor = CgsUnicode::IncrementUtf8Pointer(lpScan);
                if (*lpCursor == 0)
                    break;   // -> end-of-line resolution
            }

            const u8 lu8Char = *lpCursor;

            if (lu8Char == '\n' || lu8Char == '\r')   // 0x82835584
            {
                // Newline ends the line -- unless it overran and we can wrap at the last space.
                if (lfWidth >= lfMax && lbSpaceSeen)
                {
                    *lppLineEnd = lpLastSpace;
                    return lfCommitted * mScaleUV.mX;
                }
                *lppLineEnd = lpCursor;
                return lfWidth * mScaleUV.mX;
            }

            if (lu8Char == ' ')   // 0x82835424
            {
                lbSpaceSeen = true;
                const f32 lfWithGlyph = lpGlyph->mStart.mX + lpGlyph->mDimensionsUV.mX + lfWidth;
                if (lfWithGlyph >= lfMax)   // 0x82835524: this space takes us over the limit
                {
                    if (lpLastSpace != 0)
                    {
                        *lppLineEnd = lpLastSpace;
                        return lfCommitted * mScaleUV.mX;
                    }
                    Tail lTail = { this, lpGlyph, lpCursor, lpLastSpace, lfWidth, lfCommitted,
                                   lfMax, lbWordWrap, lppLineStart, lppLineEnd };
                    return lTail.Resolve();   // -> 0x828354D8
                }
                lpLastSpace = lpCursor;
                lfCommitted = lfWithGlyph;
            }
            else if (lbWordWrap && !lbSpaceSeen)   // 0x82835468: track the committed width pre-space
            {
                const f32 lfWithGlyph = lpGlyph->mStart.mX + lpGlyph->mDimensionsUV.mX + lfWidth;
                if (lfWithGlyph < lfMax)
                    lfCommitted = lfWithGlyph;
            }

            lfWidth += lpGlyph->mfAdvance;     // 0x82835490
            lpGlyph  = GetFontChar(lpCursor);  // 0x82835494

            // Word-wrap with no space and we've overrun: hard-break before the current glyph.
            if (lbWordWrap && !lbSpaceSeen && lfWidth >= lfMax)   // 0x8283554C
            {
                const f32 lfInk = (lfWidth - lpGlyph->mfAdvance) + lpGlyph->mStart.mX + lpGlyph->mDimensionsUV.mX;
                do { --lpCursor; } while ((*lpCursor & 0xC0) == 0x80);   // back to UTF-8 char start
                *lppLineEnd = lpCursor;
                return lfInk * mScaleUV.mX;
            }

            lpCursor = CgsUnicode::IncrementUtf8Pointer(lpCursor);   // 0x828354B8
        }

        Tail lTail = { this, lpGlyph, lpCursor, lpLastSpace, lfWidth, lfCommitted,
                       lfMax, lbWordWrap, lppLineStart, lppLineEnd };
        return lTail.Resolve();   // -> 0x828354D0
    }

    // Faithful port of X360 0x82835658. Build the sampler parameters for atlas page 0 and create the
    // font's runtime texture state, storing it in mpTextureState (backed by mTextureStateResource).
    // The X360 sizes the state (renderengine::TextureState::GetResourceDescriptor), allocates it via
    // the rw allocator, then renderengine::TextureState::Initialize marshals the sampler config into
    // a Xenos GPU descriptor. [PC DIVERGENCE: the PC TextureState keeps the config + raster for
    // draw-time use; the allocator/descriptor dance is X360 graphics memory management.]
    void Font::CreateTextureState(rw::IResourceAllocator* /*lpAllocator*/)
    {
        // (X360 asserts mpapTextures[0] != NULL, CgsFont.cpp:383.)
        if (mpapTextures == 0 || mpapTextures[0] == 0)
            return;

        renderengine::TextureState::Parameters lParams;
        lParams.muAddressU      = 2u;
        lParams.muAddressV      = 2u;
        lParams.muAddressW      = 0u;
        lParams.muMagFilter     = 1u;
        lParams.muMinFilter     = 1u;
        lParams.muMipFilter     = 1u;
        lParams.muField6        = 0u;
        lParams.muField7        = 0u;
        lParams.muMaxAnisotropy = 13u;
        lParams.muField9        = 0u;
        lParams.muField10       = 1u;
        lParams.mfMipLodBias    = -0.5f;
        lParams.mfField12       = 0.0f;
        lParams.muField13       = 0u;
        lParams.muField14       = 0u;
        lParams.muField15       = 0u;
        lParams.mpTexture       = mpapTextures[0];   // atlas page 0

        u32 lauDescriptor[10];
        renderengine::TextureState::GetResourceDescriptor(lauDescriptor);  // (X360 sizes + allocs the state)
        mpTextureState = renderengine::TextureState::Initialize(&mTextureStateResource, &lParams);
    }
}
