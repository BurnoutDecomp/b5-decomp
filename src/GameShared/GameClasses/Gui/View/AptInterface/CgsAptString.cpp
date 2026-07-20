#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"          // CgsGui::FontCollection::FindFont
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"            // CgsLanguage::LanguageManager::FindString
#include "GameShared/GameClasses/Fonts/CgsFont.h"                          // CgsResource::Font (GetStringWidth / typeface name)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Apt/Apt.h"                                   // AptAllocateStringParameters layout

// NOTE: the AptAux / AptRenderHandler types (CgsAptRenderHandler.h) embed a 256-slot OPAQUE pool
// stand-in named CgsGui::CgsAptString -- including that header here would redefine the REAL
// CgsAptString this TU owns (an ODR clash). The console reaches the language manager through the
// AptAux singleton + render handler; that resolution is done out-of-line by the bridge
// CgsGui::GetAptRenderHandlerLanguageManager() (bodied in CgsAptAux.cpp, which owns those types).

#include <cstring>   // strncpy / strstr (the X360 calls strncpy + strstr on the lowered font name)

// CgsGui::CgsAptString -- the apt text-object wrapper. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (GetText @0x8246D9A0, Construct @0x82851EF8, Prepare @0x82854A40)
// cross-checked against the PS3 External ELF (Reset / Prepare @0x5C6B20), which names every
// field as `mTextObject.<field>` -- so the embedded CgsGraphics::TextObject layout is canonical.

namespace CgsGui
{
    // -------------------------------------------------------------------------------------
    // The style tags Prepare searches for in the (lowercased) font name, and the gradient
    // colour tables it stamps when a tag fires. The tag STRINGS are the REAL rodata literals,
    // recovered from the X360 pointer table off_82F33174..off_82F33198 (idat 9.3 batch dump of
    // the decrypted ARTIST XEX, 2026-07-20): "grad" / "drop" / "emb" / "cond" / "bw" plus the
    // full gradient-range names. The names are the PS3 DWARF symbols (CgsGui::KC_*); note the
    // literals are ABBREVIATED substrings, so e.g. the font "B5EAConDisS" ("Condensed
    // Display") matches "cond" -- the earlier guessed full words ("condensed" etc.) never
    // matched the shipped font names, which left the condensed 0.85 spacing unapplied and the
    // autosave-prompt text ~17.6% too wide vs the console (Xenia x_03 pixel-measured).
    namespace
    {
        const char* const KC_GRADIENT_FONT_STRING            = "grad";        // off_82F33174 -> 0x820E0550
        const char* const KC_GRADIENT_RANGE_STRING_ORANGE    = "orange";      // off_82F33184 -> 0x820E0534
        const char* const KC_GRADIENT_RANGE_STRING_GREENBLUE = "greenblue";   // off_82F33188 -> 0x820E0528
        const char* const KC_GRADIENT_RANGE_STRING_RED       = "red";         // off_82F3318C -> 0x8205CF34
        const char* const KC_GRADIENT_RANGE_STRING_GOLD      = "gold";        // off_82F33190 -> 0x82048978
        const char* const KC_GRADIENT_RANGE_STRING_SILVER    = "silver";      // off_82F33194 -> 0x82048998
        const char* const KC_GRADIENT_RANGE_STRING_BLUEWHITE = "bw";          // off_82F33198 -> 0x820E0524
        const char* const KC_DROPSHADOW_FONT_STRING          = "drop";        // off_82F33178 -> 0x820E0548
        const char* const KC_EMBOSSED_FONT_STRING            = "emb";         // off_82F3317C -> 0x820E0544
        const char* const KC_FONT_SPACING_STRING_CONDENSED   = "cond";        // off_82F33180 -> 0x820E053C

        // Gradient colour pairs {top, bottom}, packed RGBA8 -- read verbatim from the X360
        // ARTIST rodata (dword_82F33298 .. dword_82F332C4).
        const CgsGraphics::RGBA K_COLOUR_GRADIENT_GREEN_BLUE[2] = { 0xFF69A569u, 0xFFE58F8Du };
        const CgsGraphics::RGBA K_COLOUR_GRADIENT_ORANGE[2]     = { 0xFF4040FFu, 0xFF8080FFu };
        const CgsGraphics::RGBA K_COLOUR_GRADIENT_RED[2]        = { 0xFF074DDCu, 0xFF0A0B66u };
        const CgsGraphics::RGBA K_COLOUR_GRADIENT_GOLD[2]       = { 0xFFB8ECF7u, 0xFF45AAB7u };
        const CgsGraphics::RGBA K_COLOUR_GRADIENT_SILVER[2]     = { 0xFFE2DEC6u, 0xFFF5F6EDu };
        const CgsGraphics::RGBA K_COLOUR_GRADIENT_BLUEWHITE[2]  = { 0xFFFFFBE3u, 0xFFBCA58Du };

        const CgsGraphics::RGBA K_COLOUR_BLACK = 0xFF000000u;   // dword_82F33290 (drop-shadow)
        const CgsGraphics::RGBA K_COLOUR_WHITE = 0xFFFFFFFFu;   // dword_82F33294 (case-3 compare)

        // The embossed default text colour (X360 sets mTextColour = -7194056 when the colour is
        // not already white) and the B5DotMat/dfheic special-case colour (mTextColour = -16730881).
        const CgsGraphics::RGBA K_COLOUR_EMBOSSED_DEFAULT = 0xFF923A38u;  // -7194056
        const CgsGraphics::RGBA K_COLOUR_B5DOTMAT_DFHEIC  = 0xFF00B4FFu;  // -16730881

        // The X360 PRESERVES the high (alpha) byte and byte-reverses only the low three bytes of each
        // param colour (clrlwi/srwi/inslwi/insrwi chain @0x82854EBC-0x82854F10): byte0 and byte2 swap,
        // byte1 and byte3 (alpha) stay in place. 0xAABBCCDD -> 0xAADDCCBB. This is NOT a full 32-bit
        // byteswap -- the alpha byte is kept; verified against the PPC rlwimi semantics over the asm.
        inline CgsGraphics::RGBA SwizzleParamColour(u32 luColour)
        {
            return  (luColour & 0xFF000000u)          // byte3 (alpha) kept in place
                 | ((luColour & 0x000000FFu) << 16)   // byte0 -> byte2
                 |  (luColour & 0x0000FF00u)          // byte1 kept in place
                 | ((luColour & 0x00FF0000u) >> 16);  // byte2 -> byte0
        }
    }

    // The alt-colour table the callers pass is the opaque global RGBA*; TextObject::Construct
    // wants the packed RGBA8-word type (CgsGraphics::RGBA == u32). They are the same 4-byte
    // colour storage, so the forward is a reinterpret_cast (the console passes the pointer
    // straight through too).
    namespace
    {
        inline const CgsGraphics::RGBA* AsTextObjectColours(const RGBA* lpColours)
        {
            return reinterpret_cast<const CgsGraphics::RGBA*>(lpColours);
        }
    }

    // @ 0x82851EF8 / PS3 Reset -- construct the embedded text object from the cached
    // alternate-colour table. (Both Construct and Reset do the same single call.)
    void CgsAptString::Construct(const RGBA* lpColours, s32 liNumColours)
    {
        mpAlternateTextColours = lpColours;
        miNumAlternateColours  = liNumColours;
        mTextObject.Construct(AsTextObjectColours(mpAlternateTextColours), miNumAlternateColours);
    }

    // PS3 Reset() -- re-construct the embedded text object from the cached alt-colour table.
    void CgsAptString::Reset()
    {
        mTextObject.Construct(AsTextObjectColours(mpAlternateTextColours), miNumAlternateColours);
    }

    // @ 0x8246D9A0 -- the font handle is the identity tag; if it is still the empty/default
    // handle (mpResourceMemory == null) the object was never set up. Return the cached text.
    CgsAptString::TextHandle CgsAptString::GetText() const
    {
        CGS_ASSERT(mTextObject.mpFont.mpResourceMemory != 0,
                   "Can't GetText() - Text Object hasn't been set up yet\n");
        return mTextObject.mpUtf8String;
    }

    // @ 0x82854A40 -- lay the parametrised string out into the caller's buffer and stamp the
    // result back onto lpParams. Faithful decompile of the X360 ARTIST body; the PS3 External
    // ELF (which names every field) is the layout cross-check.
    void CgsAptString::Prepare(const FontCollection* lpFonts,
                               AptAllocateStringParameters* lpParams,
                               CgsUnicode::CgsUtf8* lpStringBuffer,
                               ETextEffects eEffect,
                               f32 lfSizeScale)
    {
        CGS_ASSERT(lpFonts != 0, "lpFonts");
        CGS_ASSERT(lpParams != 0, "Invalid AptAllocateStringParameters in CgsAptString::Prepare()");

        // Reset the embedded text object (X360 calls TextObject::Construct directly with the
        // cached alt-colour table; PS3 routes through Reset()). Same effect either way.
        mTextObject.Construct(AsTextObjectColours(mpAlternateTextColours), miNumAlternateColours);

        // The boolean param flags become 0/1 (the console `_cntlzw(x) & 0x20 == 0` idiom is
        // "x != 0"; `>>4 & 1` extracts the italic bit of nFontStyle).
        mTextObject.mbBackground = (lpParams->bBackground != 0) ? 1 : 0;
        mTextObject.mbBorder     = (lpParams->bBorder     != 0) ? 1 : 0;
        mTextObject.mbItalic     = (lpParams->nFontStyle >> 4) & 1;
        mTextObject.mbMultiLine  = (lpParams->bMultiline  != 0) ? 1 : 0;
        mTextObject.mbWordWrap   = (lpParams->bWordWrap   != 0) ? 1 : 0;

        // ---- font-name style tags ----------------------------------------------------------
        // Lowercase a bounded copy of the font name and scan it for the gradient / drop-shadow /
        // emboss / condensed-spacing tags. (X360 asserts the name fits the 0x80 buffer.)
        const char* lpcFontName = lpParams->szFontName;
        if (lpcFontName)
        {
            char lacLoweredName[128];
            std::strncpy(lacLoweredName, lpcFontName, 128);
            lacLoweredName[127] = 0;   // strncpy does not NUL-terminate on a full copy
            for (char* lpc = lacLoweredName; *lpc; ++lpc)
            {
                if (*lpc >= 'A' && *lpc <= 'Z')
                    *lpc = static_cast<char>(*lpc - 'A' + 'a');   // CGSSTRNLOWER
            }

            if (std::strstr(lacLoweredName, KC_GRADIENT_FONT_STRING))
            {
                mTextObject.mbGradient = true;
                if (std::strstr(lacLoweredName, KC_GRADIENT_RANGE_STRING_ORANGE))
                {
                    mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_ORANGE[0];
                    mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_ORANGE[1];
                }
                if (std::strstr(lacLoweredName, KC_GRADIENT_RANGE_STRING_GREENBLUE))
                {
                    mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_GREEN_BLUE[0];
                    mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_GREEN_BLUE[1];
                }
                if (std::strstr(lacLoweredName, KC_GRADIENT_RANGE_STRING_RED))
                {
                    mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_RED[0];
                    mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_RED[1];
                }
                if (std::strstr(lacLoweredName, KC_GRADIENT_RANGE_STRING_GOLD))
                {
                    mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_GOLD[0];
                    mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_GOLD[1];
                }
                if (std::strstr(lacLoweredName, KC_GRADIENT_RANGE_STRING_SILVER))
                {
                    mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_SILVER[0];
                    mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_SILVER[1];
                }
                if (std::strstr(lacLoweredName, KC_GRADIENT_RANGE_STRING_BLUEWHITE))
                {
                    mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_BLUEWHITE[0];
                    mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_BLUEWHITE[1];
                }
            }

            if (std::strstr(lacLoweredName, KC_DROPSHADOW_FONT_STRING))
                mTextObject.mbDropShadow = true;
            if (std::strstr(lacLoweredName, KC_EMBOSSED_FONT_STRING))
                mTextObject.mbEmbossed = true;
            if (std::strstr(lacLoweredName, KC_FONT_SPACING_STRING_CONDENSED))
                mTextObject.mfCharSpacingMultiplier = 0.85000002f;
        }

        // ---- base colours ------------------------------------------------------------------
        const bool lbBackground = (mTextObject.mbBackground != 0);
        mTextObject.mDropShadowColour = K_COLOUR_BLACK;
        mTextObject.mBorderColour     = SwizzleParamColour(lpParams->nBorderColor);
        mTextObject.mTextColour       = SwizzleParamColour(lpParams->nColour);
        if (lbBackground)
        {
            const CgsGraphics::RGBA luBack = SwizzleParamColour(lpParams->nBackColor);
            mTextObject.mBackgroundColours.mTopLeft     = luBack;
            mTextObject.mBackgroundColours.mTopRight    = luBack;
            mTextObject.mBackgroundColours.mBottomLeft  = luBack;
            mTextObject.mBackgroundColours.mBottomRight = luBack;
        }

        // ---- explicit effect override ------------------------------------------------------
        switch (eEffect)
        {
        case E_EFFECT_DROPSHADOW:
            mTextObject.mbDropShadow = true;
            break;
        case E_EFFECT_GRADIENT:
            mTextObject.mbGradient = true;
            mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_RED[0];
            mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_RED[1];
            break;
        case E_EFFECT_EMBOSSED:
            mTextObject.mbEmbossed = true;
            // X360 (asm 0x82855060-94): if the text colour IS white, stamp the default embossed grey.
            if (mTextObject.mTextColour == K_COLOUR_WHITE)
                mTextObject.mTextColour = K_COLOUR_EMBOSSED_DEFAULT;
            break;
        case E_EFFECT_ALL:
            mTextObject.mbDropShadow = true;
            mTextObject.mbEmbossed   = true;
            mTextObject.mbGradient   = true;
            mTextObject.mGradientColour1 = K_COLOUR_GRADIENT_GOLD[0];
            mTextObject.mGradientColour2 = K_COLOUR_GRADIENT_GOLD[1];
            break;
        case E_EFFECT_NONE:
        default:
            break;
        }

        if (lfSizeScale != 1.0f)
            mTextObject.mfCharSpacingMultiplier = lfSizeScale;

        // ---- alignment + box -------------------------------------------------------------
        // The console picks the box alignment when the eFlags multi-box bit (8) is set, else the
        // text alignment; the chosen value maps to ETextAlignment and a horizontal nudge.
        f32 lfAlignNudge = 0.0f;
        const s32 liAlignment = ((lpParams->eFlags & 8u) != 0)
                                  ? static_cast<s32>(lpParams->eBoxAlignment)
                                  : static_cast<s32>(lpParams->eAlignment);
        switch (liAlignment)
        {
        case 0:
        case 3:
            mTextObject.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_LEFT;
            lfAlignNudge = 2.0f;
            break;
        case 1:
            mTextObject.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_RIGHT;
            lfAlignNudge = -2.0f;
            break;
        case 2:
            mTextObject.meAlignment = CgsGraphics::TextObject::E_ALIGNMENT_CENTER;
            lfAlignNudge = -0.5f;
            break;
        default:
            CGS_ASSERT(false, "Unsupported alignment in CgsAptString::Prepare");
            break;
        }

        // Screen rect (the +3.0 vertical padding and the alignment nudge come straight from the
        // console). mv2TopLeft = (x0 + nudge, y0 + 3); mv2BottomRight = (x1 + nudge, y1 + 3).
        mTextObject.mv2TopLeft.mX     = lpParams->x0 + lfAlignNudge;
        mTextObject.mv2TopLeft.mY     = lpParams->y0 + 3.0f;
        mTextObject.mv2BottomRight.mX = lpParams->x1 + lfAlignNudge;
        mTextObject.mv2BottomRight.mY = lpParams->y1 + 3.0f;
        mTextObject.mfFontHeight      = lpParams->fFontHeight * 1.11f;

        // ---- resolve the string ($LANGID / ~ keys go through the language manager) ---------
        CGS_ASSERT(lpParams->szString != 0, "Invalid string sent to CgsAptString::Prepare");
        const char* lpcString = lpParams->szString;
        const char  lcLead    = lpcString[0];
        if (lcLead == '$' || lcLead == '~')
        {
            const char* lpcKey = lpcString + 1;
            // Reach the language manager the way the console does (AptAux singleton -> render
            // handler -> mpLanguageManager); the bridge asserts the singleton/handler/manager.
            CgsLanguage::LanguageManager* lpLanguage = GetAptRenderHandlerLanguageManager();

            const CgsUnicode::CgsUtf8* lpResolved =
                reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpLanguage->FindString(lpcKey));
            mTextObject.mpUtf8String = lpResolved;
            if (mTextObject.mbAutosize)
                mTextObject.CalculateAutosizing();

            // No localised string found: fall back to the raw key string (sans the lead char).
            if (!mTextObject.mpUtf8String)
            {
                mTextObject.mpUtf8String =
                    reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpParams->szString);
                if (mTextObject.mbAutosize)
                    mTextObject.CalculateAutosizing();
            }

            CGS_ASSERT(CgsUnicode::IsValidUtf8String(mTextObject.mpUtf8String),
                       "Invalid string fetched from a $LANGID key");
        }
        else
        {
            mTextObject.mpUtf8String =
                reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpcString);
            if (mTextObject.mbAutosize)
                mTextObject.CalculateAutosizing();

            CGS_ASSERT(CgsUnicode::IsValidUtf8String(mTextObject.mpUtf8String),
                       "Invalid string");
        }

        // Stamp this string object back onto the parameters as the "current string".
        lpParams->pCurrString = reinterpret_cast<AptAssetString>(this);

        // ---- pick the font ----------------------------------------------------------------
        // (FindFont is DWARF-attested const, returning a const handle reference.)
        const CgsResource::SafeResourceHandle<CgsResource::Font>& lFontHandle =
            lpFonts->FindFont(lpParams->szFontName);
        mTextObject.mpFont = lFontHandle;

        // The B5DotMat dot-matrix font's "dfheic" variant gets a special cyan text colour AND a 1.2x
        // font-height bump (X360 0x82854A40 lines 432-434: *v26 = -16730881; *(a1+8) = *(a1+8) * 1.2).
        // The bump must precede the fTextWidth computation below (= stringWidth * mfFontHeight).
        if (std::strstr(lpParams->szFontName, "B5DotMat") &&
            std::strstr(mTextObject.mpFont.operator->()->macTypefaceFamilyName, "dfheic"))
        {
            mTextObject.mTextColour   = K_COLOUR_B5DOTMAT_DFHEIC;
            mTextObject.mfFontHeight *= 1.2f;
        }

        // Unless the source was a '~' key (left already pointing at the language string), copy the
        // resolved text into the caller's buffer and point the text object at that stable copy.
        if (lpParams->szString[0] != '~')
        {
            CgsUnicode::CopyN(lpStringBuffer, mTextObject.mpUtf8String, 256);
            mTextObject.mpUtf8String = lpStringBuffer;
            if (mTextObject.mbAutosize)
                mTextObject.CalculateAutosizing();
        }

        // ---- line layout ------------------------------------------------------------------
        const CgsUnicode::CgsUtf8* lpLineStart = mTextObject.mpUtf8String;
        u32 luNumLines = mTextObject.GetNumLinesAndStartLine(lpParams->nLineOffset - 1, &lpLineStart);
        const u32 luReportedLines = luNumLines;
        if (lpParams->nLineOffset > luNumLines)
        {
            lpLineStart = mTextObject.mpUtf8String;
            CGS_ASSERT(luNumLines > 0, "luNumLines > 0");
            mTextObject.GetNumLinesAndStartLine(luNumLines - 1, &lpLineStart);
        }
        mTextObject.mpUtf8String = lpLineStart;
        if (mTextObject.mbAutosize)
            mTextObject.CalculateAutosizing();
        lpParams->nMaxScroll = luReportedLines;   // [c:+104] the line count is stamped here

        // The font handle must have resolved to a real font (not the empty/default handle).
        CGS_ASSERT(mTextObject.mpFont.mpResourceMemory != 0, "Did not find font");

        // ---- measure + publish ------------------------------------------------------------
        const f32 lfStringWidth =
            mTextObject.mpFont.operator->()->GetStringWidth(mTextObject.mpUtf8String);
        mTextObject.mfStringWidth = lfStringWidth;
        if (mTextObject.mbAutosize)
            mTextObject.CalculateAutosizing();

        lpParams->fTextWidth  = lfStringWidth * mTextObject.mfFontHeight;   // [c:+92]
        lpParams->fTextHeight = lpParams->y1 - lpParams->y0;                // [c:+96]
    }

    // @ 0x82855648 -- re-point the already-prepared string at new text and re-measure.
    // The same resolve/copy/measure tail as Prepare above, minus the layout: unless
    // lbAlreadyLocalised, a '$'/'~' lead routes the key through the render handler's
    // language manager (falling back to the raw string when unknown); non-'~' text is
    // then copied into the caller's persistent buffer; finally the font is asserted
    // and the string re-measured. Every text/width store re-runs autosizing when the
    // object autosizes, exactly as the console body does. The console's inlined
    // singleton/handler/manager asserts (cpp:437/439/440) live in the shared bridge
    // (GetAptRenderHandlerLanguageManager, CgsAptAux.cpp) per this file's precedent.
    void CgsAptString::SetText(const CgsUnicode::CgsUtf8* lpNewText,
                               CgsUnicode::CgsUtf8* lpcStringBuffer,
                               bool lbAlreadyLocalised)
    {
        CGS_ASSERT(lpNewText != 0, "lpNewText");             // cpp:434
        CGS_ASSERT(lpcStringBuffer != 0, "lpcStringBuffer"); // cpp:435

        const char lcLead = static_cast<char>(lpNewText[0]);
        if (!lbAlreadyLocalised && (lcLead == '$' || lcLead == '~'))
        {
            CgsLanguage::LanguageManager* lpLanguage = GetAptRenderHandlerLanguageManager();

            const CgsUnicode::CgsUtf8* lpResolved = lpLanguage->FindString(
                reinterpret_cast<const char*>(lpNewText + 1));
            mTextObject.mpUtf8String = lpResolved;
            if (mTextObject.mbAutosize)
                mTextObject.CalculateAutosizing();

            // No localised string found: fall back to the raw key string.
            if (!mTextObject.mpUtf8String)
            {
                mTextObject.mpUtf8String = lpNewText;
                if (mTextObject.mbAutosize)
                    mTextObject.CalculateAutosizing();
            }

            // cpp:455 -- the X360 streams "Invalid string fetched from : <key> : <text>";
            // folded static per convention.
            CGS_ASSERT(CgsUnicode::IsValidUtf8String(mTextObject.mpUtf8String),
                       "Invalid string fetched from : ");
        }
        else
        {
            mTextObject.mpUtf8String = lpNewText;
            if (mTextObject.mbAutosize)
                mTextObject.CalculateAutosizing();

            // cpp:470 -- streamed "Invalid string : <text>"; folded static.
            CGS_ASSERT(CgsUnicode::IsValidUtf8String(mTextObject.mpUtf8String),
                       "Invalid string : ");
        }

        // Unless the source was a '~' key (left pointing at the language string), copy the
        // resolved text into the caller's buffer and point the text object at that copy.
        if (static_cast<char>(lpNewText[0]) != '~')
        {
            CgsUnicode::CopyN(lpcStringBuffer, mTextObject.mpUtf8String, 256);
            mTextObject.mpUtf8String = lpcStringBuffer;
            if (mTextObject.mbAutosize)
                mTextObject.CalculateAutosizing();
        }

        // The font handle must still be a real font (not the empty/default sentinel
        // pair the X360 compares against). cpp:481.
        CGS_ASSERT(mTextObject.mpFont.mpResourceMemory != 0, "Did not find font");

        // Re-measure the new text and re-run autosizing.
        mTextObject.mfStringWidth =
            mTextObject.mpFont.operator->()->GetStringWidth(mTextObject.mpUtf8String);
        if (mTextObject.mbAutosize)
            mTextObject.CalculateAutosizing();
    }
}
