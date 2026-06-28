#include "GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h"

#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"            // BrnFlapt::FlaptRenderer (mpFonts)
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"             // BrnFlapt::MovieClip / FlaptFile / TextField / FontStyle
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"          // CgsUnicode::CgsUtf8
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "SDKs/EATech/include/Apt/Apt.h"                      // AptAllocateStringParameters / AptStringAlignment

// BrnFlapt::TextFieldInstance member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.cpp)
// bodies the two X360-emitted functions:
//
//   Construct            @ 0x8246F548
//   SetUpAptStringParams @ 0x8246DF58   (static helper; no `this` in the X360 asm)
//
// Construct wires a static text field's apt string from the serialised MovieClip
// (its TextField + the owning FlaptFile's FontStyle/string tables), then lays the
// string out into the instance's own UTF-8 buffer. SetUpAptStringParams translates
// the TextField + FontStyle + initial text into the apt allocate-string descriptor.
// The X360-baked file/line cites in the asserts are discarded per project convention.

namespace BrnFlapt
{

// ---- Construct @ 0x8246F548 ----------------------------------------------
// r3=this, r4=lpMovieClip, r5=luTextFieldIndex, r6=lpRenderer, r7=lpColours,
// r8=liNumColours. Assert the clip and that the field index is in range
// (luTextFieldIndex < lpMovieClip->muNumTextFields, a byte at MovieClip+0x03),
// construct the embedded apt string with the alternate-colour table, fill the
// apt string-allocation parameters, then Prepare into macStringData.
void TextFieldInstance::Construct(const MovieClip* lpMovieClip,
                                  u32 luTextFieldIndex,
                                  const FlaptRenderer* lpRenderer,
                                  const RGBA* lpAlternateTextColours,
                                  s32 liNumAlternateColours)
{
    CGS_ASSERT(lpMovieClip != 0, "lpMovieClip");
    CGS_ASSERT(luTextFieldIndex < lpMovieClip->muNumTextFields,
               "luTextFieldIndex < lpMovieClip->muNumTextFields");

    // Cache the alternate-text-colour table + construct the embedded text object.
    mAptString.Construct(lpAlternateTextColours, liNumAlternateColours);

    // Resolve the serialised TextField, its FontStyle, and its initial string from
    // the owning FlaptFile (the X360 indexes mpaTextFields by index*0x20, then reads
    // the font-style array at FlaptFile+0x28 and the string table at FlaptFile+0x44).
    const TextField* lpTextField = &lpMovieClip->mpaTextFields[luTextFieldIndex];
    const FlaptFile* lpFile      = lpMovieClip->mpFile;
    const FontStyle* lpFontStyle = &lpFile->mpaFontStyles[lpTextField->muFontStyleIndex];
    const CgsUnicode::CgsUtf8* lpInitialText = lpFile->mpapStrings[lpTextField->muInitialStringId];

    AptAllocateStringParameters lAptStringParams;
    SetUpAptStringParams(lpTextField, lpFontStyle, lpInitialText, &lAptStringParams);

    // Lay the string out into this instance's UTF-8 buffer (the X360 passes this+0x84
    // == macStringData), with no text effect and a unit size scale. The renderer's
    // font collection is the inlined GetFonts() (FlaptRenderer::mpFonts at +0x0C).
    mAptString.Prepare(static_cast<const CgsGui::FontCollection*>(lpRenderer->mpFonts),
                       &lAptStringParams,
                       macStringData,
                       CgsGui::CgsAptString::E_EFFECT_NONE,
                       1.0f);
}

// ---- SetUpAptStringParams @ 0x8246DF58 -----------------------------------
// Static: the X360 passes the four pointers in r3..r6 with no `this`. Assert each is
// non-null, then translate the serialised TextField + FontStyle + initial text into
// the apt allocate-string descriptor. The unset output fields (pnNumLines, fTextWidth,
// fTextHeight, fStrLen) are left for CgsAptString::Prepare to fill, exactly as the
// X360 does (no stores to them here).
void TextFieldInstance::SetUpAptStringParams(const TextField* lpTextField,
                                             const FontStyle* lpFontStyle,
                                             const CgsUnicode::CgsUtf8* lpInitialText,
                                             AptAllocateStringParameters* lpOutAptStringParams)
{
    CGS_ASSERT(lpTextField != 0, "lpTextField");
    CGS_ASSERT(lpFontStyle != 0, "lpFontStyle");
    CGS_ASSERT(lpInitialText != 0, "lpInitialText");
    CGS_ASSERT(lpOutAptStringParams != 0, "lpOutAptStringParams");

    // Flags byte (TextField+0x0B): bit0 -> word-wrap, bit1 -> multiline.
    lpOutAptStringParams->bWordWrap  = lpTextField->mxFlags & 1;
    lpOutAptStringParams->bMultiline = (lpTextField->mxFlags >> 1) & 1;

    // Alignment byte (TextField+0x0C) drives both the line and box alignment.
    lpOutAptStringParams->eAlignment    = static_cast<AptStringAlignment>(lpTextField->muAlignment);
    lpOutAptStringParams->eBoxAlignment = static_cast<AptStringAlignment>(lpTextField->muAlignment);

    // Font from the FontStyle entry.
    lpOutAptStringParams->fFontHeight = lpFontStyle->mfFontHeight;
    lpOutAptStringParams->nColour     = lpFontStyle->muColour;
    lpOutAptStringParams->szFontName  = lpFontStyle->mpacFontName;

    // The initial UTF-8 text to lay out.
    lpOutAptStringParams->szString = reinterpret_cast<const char*>(lpInitialText);

    // The text-field bounding box.
    lpOutAptStringParams->x0 = lpTextField->mTopLeft.mfX;
    lpOutAptStringParams->y0 = lpTextField->mTopLeft.mfY;
    lpOutAptStringParams->x1 = lpTextField->mBottomRight.mfX;
    lpOutAptStringParams->y1 = lpTextField->mBottomRight.mfY;

    // Fixed presentation defaults the X360 bakes in.
    lpOutAptStringParams->eFlags       = 6;
    lpOutAptStringParams->pCurrString  = 0;
    lpOutAptStringParams->bBackground  = 0;
    lpOutAptStringParams->bBorder      = 0;
    lpOutAptStringParams->nBackColor   = 0xFFFFFFFFu;
    lpOutAptStringParams->nBorderColor = 0xFF000000u;
    lpOutAptStringParams->nFontStyle   = 0;
    lpOutAptStringParams->nIndent      = -1;
    lpOutAptStringParams->nLeftMargin  = -1;
    lpOutAptStringParams->nRightMargin = -1;
    lpOutAptStringParams->nLineOffset  = 1;
    lpOutAptStringParams->nMaxScroll   = 0;
}

}
