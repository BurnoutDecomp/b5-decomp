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

#include <cstddef>   // offsetof (AptMovieData native-8 offset static_asserts)
#include <cstdint>

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC mFileName member

struct AptCharacter;   // FindExport returns the exported character (pointer only)
struct AptImportEntry; // AptMovieData::mpImportTable element (defined in AptCharacterAnimation.h)

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

// ---------------------------------------------------------------------------
// MakeAptFile -- the AptFile::AptFile ctor @0x82AE6268 wrapped as the loader/linker
// factory (pool-allocate 28 bytes, then construct). The X360 ctor is folded (no own
// export symbol); body in AptFile.cpp, decompiled from the decrypted ARTIST.XEX.
// pName is the .apt file name (an EAStringC, copied into mFileName with a refcount
// bump). Returns the constructed AptFile (the ctor's `this`).
// ---------------------------------------------------------------------------
AptFile* MakeAptFile(void* pMem, EAStringC* pName);

// ---------------------------------------------------------------------------
// The loaded .apt movie root view (AptFile::mpData points here once loaded). This
// names only the AptFile/AptLoader-touched members; the [c:] notes are the console
// byte offsets the asm uses (widths are x64, the PC-port FLAG). RECONCILE POINT
// with class:AptCharacterAnimation: mpCharacterTable / mnImportCount / mpImportTable
// are that struct's members (the AptCharacterAnimation embedded at root+0x20 on
// native-8); the export table is the def-base initList slot the X360 FindExport /
// GetIDFromImportFile asm walks as a {name,id} export table (the PS3 header labels
// it the init-indicator list -- a PS3/X360 branch divergence).
//
// NATIVE-8 LAYOUT (2026-07-01): AptFile::mpData points at the ROOT CHARACTER HEADER
// (the type-9 record, sig@+0x08). The movie's AptCharacterAnimation def base is at
// root+0x20 (native-8 char header size), and every movie field below is DEF-BASE-
// RELATIVE (charCount@def+0x18, charTable@def+0x20, importCount@def+0x34,
// importTable@def+0x38, exportCount@def+0x40, exportTable@def+0x48 -- the same
// offsets AptCharacterAnimation::Fixup relocates). So from mpData (the root header)
// they sit at root+0x38 / +0x40 / +0x54 / +0x58 / +0x60 / +0x68. The struct is laid
// out with an explicit byte prefix + static_asserts so `pMovie->field` reads land at
// those exact native-8 offsets -- the widened form of the console 4-byte struct the
// asm walked (charTable@+0x20/importCount@+0x30/... in the console layout). This
// unblocks FindExport / GetIDFromImportFile / AllImportsAvailable / isFileImported /
// CancelPreloadedAnimation on the native-8 bundle (previously they read the console
// offsets off the ROOT header -- e.g. mnImportCount landed on the 0x09876543 sig).
// // FLAG (x64 native-8 fork): the def-base-relative offsets + the widened
// AptImportEntry(0x20)/AptExportEntry(0x10) strides are the converter's native-8 form.
// ---------------------------------------------------------------------------
struct AptExportEntry            // native-8 record = 16 bytes {name@+0, id@+8}
{
    const char* mpName;          // +0x00  the exported symbol name (relocated char*)
    int32_t     mnCharacterId;   // +0x08  index into mpCharacterTable
    int32_t     mnPad0C;         // +0x0C  (pad to the serialized 0x10 stride)
};
static_assert(sizeof(AptExportEntry) == 0x10, "AptExportEntry native-8 stride 0x10");

struct AptMovieData
{
    // The root character header (type@0, sig@+0x08, ...) -- mpData points here.
    char            _rootHeader[0x20];   // root+0x00 .. root+0x20 (the native-8 char header)
    // The embedded AptCharacterAnimation def base begins here (root+0x20). The leading
    // frame fields (frameCount@0x00 / mpFrames@0x08) are not used by this view.
    char            _defHead[0x18];      // def+0x00 .. def+0x18
    int32_t         mnCharacterCount;    // def+0x18  (root+0x38)
    int32_t         _pad3C;              // def+0x1C  (pad to the def+0x20 pointer slot)
    AptCharacter**  mpCharacterTable;    // def+0x20  (root+0x40)
    char            _defMid[0x0C];       // def+0x28..0x34 (screenW/H@0x28/0x2C, ms@0x30)
    int32_t         mnImportCount;       // def+0x34  (root+0x54)
    AptImportEntry* mpImportTable;       // def+0x38  (root+0x58, stride 0x20)
    int32_t         mnExportCount;       // def+0x40  (root+0x60)  (== initCount)
    int32_t         _pad64;              // def+0x44  (pad to the def+0x48 pointer slot)
    AptExportEntry* mpExportTable;       // def+0x48  (root+0x68, stride 0x10)  (== initList)
};
static_assert(offsetof(AptMovieData, mnCharacterCount) == 0x38, "AptMovieData.charCount@root+0x38");
static_assert(offsetof(AptMovieData, mpCharacterTable) == 0x40, "AptMovieData.charTable@root+0x40");
static_assert(offsetof(AptMovieData, mnImportCount)    == 0x54, "AptMovieData.importCount@root+0x54");
static_assert(offsetof(AptMovieData, mpImportTable)    == 0x58, "AptMovieData.importTable@root+0x58");
static_assert(offsetof(AptMovieData, mnExportCount)    == 0x60, "AptMovieData.exportCount@root+0x60");
static_assert(offsetof(AptMovieData, mpExportTable)    == 0x68, "AptMovieData.exportTable@root+0x68");
