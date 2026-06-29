// ===========================================================================
// EATech Apt -- AptFile::~AptFile.   PS3 EXTERNAL @0x812AD4 (D2), 0x80CBBC (D1).
//
// DECOMPILED from the PS3 External ELF. Teardown order (matching the asm):
//   1. unregister this file's weak node from the current target's loader
//      (GetTarget()->loader->Invalidate(this));
//   2. if the async load already resolved (mnState 3..6 with data), unresolve the
//      embedded character animation and free the loaded data block;
//   3. release the file name (the EAStringC member's own destructor -- the
//      asm's explicit DecreaseInternalRefCount(this+4)).
//
// AptSharedPtrDelete calls this, then frees the AptFile block.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"   // GetTarget / AptTarget_GetLoader / AptLoader::Invalidate
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // AptImportEntry (import-table record) + AptCharacter
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"            // AptSharedPtr<AptFile>::Dispose

// ---------------------------------------------------------------------------
// FLAG (homed by their own engine TUs; reached ONLY once the async load has
// resolved, which the request layer cannot do yet). Routed through hooks rather
// than the literal console offsets/fn-ptr so the x64 layout stays correct:
//   AptFile_UnresolveAnimation -> AptCharacterAnimation::Unresolve((mpData+16),
//                                 mpResolveContext)   @0x80C3C4
//   AptFile_FreeLoadedBlock    -> the loaded-block free via off_1059C66C
// ---------------------------------------------------------------------------
void AptFile_UnresolveAnimation(void* pLoadedData, void* pResolveContext);
void AptFile_FreeLoadedBlock(void* pDataBlock);

AptFile::~AptFile()
{
    // 1. Unregister from the current target's loader (skipped during bring-up
    //    while GetTarget() is null -- see the FLAG in AptLoader.h).
    if (AptTarget* pTarget = GetTarget())
    {
        if (AptLoader* pLoader = AptTarget_GetLoader(pTarget))
            pLoader->Invalidate(this);
    }

    // 2. Loaded-data teardown (only after the async load resolved). FLAG: the
    //    parser + completion path that set mnState/mpData are a follow-on, so
    //    this branch is currently unreachable and its ops are extern hooks.
    if (mnState >= 3 && mnState <= 6 && mpData)
    {
        AptFile_UnresolveAnimation(mpData, mpResolveContext);
        AptFile_FreeLoadedBlock(mpDataBlock);
    }

    // 3. mFileName (the EAStringC member) is released by its destructor, which
    //    the compiler runs after this body -- the faithful equivalent of the
    //    asm's DecreaseInternalRefCount(this+4).
}

// ---------------------------------------------------------------------------
// AptFile::mpData points at the loaded + resolved .apt movie data root. The X360
// ARTIST AptFile accessors below reveal the table set the request layer reads; the
// embedded AptCharacterAnimation sits at the console root+0x10 (AptConstFile: "the
// embedded AptCharacterAnimation sits at root+16").
//
// FLAG: PARTIAL + x64-native. The 32-bit .apt is transcoded by the PC loader into a
// native runtime form, so this view names only the AptFile-touched members (widths are
// x64; the [c:] notes are the console byte offsets the asm uses). Two reconcile points
// with class:AptCharacterAnimation (Adriwin):
//   * mpCharacterTable / mnImportCount / mpImportTable ARE that struct's members (it is
//     the object embedded at root+0x10).
//   * the EXPORT table here ([c:+0x38]/[c:+0x3C] == animation+0x28/0x2C) is the console
//     slot the PS3-decoded AptCharacterAnimation header currently labels the init-
//     indicator list; the X360 FindExport asm walks it as a {name,id} export table -- a
//     likely PS3/X360 branch divergence (see the ps3-reconciliation campaign).
// ---------------------------------------------------------------------------
struct AptExportEntry            // console record = 8 bytes {name@+0, id@+4}
{
    const char* mpName;          // the exported symbol name
    int32_t     mnCharacterId;   // index into mpCharacterTable
};

struct AptMovieData
{
    AptCharacter**  mpCharacterTable;   // [c:+0x20]
    int32_t         mnImportCount;      // [c:+0x30]
    AptImportEntry* mpImportTable;      // [c:+0x34]  (16-byte AptImportEntry records)
    int32_t         mnExportCount;      // [c:+0x38]
    AptExportEntry* mpExportTable;      // [c:+0x3C]
};

// ---------------------------------------------------------------------------
// FindExport @0x82AD9DF0 -- linear scan of the export table, comparing pName to each
// export's name with an inline strcmp; on a full match return the character the export
// names (mpCharacterTable[entry.mnCharacterId]). Null when the table is empty / no match.
// ---------------------------------------------------------------------------
AptCharacter* AptFile::FindExport(const char* pName) const
{
    const AptMovieData* pMovie = static_cast<const AptMovieData*>(mpData);

    const int32_t nExportCount = pMovie->mnExportCount;
    if (nExportCount <= 0)
        return nullptr;

    for (int32_t iExport = 0; iExport < nExportCount; ++iExport)
    {
        // Inline strcmp -- the asm loads zero-extended (unsigned) bytes and walks until
        // the SEARCH name's terminator, breaking early on any mismatch.
        const char* pSearch = pName;
        const char* pExport = pMovie->mpExportTable[iExport].mpName;
        int nDiff;
        do
        {
            nDiff = static_cast<unsigned char>(*pSearch) - static_cast<unsigned char>(*pExport);
            if (*pSearch == '\0')
                break;
            ++pSearch;
            ++pExport;
        }
        while (nDiff == 0);

        if (nDiff == 0)   // names matched up to a shared terminator
            return pMovie->mpCharacterTable[pMovie->mpExportTable[iExport].mnCharacterId];
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// isFileImported @0x82AEB1C8 -- scan this movie's import table for an entry whose file
// name equals the candidate's, then DISPOSE the candidate (consuming the passed
// reference) regardless of the result. Returns whether the file was an import.
// ---------------------------------------------------------------------------
bool AptFile::isFileImported(AptFile** ppCandidate) const
{
    const AptMovieData* pMovie = static_cast<const AptMovieData*>(mpData);

    bool bImported = false;
    const int32_t nImportCount = pMovie->mnImportCount;   // (asm re-reads this each pass; mpData is stable)
    for (int32_t iImport = 0; iImport < nImportCount; ++iImport)
    {
        // Wrap the import's file name in a temporary EAStringC and compare it to the
        // candidate's name. The temp's ctor InitFromBuffer's the buffer and its dtor
        // DecreaseInternalRefCount's it -- exactly the asm's per-iteration InitFromBuffer
        // / compare / DecreaseInternalRefCount bracket.
        // FLAG: the asm comparator (IDA-unnamed sub @...0040_0, (EAStringC,EAStringC) ->
        // bool) is the EAStringC equality operator.
        EAStringC importName(pMovie->mpImportTable[iImport].mpImportFileName);
        bImported = (importName == (*ppCandidate)->mFileName);
        if (bImported)
            break;
    }

    // Consume the candidate either way: dispose the AptFile and null the slot (the asm
    // does this on both the matched and the exhausted exit).
    AptFile* pConsumed = *ppCandidate;
    *ppCandidate = nullptr;
    AptSharedPtr<AptFile>::Dispose(pConsumed);
    return bImported;
}
