#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterHelper: the dynamically-created-character factory.
//
// A stateless helper (all static methods) that the ActionScript runtime uses to
// lazily build + tear down the two shared DEFAULT character templates that back
// AS-created display objects:
//   * the default DYNAMIC-TEXT character  -- backs MovieClip.createTextField
//                                            (built by CreateTextCharacterInst)
//   * the default MOVIE-CLIP character    -- backs MovieClip.createEmptyMovieClip
//                                            (built by the sibling
//                                             CreateMovieCharacterInst)
// Both are AptCharacter subtype singletons allocated from the non-GC Apt pool and
// freed together by Shutdown.
//
// SHAPE + BODIES from the X360 ARTIST.XEX (no Feb-2007 / DecFIGS source exists for
// this helper). The two bodies attested in the X360 ledger:
//     AptCharacterHelper::CreateTextCharacterInst @ 0x82B010C0
//     AptCharacterHelper::Shutdown                @ 0x82AE2FA0
// (the sibling CreateMovieCharacterInst @0x82B0BC.. / PS3 @0xF56FC4 -- the
//  off_8324E49C builder -- is bodied in AptCharacterHelper.cpp too.)
//
// MODULE-STATIC SINGLETONS: the X360 caches the two templates in the .data globals
// off_8324E498 (text) / off_8324E49C (movie). They are modelled here as the class
// statics spDefaultTextCharacter / spDefaultMovieCharacter (both written by their
// builders in this TU). No raw dword_XXXX pokes: access is by the named static.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

struct AptCharacter;              // base of the cached templates
struct AptCharacterDynamicText;   // the text template's concrete type (off_8324E498)
struct AptCIH;                    // the level-0 animation node CreateTextCharacterInst walks/returns

class AptCharacterHelper
{
public:
    // CreateTextCharacterInst @0x82B010C0 -- lazily build the shared default
    // DYNAMIC-TEXT character template (spDefaultTextCharacter): allocate it from
    // the non-GC Apt pool, zero it, then stamp the authored text defaults (type
    // tag, margins, default colour/font size, the text-character flag) and resolve
    // the level-0 movie's default font into it. Idempotent at the call sites (they
    // only call it when the template is still null). Returns the level-0 animation
    // node the font was resolved from (the X360 returns the AptCIH* it walked).
    //
    // NOTE: the X360 caller (sMethod_createTextField) passes the field's text
    // string in r3, but the prologue here never reads it -- the template is the
    // field-independent default -- so the recovered signature takes no parameter
    // (the asm ignores the incoming r3). The text string is applied to the
    // instance AFTER InsertChild, not to this shared template.
    static AptCIH* CreateTextCharacterInst();

    // CreateMovieCharacterInst @0x82B0BC.. (PS3 @0xF56FC4) -- the sibling lazy builder
    // for the shared default empty MOVIE-CLIP character template (spDefaultMovieCharacter,
    // type tag 5), used by AS createEmptyMovieClip: allocate the 68-byte block from the
    // non-GC Apt pool, zero it, stamp the movie type tag, resolve the level-0 movie's
    // default character into mpFixupLink, and set the per-type flag. Idempotent at the
    // call site (only called when the template is still null). Body in the .cpp.
    //
    // RECONCILE: returns the level-0 animation node it walked (the X360/PS3 return the
    // AptCIH*, exactly like the sibling CreateTextCharacterInst); the lone caller
    // (sMethod_createEmptyMovieClip) ignores the return.
    static AptCIH* CreateMovieCharacterInst();

    // Shutdown @0x82AE2FA0 -- free both cached default templates (text + movie)
    // back to the non-GC Apt pool (each is a 68-byte block) and null the statics.
    static void Shutdown();

    // ---- the cached default-character templates (X360 .data singletons) --------
    // spDefaultTextCharacter  == off_8324E498 (built by CreateTextCharacterInst).
    // spDefaultMovieCharacter == off_8324E49C (built by the sibling
    //   CreateMovieCharacterInst; referenced here only so Shutdown frees it).
    // Both null until first use; reset to null by Shutdown / AptValueInitialize.
    static AptCharacterDynamicText* spDefaultTextCharacter;
    static AptCharacter*            spDefaultMovieCharacter;   // written by CreateMovieCharacterInst (this TU)
};

// ---------------------------------------------------------------------------
// Fetch the runtime animation node currently mounted at display level `nLevel`
// (the X360 _AptGetAnimationAtLevel @0x82B00788, which lazily creates the level-0
// root CIH on first use). HOMED in AptCharacterHelper.cpp; null when no movie is
// mounted.
// ---------------------------------------------------------------------------
AptCIH* AptGetAnimationAtLevel(int nLevel);

// The NON-CREATING half of AptGetAnimationAtLevel: the same root-display-list
// search for the node mounted at display level nLevel, WITHOUT the lazy-create
// tail. Liveness/composition QUERIES must not mint level nodes; the search is
// exactly the loop AptGetAnimationAtLevel opens with. Null when nothing is
// mounted at the level (or no target/director exists yet).
AptCIH* AptFindAnimationAtLevel(int nLevel);

// ---------------------------------------------------------------------------
// Resolve the default font for a freshly-created text field from `pFontOwner` (the
// level-0 movie's font-list-bearing character, reached by named members:
// AptGetAnimationAtLevel(0)->mpCharacterInst->mpRenderItem->mpCharacter->mpFixupLink).
//
// HOMED in AptCharacterHelper.cpp: the walk is the owner's embedded movie def-base
// (AptGetMovieCharacterAnimation -> the serialized-64 AptCharacterAnimation, whose
// charCount/charTable offsets are static_assert-locked) -- take charTable[0] as the
// default font handle and find the index of the first type-3 (font) character,
// returned via *pnDefaultGlyphIndex (left at -1 when none). Returns the default
// font character (stored into the template's mpFixupLink slot), or null.
// ---------------------------------------------------------------------------
AptCharacter* AptResolveDefaultTextFont(AptCharacter* pFontOwner, int32_t* pnDefaultGlyphIndex);
