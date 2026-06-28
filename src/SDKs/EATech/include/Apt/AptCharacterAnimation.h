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
};
