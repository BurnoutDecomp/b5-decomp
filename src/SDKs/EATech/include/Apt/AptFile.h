#pragma once

// ===========================================================================
// EATech Apt -- AptFile: the loader's per-movie file handle / load-request.
//
// AptLoader::Load(fileName) registers one of these (ref-counted via
// AptSharedPtr<AptFile>) and returns a shared pointer to it. It starts life in
// the "requested" state and is later filled in by the async load-completion path
// (AptLoader::Update/notify/CompleteLoad) once the .apt streams in and parses --
// at which point mpData points at the loaded movie root and mnState advances to
// "loaded". The shared count is the FIRST word (the AptSharedPtr primitives
// lwarx/stwcx. on *pData).
//
// SHAPE recovered from the PS3 EXTERNAL ELF (full mangled Apt symbol table):
//   - AptLoader::Load        @0x80CFF4 builds it (the field-by-field init)
//   - AptFile::~AptFile      @0x812AD4 tears it down (reveals the owned bits)
//   - AptFile::FindExport    @0x7E3920 reads mpData's export table
//   - AptSharedPtrDelete     @0x80CC64 frees the 28-byte block from the non-GC pool
// Console layout is 28 bytes (7 x 32-bit); the named members below let the x64
// PC build compute the correct (wider) sizeof/offsets -- the Apt engine is
// written with 32-bit pointers + hard-coded offsets, so reconstructing with
// named members (not the literal console offsets) is the pervasive PC-port FLAG.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC mFileName member

struct AptCharacter;   // FindExport returns the exported character (pointer only)

struct AptFile
{
    // +0 (console): the AptSharedPtr<AptFile> reference count. MUST stay the
    // first member -- AptSharedPtrIncRef/DecRef atomically mutate *(int*)this.
    int32_t   mnRefCount;

    // +4: the movie's file name (ref-counted; released by ~AptFile via the
    // EAStringC member destructor, matching the asm's explicit
    // DecreaseInternalRefCount(this+4)).
    EAStringC mFileName;

    // +8: load state. 1 = requested (just registered, not yet streamed);
    // 4/5 = loaded (AptLoader::IsLoaded accepts these); 3..6 = "has resolved
    // data" (the ~AptFile teardown branch). Advanced by the async completion path.
    int32_t   mnState;

    // +12: set to 1 at creation. (FLAG: precise role not yet pinned -- a
    // generation/owner flag; preserved verbatim from the Load init.)
    int32_t   mnField12;

    // +16: the AptCharacterAnimation resolve context (0 until loaded); passed to
    // AptCharacterAnimation::Unresolve in ~AptFile.
    void*     mpResolveContext;

    // +20: the loaded movie root (the parsed AptData; 0 until loaded). Holds the
    // export table (FindExport) and the embedded AptCharacterAnimation.
    void*     mpData;

    // +24: the raw loaded data block (0 until loaded); freed by the AptData free
    // hook in ~AptFile.
    void*     mpDataBlock;

    ~AptFile();

    // FindExport @0x82AD9DF0 (PS3 @0x7E3920) -- linear-search the loaded movie's export
    // table for the export named pName; returns the exported AptCharacter, or null if the
    // name is absent (or the movie is not loaded). Called by AptCharacterAnimation::Link.
    AptCharacter* FindExport(const char* pName) const;

    // isFileImported @0x82AEB1C8 -- true when (*ppCandidate)'s file name appears in this
    // movie's import table. CONSUMES the candidate either way: disposes the AptFile and
    // nulls *ppCandidate (matching the asm). Called by AptLinker::isFileImported.
    bool isFileImported(AptFile** ppCandidate) const;
};
