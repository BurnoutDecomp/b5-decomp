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

#include <new>   // placement new for MakeAptFile's EAStringC member construct

#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"   // GetTarget / AptLoader::Invalidate
#include "SDKs/EATech/include/Apt/AptTarget.h"   // AptTarget::GetLoader (full type for the member call)
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // AptImportEntry (import-table record) + AptCharacter
#include "SDKs/EATech/include/Apt/AptCIH.h"                  // KU_AptEmbeddedMovieOff (native-8 character-header size)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"            // AptSharedPtr<AptFile>::Dispose

// ---------------------------------------------------------------------------
// The loaded-data teardown the ~AptFile branch reaches once the async load has
// resolved. DECOMPILED from the PS3 EXTERNAL ~AptFile (D2 @0xF48E30); the
// register form is:
//   if ( (*(this+2) - 3) <= 3 && *(this+5) ) {           // mnState in 3..6 && mpData
//       AptCharacterAnimation::Unresolve(*(this+5) + 16, *(this+4));   // (mpData+0x10, mpResolveContext)
//       (*dword_1071B1F4)(*(this+6));                     // free hook on mpDataBlock
//   }
// Decomposed by the request-layer dossier into these two free helpers; bodied
// here against the named x64 members (console offsets in [c:] comments).
// ---------------------------------------------------------------------------

// The loaded-block free hook the Apt runtime installs at GUI bring-up. PS3
// dword_1071B1F4 (X360 off_1059C66C) is set to
// `&CgsGui::AptCallbackFile::FreeAnimation` by CgsGui::AptAux::ConstructApt -- the
// registered "free a loaded .apt data block" user-function.
typedef void (*AptFreeAnimationHook)(void* pDataBlock);
extern AptFreeAnimationHook gpAptFreeAnimationHook;   // dword_1071B1F4 / off_1059C66C
AptFreeAnimationHook gpAptFreeAnimationHook = nullptr;

// AptFile_UnresolveAnimation inlined at its call site in ~AptFile (see above).

// AptFile_FreeLoadedBlock -- hand the raw loaded-data block (mpDataBlock) back to the
// registered Apt free callback (PS3 (*dword_1071B1F4)(*(this+6))).
// FLAG PC-platform leaf: the host free-callback boundary (gpAptFreeAnimationHook, installed by AptAux::ConstructApt).
void AptFile_FreeLoadedBlock(void* pDataBlock)
{
    if (gpAptFreeAnimationHook != nullptr)   // installed by CgsGui::AptAux::ConstructApt
        gpAptFreeAnimationHook(pDataBlock);
}

AptFile::~AptFile()
{
    AptLifeLog("~AptFile   ", mFileName.GetBuffer(), this);   // [aptlife] opt-in witness

    // 1. Unregister from the current target's loader (null-guarded: a file torn
    //    down after the target instance is destroyed sees GetTarget() null).
    if (AptTarget* pTarget = GetTarget())
    {
        if (AptLoader* pLoader = pTarget->GetLoader())
            pLoader->Invalidate(this);
    }

    // 2. Loaded-data teardown (only after the async load resolved). mnState/mpData
    //    are set by AptLoader::CompleteLoad (driven through the homed host stream
    //    hook AptLoaderStartAsyncLoad -> AptCompleteAnimationAsyncLoad), so this
    //    branch is LIVE.
    if (mnState >= 3 && mnState <= 6 && mpData)
    {
        // Inverse of the load-time Fixup: run AptCharacterAnimation::Unresolve on the
        // AptCharacterAnimation embedded in the movie root (mpData), passing the load
        // base (mpResolveContext) as nBase.
        // PS3: AptCharacterAnimation::Unresolve(*(this+5) + 16, *(this+4)).
        //
        // ⚠️⚠️ THE LITERAL 16 WAS THE CONSOLE'S, LEFT ON A NATIVE-8 RECORD. mpData is
        // the movie ROOT CHARACTER HEADER, and the embedded AptCharacterAnimation def
        // base sits one character header past it: 0x10 on the 32-bit console, 0x20 on
        // the widened native-8 data this build ships (KU_AptEmbeddedMovieOff -- the
        // same constant AptCharacterAnimationInst's ctor, AptCIH::GetMovie,
        // AptCharacterHelper and the Fixup walk all already use for this exact step,
        // and AptCharacterAnimation.h's own layout banner pins charTable at def+0x20).
        // At the console offset the walk read charCount from def+0x08 and charTable
        // from def+0x10 -- i.e. it unresolved a movie that was not there, so no
        // imported character was ever released and no import's AptFile refcount ever
        // reached zero.
        AptCharacterAnimation* pAnimation = reinterpret_cast<AptCharacterAnimation*>(
            reinterpret_cast<char*>(mpData) + KU_AptEmbeddedMovieOff);   // [c:] *(this+5) + 16
        pAnimation->Unresolve(reinterpret_cast<intptr_t>(mpResolveContext));
        AptFile_FreeLoadedBlock(mpDataBlock);
    }

    // 3. mFileName (the EAStringC member) is released by its destructor, which
    //    the compiler runs after this body -- the faithful equivalent of the
    //    asm's DecreaseInternalRefCount(this+4).
}

// ---------------------------------------------------------------------------
// MakeAptFile -- AptFile::AptFile @0x82AE6268 (the constructor the loader/linker
// run over a pool-allocated 28-byte block). The X360 ctor is not exported as its
// own symbol (it folds into AptScriptFunctionByteCodeBlock::operator new's range);
// disassembled directly from the decrypted ARTIST.XEX @0x82AE6268:
//   *(this+0)    = 0;                 // mnRefCount = 0
//   *(this+4)    = *(pName+0);        // mFileName.m_pData = pName->m_pData ...
//   if (*(pName+0) != &s_EmptyData)   //   (the EAStringC copy-construct; bump the
//       interrupt-masked lwarx/+0x10000/stwcx. on *m_pData;   // shared refcount
//   *(this+0x10) = 0;                 // mpResolveContext = 0
//   *(this+0x14) = 0;                 // mpData = 0
//   *(this+0x18) = 0;                 // mpDataBlock = 0
//   *(this+8)    = 1;                 // mnState = 1   (requested)
//   *(this+0xC)  = 1;                 // mnField12 = 1
// The EAStringC member copy + its refcount increment (the +0x10000 add on the
// StringDataC refcount, with the empty-string sentinel guard) is exactly the
// EAStringC copy constructor, so it is expressed here as a placement-construct of
// the member from *pName -- semantic parity with the asm, x64-native widths.
// ---------------------------------------------------------------------------
AptFile* MakeAptFile(void* pMem, EAStringC* pName)
{
    AptFile* pFile = static_cast<AptFile*>(pMem);

    pFile->mnRefCount = 0;                                  // *(this+0) = 0

    // mFileName = *pName: copy the shared StringDataC pointer + bump its internal
    // refcount (the empty-string sentinel guard is inside the EAStringC copy ctor).
    ::new (static_cast<void*>(&pFile->mFileName)) EAStringC(*pName);   // *(this+4) = *(pName+0) + IncRef

    pFile->mpResolveContext = nullptr;                     // *(this+0x10) = 0
    pFile->mpData           = nullptr;                     // *(this+0x14) = 0
    pFile->mpDataBlock      = nullptr;                     // *(this+0x18) = 0
    pFile->mnState          = 1;                           // *(this+8)   = 1 (requested)
    pFile->mnField12        = 1;                           // *(this+0xC) = 1

    return pFile;                                          // ctor returns `this`
}

// AptFile::mpData points at the loaded + resolved .apt movie data root. The
// AptExportEntry / AptMovieData view of that root now lives in AptFile.h (shared so
// AptCharacterAnimation::GetIDFromImportFile + AptLoader::AllImportsAvailable reuse
// it). See the reconcile note there (the export table at [c:+0x38]/[c:+0x3C] vs the
// PS3 init-indicator slot). AptImportEntry comes from AptCharacterAnimation.h.
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // AptImportEntry (AptMovieData::mpImportTable element)

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
        // The asm comparator (the IDA-unnamed (EAStringC,EAStringC) -> bool sub
        // @...0040_0) is the EAStringC equality operator -- identified and wired.
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
