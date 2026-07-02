#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterAnimation: a loaded .apt movie's root.
//
// It owns the movie's character table (the AptCharacters), its import table (the
// other .apt files it pulls characters from), and an init-indicator list. It is
// produced by AptCharacterAnimation::Fixup (@0x82AFF268) from the serialized .apt.
//
// SERIALIZED 64-BIT DEF-BASE / IN-PLACE (the faithful GUIAPT64 "1:7:8" format).
// The struct below IS the on-disk movie def-base overlaid directly (the loader
// relocates it IN PLACE via Fixup, then the runtime reads it through this struct
// -- AptMovieCharacter_GetAnimation returns `char + KU_AptEmbeddedMovieOff`, a
// pointer straight into the 64-bit file blob). So the member OFFSETS are the
// VERIFIED 64-bit serialized offsets (vs TITLE_SCREEN02.bundle), NOT a transcoded
// runtime layout: charCount@0x18, charTable@0x20, importCount@0x34, importTable@
// 0x38, initCount@0x40, initList@0x48 -- exactly the byte offsets Fixup already
// relocates. (The old code laid these at a transcoded +0x0C/+0x10/+0x20/... which
// mismatched the def-base Fixup produced; redefining the struct to the serialized
// layout makes the named-member methods read the correct offsets.)
//
// The 32-bit console record has the SAME field order at half the pointer width
// (frameCount@0, framesOffset@4, charCount@0xC, charTable@0x10, importCount@0x20,
// importTable@0x24, initCount@0x28, initList@0x2C); those console dword positions
// are noted for traceability of the ARTIST/PS3 asm this is decompiled from.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF:
//   IsImport          @0x7E3738   UnmapCharacter   @0x7E36CC
//   ClearCharacterList@0x810DEC   IncCharacterList @0x80E790
//   (ResetInitIndicators @0x7E37A4, GetIDFromImportFile @0x7E77FC, Fixup/Resolve
//    reach into the AptMovie timeline / the imported export table.)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // offsetof (the layout static_asserts below)
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacter.h"    // AptCharacter (table entries)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"     // AptFilePtr (import + IncCharacterList)

struct AptFile;

// One import: pull character `mnId` from `mpImportFileName`'s export `mpClassName`.
// Serialized 64-bit record = 32 bytes {movieName@0, importName@8, id@0x10, AptFile
// @0x18} -- the widened GUIAPT64 form of the 16-byte console record
// {name@+0, class@+4, id@+8, AptFilePtr@+12}. On x64 the two 8-byte pointers +
// the padded int naturally lay the fields at the serialized stride 0x20.
struct AptImportEntry
{
    const char* mpImportFileName;   // +0x00  the .apt to load
    const char* mpClassName;        // +0x08  the exported symbol to import from it
    int32_t     mnId;               // +0x10  the local character id this maps to
    AptFile*    mpFile;             // +0x18  the loaded import (an AptFilePtr's AptFile*)
};
// Lock the serialized 64-bit import-entry stride/offsets (the Fixup walk indexes
// mpImportTable by 0x20 and reads name@0/class@8/id@0x10/AptFile@0x18).
static_assert(offsetof(AptImportEntry, mpImportFileName) == 0x00, "AptImportEntry.movieName@0");
static_assert(offsetof(AptImportEntry, mpClassName)      == 0x08, "AptImportEntry.importName@8");
static_assert(offsetof(AptImportEntry, mnId)             == 0x10, "AptImportEntry.id@0x10");
static_assert(offsetof(AptImportEntry, mpFile)           == 0x18, "AptImportEntry.AptFile@0x18");
static_assert(sizeof(AptImportEntry)                     == 0x20, "AptImportEntry serialized stride 0x20");

// One init indicator. Serialized 64-bit record = 16 bytes {ptr@0, indicator@8} --
// the widened GUIAPT64 form of the 8-byte console record {ptr@+0, indicator@+4};
// the indicator is sign-flagged (negated while pending, restored by
// ResetInitIndicators).
struct AptInitEntry
{
    void*   mpInitObject;   // +0x00
    int32_t mnIndicator;    // +0x08
};
static_assert(offsetof(AptInitEntry, mpInitObject) == 0x00, "AptInitEntry.ptr@0");
static_assert(offsetof(AptInitEntry, mnIndicator)  == 0x08, "AptInitEntry.id@8");
static_assert(sizeof(AptInitEntry)                 == 0x10, "AptInitEntry serialized stride 0x10");

struct AptCharacterAnimation
{
    // The serialized 64-bit movie def-base, overlaid in place. Offsets are the
    // VERIFIED GUIAPT64 byte offsets (vs TITLE_SCREEN02.bundle); explicit padding
    // holds the not-yet-decoded slots so offsetof() matches the file exactly.
    // Console dword indices are noted for traceability of the ARTIST/PS3 asm.

    int32_t        mnFrameCount;      // +0x00  console [0]  frameCount
    uint8_t        mPad04[0x04];      // +0x04  (pad to the +0x08 pointer slot)
    void*          mpFrames;          // +0x08  console [1]  framesOffset (the timeline; AptMovie::resolve)
    void*          mpUnk10;           // +0x10  console [2]  (undecoded)
    int32_t        mnCharacterCount;  // +0x18  console [3]  charCount
    uint8_t        mPad1C[0x04];      // +0x1C  (pad to the +0x20 pointer slot)
    AptCharacter** mpCharacterTable;  // +0x20  console [4]  charTable
    int32_t        mnScreenW;         // +0x28  console [5]  screen width
    int32_t        mnScreenH;         // +0x2C              screen height
    int32_t        mnResolveScratch;  // +0x30  console [6]/[12]  ms / the AptMovie::resolve scratch (Resolve clears it)
    int32_t        mnImportCount;     // +0x34  console [8]  importCount
    AptImportEntry* mpImportTable;    // +0x38  console [9]  importTable (stride 0x20)
    int32_t        mnInitListCount;   // +0x40  console [10] initCount
    uint8_t        mPad44[0x04];      // +0x44  (pad to the +0x48 pointer slot)
    AptInitEntry*  mpInitList;        // +0x48  console [11] initList (stride 0x10)
    // +0x50 -- the resolved-value count _parseStream accumulates during Fixup (the XB1
    // CompleteLoad sub_1408348B0 zeroes `*(def+80)` then threads `def+80` as the resolve
    // twin sub_1408567D0's a4; every parse of an action stream ++s it per resolved entry).
    int64_t        mnParsedValueCount; // +0x50  xb1 [a4 of the resolve walk]

    // @0x7E3738 -- index of the import whose id == nId, or -1.
    int32_t IsImport(int32_t nId);

    // @0x7E36CC -- index of pCharacter in the character table, or -1.
    int32_t UnmapCharacter(AptCharacter* pCharacter);

    // @0x810DEC -- drop a character reference on every table entry (except the
    // type-9 imports, which the importing movie still owns).
    void ClearCharacterList() const;

    // @0x80E790 -- add a character reference to every table entry, binding each
    // un-bound character to the given animation file.
    void IncCharacterList(AptFilePtr filePtr) const;

    // @0x7E37A4 (X360 @0x82AD92A8) -- restore the init-indicator list: walk the
    // init indicators and un-flag (restore the sign of) the ones that were marked
    // pending. Called when an animation instance is torn down before its init
    // actions have all run (AptCharacterAnimationInst::~AptCharacterAnimationInst).
    // The X360 body returns `this` (a chaining return the callers ignore); modelled
    // as `void` to match the family's list-walk methods. BODY in its own TU
    // (class:AptCharacterAnimation); declared here so callers compile.
    void ResetInitIndicators();

    // @0x82ADE998 -- resolve the character id that import-table slot nImportIndex's
    // class name names in the imported movie's export table (inline strcmp, like
    // AptFile::FindExport); returns -1 when the imported movie has no exports or the
    // name is absent. Body in AptCharacterAnimation.cpp.
    int32_t GetIDFromImportFile(int32_t nImportIndex);

    // --- init-action execution (the __Packages bootstrap) ------------------
    // @0x82AEA8C8 -- ExportClassDefinitionAssets: for each frame-label whose name
    // begins with "__Packages." (and is still pending), run the matching frame's
    // ActionScript class-definition stream once, then mark the label consumed.
    // pA2 is the AptCIH-at-level (r4); the asm walks the serialised label table.
    // Returns the last sub-result (callers ignore it). Body in the .cpp.
    void* ExportClassDefinitionAssets(void* pA2);

    // @0x82AEE2D8 -- ExecuteInitAction: find the type-8 import whose id == nId,
    // run its class-definition assets, then run that init slot's action stream and
    // mark it consumed (negate the id). pA2 is the AptCIH-at-level (r4); nId is r5.
    // Body in the .cpp.
    void* ExecuteInitAction(void* pA2, int32_t nId);

    // @0x82AF4340 -- ExecuteInitActions: locate the init-list bucket for character
    // nId (directly, or via GetIDFromImportFile through the import chain), run every
    // pending type-3 init command in it, then run the import-resolved init action.
    // pA2 is the AptCIH (r4); nId is r5. Body in the .cpp.
    void* ExecuteInitActions(void* pA2, int32_t nId);

    // @0x82AF77B0 -- Unresolve: the inverse of Fixup. Releases the per-character
    // animation refs, un-relocates every record's pointer slot (subtracting the load
    // base), tears down the imports, and stamps the freed characters. nBase is the
    // load base (r4). Returns the last sub-result. Body in the .cpp.
    void* Unresolve(int32_t nBase);

    // @0x82AD92A8 (PS3 @0x7E37A4) -- restore the init-indicator list (declared above
    // as ResetInitIndicators); body now in the .cpp.

    // --- .apt load resolution (D) ------------------------------------------
    // @0x80EEC4 -- resolve this (serialised) movie root against the load base:
    // clear the resolve scratch + run the recursive Fixup. Returns the root.
    AptCharacterAnimation* Resolve(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
    // @0x82AFF268 (PS3 @0xF44D40) -- relocate the serialised movie root IN PLACE against
    // the load base (the aptDataOffset memory address). SINGLE native-64-bit path (GUIAPT64
    // "1:7:8" only); the console dual-path/transcode is gone. Faithful body in the .cpp.
    AptCharacterAnimation* Fixup(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
};

// Lock the serialized 64-bit def-base offsets Fixup relocates + the runtime reads
// (VERIFIED vs TITLE_SCREEN02.bundle). A member reorder that desynced these from
// the file blob would silently break every named-member accessor + the Fixup walk.
static_assert(offsetof(AptCharacterAnimation, mnCharacterCount) == 0x18, "AptCharacterAnimation.charCount@0x18");
static_assert(offsetof(AptCharacterAnimation, mpCharacterTable) == 0x20, "AptCharacterAnimation.charTable@0x20");
static_assert(offsetof(AptCharacterAnimation, mnImportCount)    == 0x34, "AptCharacterAnimation.importCount@0x34");
static_assert(offsetof(AptCharacterAnimation, mpImportTable)    == 0x38, "AptCharacterAnimation.importTable@0x38");
static_assert(offsetof(AptCharacterAnimation, mnInitListCount)  == 0x40, "AptCharacterAnimation.initCount@0x40");
static_assert(offsetof(AptCharacterAnimation, mpInitList)       == 0x48, "AptCharacterAnimation.initList@0x48");
