// ===========================================================================
// EATech Apt -- AptRenderItemStaticText.   DECOMPILED from the X360 ARTIST.XEX.
//   Clone 0x82AEC950 / Render 0x82AFEC18 (vtable off_821458D0, flag 0x280000).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemStaticText.h"
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // the def base (the font resolvers)
#include "SDKs/EATech/include/Apt/AptCIH.h"                    // KU_AptEmbeddedMovieOff
#include "SDKs/EATech/include/Apt/AptCharacterStaticText.h"  // the baked paragraph/glyph layout
#include "SDKs/EATech/include/Apt/AptCharacter.h"            // AptCharacter::render (per-glyph shape draw)
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"     // matrix/colour stack + gAptIdentityMatrix
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"             // DOGMA_PoolManager
#include <new>                                     // placement new

// ---------------------------------------------------------------------------
// Serialised .apt font/glyph table resolvers (the static-text glyph-list walk).
//
// The baked paragraphs reference a font by index into the owning movie's font
// table -- reached through the AptCharacter::mpFixupLink back-link Fixup wires to
// the character-table base -- and each glyph references a shape character by index
// into the resolved font's glyph table. Both are serialised .apt tables embedded in
// subclass fields with no C++ home in this TU's scope, so the lookups route through
// these declared helpers rather than offset-poked, matching the deferred
// glyph-list-walk family (AptCharacterHelper::AptResolveDefaultTextFont):
//   AptResolveTextFontCharacter : *(pFontOwner+0x20)[nFontIndex]  -> font character
//   AptResolveFontGlyph         : *(pFontChar+0x18)[nGlyphIndex]  -> glyph shape character
// ---------------------------------------------------------------------------
// HOMED 2026-07-02 (retiring the AptRenderLinkStubs nulls). Native-8 forms of
// the console reads (the offsets flatten through the def base):
//   * TextFontCharacter: console `*(fontOwner+0x20)[i]` == root(+0x10 defbase)
//     (+0x10 charTable) -- native: the def base at mpFixupLink +
//     KU_AptEmbeddedMovieOff, then its typed mpCharacterTable[nFontIndex].
//   * FontGlyph: console `*(fontChar+0x18)[i]` -- the FONT record's glyph
//     char-pointer array (native +0x30, the slot Fixup case-3 relocates;
//     Unresolve's inverse re-indexes these pointers back to ids, proving the
//     live entries are pointers). The font subclass fields have no C++ home
//     yet, so the read stays a doctrine-sanctioned serialized-record access
//     with the console offset noted.
AptCharacter* AptResolveTextFontCharacter(AptCharacter* pFontOwner, int nFontIndex)
{
    AptCharacterAnimation* const pDef =
        reinterpret_cast<AptCharacterAnimation*>(
            reinterpret_cast<char*>(pFontOwner) + KU_AptEmbeddedMovieOff);
    return pDef->mpCharacterTable[nFontIndex];
}

AptCharacter* AptResolveFontGlyph(AptCharacter* pFontChar, int nGlyphIndex)
{
    AptCharacter** const ppGlyphs = *reinterpret_cast<AptCharacter***>(
        reinterpret_cast<char*>(pFontChar) + 0x30);   // [c: 0x18] the glyph array
    return ppGlyphs[nGlyphIndex];
}

// Pen-advance scale: the baked int16 advance walks the per-glyph x position as
// advance * 0.05 (X360 flt_820047C8).
static const float KF_GLYPH_ADVANCE_SCALE = 0.05f;

// Factory ctor -- base render item + the static-text render-type flag.
AptRenderItemStaticText::AptRenderItemStaticText(AptCharacter* pCharacter, int nCreatedOnTick)
    : AptRenderItem(pCharacter, nCreatedOnTick)
{
    mFlags = (mFlags & ~0x3F00u) | 0xA00u;   // static-text=10; x64 type field (XB1 ctor 0x140826F50 `or 0A00h`)
}

// Clone copy-ctor -- base clone copy + re-stamp the static-text render-type flag.
AptRenderItemStaticText::AptRenderItemStaticText(const AptRenderItemStaticText* pSource, int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItem(pSource, nCreatedOnTick, bCopyExtended)
{
    mFlags = (mFlags & ~0x3F00u) | 0xA00u;   // static-text=10; x64 type field (XB1 ctor 0x140826F50 `or 0A00h`)
}

// Clone @0x82AEC950 -- pool-allocate a fresh static-text render item copy-init'd
// from this one (null on pool exhaustion).
AptRenderItem* AptRenderItemStaticText::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* p = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemStaticText));
    if (!p)
        return nullptr;
    return new (p) AptRenderItemStaticText(this, nCreatedOnTick, bCopyExtended);
}

// Render @0x82AFEC18 -- draw the baked static-text glyph runs.
//
// Bracket the whole character in the item's colour/position transforms plus the
// character's own base transform, then for each paragraph push its colour
// transform and walk its glyph run: each glyph is drawn via AptCharacter::render
// under a per-glyph vertex matrix (uniform font scale, positioned at the running
// pen). The pen continues across paragraphs that share an origin and resets when
// the origin moves.
void AptRenderItemStaticText::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    const AptCharacterStaticText* pChar =
        static_cast<const AptCharacterStaticText*>(mpCharacter);

    // Item colour transform + position matrix, then the character's base transform.
    PushMatrices(pCtx, this);
    pCtx->pushVertexMatrix();
    pCtx->appendVertexMatrix(const_cast<AptMatrix*>(&pChar->mTransform));

    // The pen tracks the previous paragraph's origin: a paragraph that repeats the
    // same (x,y) continues the running pen; a new origin resets it. The console
    // seeds the previous origin to a sentinel far outside any layout.
    float fPrevX   = -100000000.0f;
    float fPrevY   = -100000000.0f;
    float fAdvance = 0.0f;

    // Reused per-glyph vertex matrix, seeded from identity (b/c stay 0 throughout;
    // a/d/tx/ty are re-stamped per glyph).
    AptMatrix glyphMatrix = gAptIdentityMatrix;

    for (int32_t p = 0; p < pChar->miNumParagraphs; ++p)
    {
        const AptStaticTextParagraph& para = pChar->mpParagraphs[p];

        // Per-paragraph colour transform pushed onto the context.
        pCtx->pushColourTransform();
        AptCXForm cxform;
        cxform.AptFloatArrayCXFormCopy(&para.mColourTransform);
        pCtx->appendColourTransform(&cxform);

        // Resolve the paragraph's font character once for the whole run.
        AptCharacter* pFontChar =
            AptResolveTextFontCharacter(pChar->mpFixupLink, para.miFontIndex);

        // Reset the running pen when the paragraph origin moves.
        if (fPrevX != para.mfX || fPrevY != para.mfY)
            fAdvance = 0.0f;
        fPrevX = para.mfX;
        fPrevY = para.mfY;

        for (int32_t g = 0; g < para.miNumGlyphs; ++g)
        {
            const AptStaticTextGlyph& glyph = para.mpGlyphs[g];

            // Per-glyph transform: uniform font scale, positioned at the running pen.
            glyphMatrix.a  = para.mfFontScale;
            glyphMatrix.d  = para.mfFontScale;
            glyphMatrix.tx = fAdvance + para.mfX;
            glyphMatrix.ty = para.mfY;

            // FLAG: when (gAptOptFlags & 4) the console takes the _drawCharacterInstOpti
            // clip-stack fast-path here (sub_82ADFDB0 pushes a clip entry; the matching
            // gsnClipStackIndex pop runs after the draw). Deferred exactly as
            // AptRenderItem::PushMatrices defers it; the standard append/draw path below
            // renders the glyph correctly in the default configuration.
            pCtx->pushVertexMatrix();
            pCtx->appendVertexMatrix(&glyphMatrix);
            AptCharacter* pGlyphChar = AptResolveFontGlyph(pFontChar, glyph.miGlyphIndex);
            pGlyphChar->render(pCtx, eOp, nTick);
            pCtx->popVertexMatrix();

            fAdvance += static_cast<float>(glyph.miAdvance) * KF_GLYPH_ADVANCE_SCALE;
        }

        pCtx->popColourTransform();
    }

    pCtx->popVertexMatrix();
    PopMatrices(pCtx, this);
}
