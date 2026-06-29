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

    // --- .apt load resolution (D) ------------------------------------------
    // @0x80EEC4 -- resolve this (serialised) movie root against the load base:
    // clear the resolve scratch + run the recursive Fixup. Returns the root.
    AptCharacterAnimation* Resolve(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
    // @0x80E9E4 -- relocate/resolve the serialised character tree. On x64 the strategy
    // depends on the .apt's pointer size (AptConstFile::GetPointerSizeBytes): this is the
    // DISPATCHER (body in AptCharacterAnimation.cpp) over the two paths below.
    AptCharacterAnimation* Fixup(void* pBase, struct AptConstFile* pConstFile, void* pBlock);

    // --- the Fixup pointer-size dual-path (the per-record walk is shared) ------
    // 8-byte .apt: the faithful console relocation -- add the load base to each record's
    // file-relative pointer slot IN PLACE (a 64-bit base fits an 8-byte slot) and use the
    // blob directly as the runtime root. FLAG: scaffold -- the per-record slot map (which
    // fields are pointers, per character type) is the shared follow-on.
    AptCharacterAnimation* FixupInPlace(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
    // 4-byte .apt: the x64 fork -- TRANSCODE the 32-bit serialised records into native
    // 64-bit runtime structs (in-place cannot work; a 64-bit pointer won't fit a 32-bit
    // slot). FLAG: the deep data-side keystone -- the per-record transcode is the follow-on.
    AptCharacterAnimation* FixupTranscode(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
};
