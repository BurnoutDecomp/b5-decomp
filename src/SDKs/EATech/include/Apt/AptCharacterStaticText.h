#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterStaticText: the baked static-text character (the SWF
// DefineText analogue). It owns no editable buffer; the glyph runs are authored
// into the .apt and relocated in place by AptCharacterAnimation::Fixup, so the
// runtime walks them through this character.
//
// LAYOUT recovered store-for-store from the X360 ARTIST.XEX static-text render
// (AptRenderItemStaticText::Render @0x82AFEC18), which is the only code in scope
// that walks it. Console byte offsets are documentation; members are accessed BY
// NAME so the x64 widths are correct (the engine is 32-bit-pointer + hard-offset
// code -- same rule as AptCharacter / AptCharacterDynamicText).
//
//   AptCharacter base                          [c:+0x00..+0x0F]
//   mafAuthoredBounds[4]   authored fields     [c:+0x10..+0x1F]  (see note)
//   mTransform             AptMatrix           [c:+0x20]  the character base xform,
//                                                         concat'd onto the item's
//                                                         position matrix before the
//                                                         paragraph walk
//   miNumParagraphs        int32               [c:+0x38]
//   mpParagraphs           AptStaticTextParagraph*  [c:+0x3C]
//
// Each paragraph (56-byte serialised record) carries a font index, a flat ARGB
// colour transform, a baked position + font scale, and a run of glyphs; each glyph
// (4-byte record) is a {font-glyph index, pen advance} pair. The font index selects
// a font character from the owning movie's font table (reached through the
// AptCharacter::mpFixupLink back-link), and each glyph index selects a shape
// character from that font's glyph table -- both serialised .apt tables resolved
// through the declared helpers in AptRenderItemStaticText.cpp (the same deferred
// glyph-list-walk family as AptResolveDefaultTextFont).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacter.h"        // base
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"    // mTransform
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"    // AptFloatArrayCXForm (per-paragraph colour)

// One baked glyph in a static-text run (4-byte serialised record).
struct AptStaticTextGlyph
{
    int16_t miGlyphIndex;   // [+0x00] index into the font character's glyph table
    int16_t miAdvance;      // [+0x02] pen advance, applied as advance * 0.05 (KF_GLYPH_ADVANCE_SCALE)
};

// One baked paragraph / text run (56-byte serialised record, [c:0x38]).
struct AptStaticTextParagraph
{
    int32_t             miFontIndex;        // [+0x00] index into the font owner's font table
    AptFloatArrayCXForm mColourTransform;   // [+0x04] flat ARGB colour transform (scale[4]+translate[4])
    float               mfX;                // [+0x24] paragraph origin x
    float               mfY;                // [+0x28] paragraph origin y
    float               mfFontScale;        // [+0x2C] uniform glyph scale (matrix a == d)
    int32_t             miNumGlyphs;        // [+0x30]
    AptStaticTextGlyph* mpGlyphs;           // [+0x34]
};

struct AptCharacterStaticText : public AptCharacter
{
    // [c:+0x10..+0x1F] authored fields not read by Render. By analogy with
    // AptCharacterDynamicText's four field-edge margins this is the authored bounds
    // rect; left as a documented reserved run because no in-scope code decodes it
    // for the static-text character (kept as four floats -- floats do not widen on
    // x64, so the documented shape is width-stable).
    float mafAuthoredBounds[4];

    AptMatrix               mTransform;        // [c:+0x20]
    int32_t                 miNumParagraphs;   // [c:+0x38]
    AptStaticTextParagraph* mpParagraphs;      // [c:+0x3C]
};
