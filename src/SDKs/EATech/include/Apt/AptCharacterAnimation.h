#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterAnimation: a loaded .apt movie's root.
//
// It owns the movie's character table (the AptCharacters), its import table (the
// other .apt files it pulls characters from), and an init-indicator list. It is
// produced by AptCharacterAnimation::Fixup (@0x80E9E4) from the serialized .apt.
//
// 32-BIT-FORMAT / x64-RUNTIME FORK (see the project Apt notes): the .apt file is
// 32-bit and the console Fixup relocates it IN-PLACE -- impossible on x64. So the
// RUNTIME form below is native 64-bit with NAMED members (the PC .apt loader
// transcodes the file into it), and these accessors reconstruct the console
// LOGIC against the named members + sizeof (not the file's 16/8/4-byte strides).
// Console dword positions are noted for traceability.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF:
//   IsImport          @0x7E3738   UnmapCharacter   @0x7E36CC
//   ClearCharacterList@0x810DEC   IncCharacterList @0x80E790
//   (ResetInitIndicators @0x7E37A4, GetIDFromImportFile @0x7E77FC, Fixup/Resolve
//    deferred -- they reach into the AptMovie timeline / the imported export table.)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacter.h"    // AptCharacter (table entries)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"     // AptFilePtr (import + IncCharacterList)

struct AptFile;

// One import: pull character `mnId` from `mpImportFileName`'s export `mpClassName`.
// Console record = 16 bytes {name@+0, class@+4, id@+8, AptFilePtr@+12}.
struct AptImportEntry
{
    const char* mpImportFileName;   // +0  the .apt to load
    const char* mpClassName;        // +4  the exported symbol to import from it
    int32_t     mnId;               // +8  the local character id this maps to
    AptFile*    mpFile;             // +12 the loaded import (an AptFilePtr's AptFile*)
};

// One init indicator. Console record = 8 bytes {ptr@+0, indicator@+4}; the
// indicator is sign-flagged (negated while pending, restored by
// ResetInitIndicators). Layout decoded; methods that use it are deferred.
struct AptInitEntry
{
    void*   mpInitObject;   // +0
    int32_t mnIndicator;    // +4
};

struct AptCharacterAnimation
{
    // Console dwords [3]/[4] -- the character table.
    int32_t        mnCharacterCount;
    AptCharacter** mpCharacterTable;

    // Console dwords [8]/[9] -- the import table.
    int32_t         mnImportCount;
    AptImportEntry* mpImportTable;

    // Console dwords [10]/[11] -- the init-indicator list.
    int32_t       mnInitListCount;
    AptInitEntry* mpInitList;

    // Console dword [12] -- cleared by Resolve; the AptMovie::resolve scratch.
    void*         mpResolveState;

    // (More fields -- console [0-2],[5-7],[12+] used by Fixup/Resolve/AptMovie --
    //  are added as decoded; this runtime form is transcoded, so its layout is
    //  ours, populated by the loader.)

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
    // @0x80E9E4 -- relocate/resolve the serialised character tree. On x64 the strategy
    // depends on the .apt's pointer size (AptConstFile::GetPointerSizeBytes): this is the
    // DISPATCHER (body in AptCharacterAnimation.cpp) over the two paths below.
    AptCharacterAnimation* Fixup(void* pBase, struct AptConstFile* pConstFile, void* pBlock);

    // --- the Fixup pointer-size dual-path (the per-record walk is shared) ------
    // Both run the same console character/import/init walk (FixupWalk in the .cpp);
    // they differ only in the file slot WIDTH the relocation operates on.
    // 8-byte .apt: the faithful console relocation widened to 8-byte slots -- add the
    // load base to each record's file-relative pointer slot IN PLACE (a 64-bit base
    // fits an 8-byte slot) and use the blob directly as the runtime root.
    AptCharacterAnimation* FixupInPlace(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
    // 4-byte .apt (the console default): the same walk relocating the 32-bit slots in
    // place. FLAG (x64 fork): the fully faithful x64 form would transcode each 32-bit
    // record into a native 64-bit struct, but the deferred in-place callees
    // (AptMovie::resolve / AptLoader::Load / AptCharacter::SetupCharacter) still consume
    // the 32-bit blob -- so this reproduces the verbatim console in-place relocation
    // until those land (see the .cpp).
    AptCharacterAnimation* FixupTranscode(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
};
