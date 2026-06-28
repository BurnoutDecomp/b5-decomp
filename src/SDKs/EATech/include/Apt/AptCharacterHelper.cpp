// ===========================================================================
// EATech Apt -- AptCharacterHelper bodies.   DECOMPILED from the X360 ARTIST.XEX
// (no Feb-2007 / DecFIGS source exists for this helper):
//     AptCharacterHelper::CreateTextCharacterInst @ 0x82B010C0
//     AptCharacterHelper::Shutdown                @ 0x82AE2FA0
//
// The lazy factory for the shared DEFAULT dynamic-text character template
// (spDefaultTextCharacter == off_8324E498) that backs AS-created text fields
// (MovieClip.createTextField). Shutdown frees it together with the sibling
// default movie-clip template (spDefaultMovieCharacter == off_8324E49C, built by
// AptCharacterHelper::CreateMovieCharacterInst).
//
// Both templates are 68-byte (0x44) AptCharacter subtype blocks allocated from the
// non-GC Apt pool (gpNonGCPoolManager == off_8324D808). The X360 stamps the text
// template's authored layout defaults field-by-field; reconstructed here BY NAME
// on AptCharacterDynamicText (no offset pokes). The one piece with no
// reconstructable home -- the serialised .apt font glyph-list walk that picks the
// default font + glyph index -- is routed through the declared (deferred)
// AptResolveDefaultTextFont helper rather than offset-poked.
//
// The _savegprlr_29 / _restgprlr_29 in Shutdown's asm are the compiler's
// register-save/restore prologue thunks (not real calls) -- folded away here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"  // the text template layout (by name)
#include "SDKs/EATech/include/Apt/AptCharacter.h"             // mnType / mnRefAndFlags / mpFixupLink
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCharacterInst::mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"            // AptRenderItem::mpCharacter
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH::mpCharacterInst
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpNonGCPoolManager (off_8324D808)
#include "SDKs/EATech/Apt/DogmaAllocator.h"                   // DOGMA_PoolManager::Allocate/Deallocate

#include <cstring>   // memset

// The authored default-text constants the X360 stamps (the .rdata float literals
// flt_82006D70 / flt_82001D9C / flt_82013FB0 the builder loads).
static const float  KF_DEFAULT_TEXT_MARGIN_LOW  = -2.0f;  // flt_82006D70
static const float  KF_DEFAULT_TEXT_MARGIN_HIGH =  2.0f;  // flt_82001D9C
static const float  KF_DEFAULT_TEXT_FONT_SIZE   = 12.0f;  // flt_82013FB0
// The text-character per-type flag the builder OR-s into mnRefAndFlags' low half.
static const uint32_t KU_TEXT_CHARACTER_FLAG    = 0x8000u;
// The .apt dynamic-text character type tag.
static const int32_t  KI_CHARACTER_TYPE_TEXT    = 2;

// ---- the cached default-character template singletons (X360 .data) ------------
AptCharacterDynamicText* AptCharacterHelper::spDefaultTextCharacter  = nullptr;  // off_8324E498
AptCharacter*            AptCharacterHelper::spDefaultMovieCharacter = nullptr;  // off_8324E49C

// ---------------------------------------------------------------------------
// CreateTextCharacterInst @0x82B010C0
// ---------------------------------------------------------------------------
AptCIH* AptCharacterHelper::CreateTextCharacterInst()
{
    // Allocate the shared default dynamic-text character template from the non-GC
    // pool and zero it (the X360 Allocate(68) + memset(.,0,68)).
    spDefaultTextCharacter =
        static_cast<AptCharacterDynamicText*>(gpNonGCPoolManager->Allocate(sizeof(AptCharacterDynamicText)));
    std::memset(spDefaultTextCharacter, 0, sizeof(AptCharacterDynamicText));

    AptCharacterDynamicText* pTemplate = spDefaultTextCharacter;

    // Authored layout defaults stamped into the template (in the X360 store order).
    pTemplate->mnType = KI_CHARACTER_TYPE_TEXT;                  // [c:0x00]
    pTemplate->mfMarginLeft   = KF_DEFAULT_TEXT_MARGIN_LOW;      // [c:0x10] -2
    pTemplate->mfMarginTop    = KF_DEFAULT_TEXT_MARGIN_LOW;      // [c:0x14] -2
    pTemplate->mfMarginRight  = KF_DEFAULT_TEXT_MARGIN_HIGH;     // [c:0x18]  2
    pTemplate->mfMarginBottom = KF_DEFAULT_TEXT_MARGIN_HIGH;     // [c:0x1C]  2
    pTemplate->mnAuthoredReserved0 = 0;                          // [c:0x24]
    pTemplate->mnAuthoredReserved1 = 0;                          // [c:0x28]
    pTemplate->mfFontSize = KF_DEFAULT_TEXT_FONT_SIZE;           // [c:0x2C] 12.0
    pTemplate->mnAuthoredReserved2 = 0;                          // [c:0x30]
    pTemplate->mnAuthoredReserved3 = 0;                          // [c:0x34]
    pTemplate->mnAuthoredReserved4 = 0;                          // [c:0x38]
    pTemplate->mpDefaultText = nullptr;                          // [c:0x3C]
    pTemplate->mnAuthoredReserved5 = 0;                          // [c:0x40]
    // No default glyph resolved yet (the X360 seeds the index to -1 before the walk).
    pTemplate->mnDefaultGlyphIndex = -1;                         // [c:0x20]

    // Resolve the default font from the level-0 movie. The X360 reaches the
    // font-list-bearing character through the level-0 animation node's spine, all
    // named members: node -> char-inst -> render-item -> character -> fixup-link.
    AptCIH* pLevel0 = AptGetAnimationAtLevel(0);
    AptCharacter* pFontOwner =
        pLevel0->mpCharacterInst->mpRenderItem->mpCharacter->mpFixupLink;

    // Take the default font character + its default glyph index from that owner's
    // serialised glyph list (deferred helper -- the glyph-list record has no C++
    // home). The font character is stored in the template's mpFixupLink slot (the
    // AptCharacter back-link), the glyph index in mnDefaultGlyphIndex.
    pTemplate->mpFixupLink =
        AptResolveDefaultTextFont(pFontOwner, &pTemplate->mnDefaultGlyphIndex);

    // Mark the template as a text character (per-type flag in the low half of the
    // packed ref/flags word; the X360 OR-s 0x8000 into that halfword).
    pTemplate->mnRefAndFlags |= KU_TEXT_CHARACTER_FLAG;

    // The X360 returns the level-0 animation node it walked.
    return pLevel0;
}

// ---------------------------------------------------------------------------
// Shutdown @0x82AE2FA0 -- free both cached default templates + null the statics.
// ---------------------------------------------------------------------------
void AptCharacterHelper::Shutdown()
{
    if (spDefaultTextCharacter)
    {
        gpNonGCPoolManager->Deallocate(spDefaultTextCharacter, sizeof(AptCharacterDynamicText));
    }
    if (spDefaultMovieCharacter)
    {
        // The movie template is the same 68-byte block size as the text template.
        gpNonGCPoolManager->Deallocate(spDefaultMovieCharacter, sizeof(AptCharacterDynamicText));
    }

    spDefaultTextCharacter  = nullptr;
    spDefaultMovieCharacter = nullptr;
}
