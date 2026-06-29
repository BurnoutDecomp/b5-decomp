// ===========================================================================
// EATech Apt -- AptCharacterAnimation accessors.   DECOMPILED from the PS3
// EXTERNAL ELF (cross-checked vs X360 ARTIST). See the header for the
// 32-bit-format / 64-bit-runtime fork: the console walks the in-place file
// records with fixed strides; these reconstruct the same logic against the
// transcoded runtime struct's named members.
//
//   IsImport @0x7E3738 / UnmapCharacter @0x7E36CC /
//   ClearCharacterList @0x810DEC / IncCharacterList @0x80E790
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptSharedPtrIncRef/DecRef/Delete
#include "SDKs/EATech/include/Apt/AptConstFile.h"   // the serialised .apt header (Resolve/Fixup)

// IsImport @0x7E3738 -- find the import slot whose local id matches nId.
int32_t AptCharacterAnimation::IsImport(int32_t nId)
{
    if (mnImportCount <= 0)
        return -1;
    if (mpImportTable[0].mnId == nId)
        return 0;
    for (int32_t i = 1; i < mnImportCount; ++i)
    {
        if (mpImportTable[i].mnId == nId)
            return i;
    }
    return -1;
}

// UnmapCharacter @0x7E36CC -- index of pCharacter in the character table.
int32_t AptCharacterAnimation::UnmapCharacter(AptCharacter* pCharacter)
{
    if (mnCharacterCount <= 0)
        return -1;
    if (mpCharacterTable[0] == pCharacter)
        return 0;
    if (mnCharacterCount == 1)
        return -1;
    for (int32_t i = 1; i < mnCharacterCount; ++i)
    {
        if (mpCharacterTable[i] == pCharacter)
            return i;
    }
    return -1;
}

// ClearCharacterList @0x810DEC -- release one character reference per table entry
// (starting at index 1; index 0 is the movie's own root). Type-9 entries are
// imports still owned by the source movie, so they are left alone.
void AptCharacterAnimation::ClearCharacterList() const
{
    for (int32_t i = 1; i < mnCharacterCount; ++i)
    {
        AptCharacter* pCharacter = mpCharacterTable[i];
        if (pCharacter)
        {
            if (pCharacter->mnType != 9)
                pCharacter->ReleaseCharacterReference();
        }
    }
}

// IncCharacterList @0x80E790 -- add a character reference per table entry,
// binding any character that has no animation file yet to filePtr.
void AptCharacterAnimation::IncCharacterList(AptFilePtr filePtr) const
{
    for (int32_t i = 1; i < mnCharacterCount; ++i)
    {
        AptCharacter* pCharacter = mpCharacterTable[i];
        if (pCharacter)
        {
            if (!pCharacter->mpAnimationFile)
            {
                // AptSharedPtr assignment into the character's animation-file
                // slot (inc new, dec old). The old ref is null in this branch.
                AptFile* newp = filePtr.pData;
                AptFile* oldp = pCharacter->mpAnimationFile;
                pCharacter->mpAnimationFile = newp;
                if (newp)
                    AptSharedPtrIncRef(newp);
                if (oldp && AptSharedPtrDecRef(oldp) == 0)
                    AptSharedPtrDelete(oldp);
            }
            pCharacter->AddCharacterReference();
        }
    }
}

// Resolve @0x80EEC4 -- resolve the (serialised) movie root against the load base.
AptCharacterAnimation* AptCharacterAnimation::Resolve(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    // FLAG: the console relocates pConstFile->mnSecondaryOffset in place (+= base)
    // around the Fixup; x64 computes addresses without the 32-bit write-back, so
    // only the scratch clear + the Fixup remain here.
    mpResolveState = 0;
    return Fixup(pBase, pConstFile, pBlock);
}

// Fixup @0x80E9E4 -- relocate/resolve the serialised character tree. POINTER-SIZE
// DUAL-PATH (a PC-compatibility refinement of the console, which only ever shipped the
// 4-byte format): the console walks the character table and adds the load base to every
// record's file-relative offset IN PLACE, then uses the blob directly as the runtime
// root. That in-place relocation works VERBATIM on x64 for an 8-byte .apt (a 64-bit base
// fits an 8-byte slot), but NOT for a 4-byte .apt (a 64-bit pointer won't fit a 32-bit
// slot) -- that one must instead TRANSCODE the records into native structs. So we
// dispatch on the header's pointer size: 8 -> FixupInPlace (faithful verbatim),
// 4 -> FixupTranscode (the x64 fork). This keeps the faithful path faithful, confines
// the x64 invention to the 4-byte fork, and supports 8-byte .apts from other builds.
AptCharacterAnimation* AptCharacterAnimation::Fixup(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    if (pConstFile && pConstFile->GetPointerSizeBytes() == 8)
        return FixupInPlace(pBase, pConstFile, pBlock);
    return FixupTranscode(pBase, pConstFile, pBlock);
}

// FixupInPlace -- the 8-byte path: the faithful console relocation generalised to 8-byte
// slots. FLAG (scaffold): walk the character / import / frame records and add the load
// base to each file-relative pointer slot in place, then return `this` (the blob IS the
// runtime root). The per-record slot map (which fields are pointers, by character type)
// + the AptMovie::resolve / AptLoader::Load recursion is the shared follow-on with
// FixupTranscode -- so until that walk is bodied this is the identity stub.
AptCharacterAnimation* AptCharacterAnimation::FixupInPlace(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    (void)pBase; (void)pConstFile; (void)pBlock;
    return this;
}

// FixupTranscode -- the 4-byte path (the console default): the x64 fork that transcodes
// the 32-bit serialised records into native 64-bit runtime structs. FLAG: the deep
// data-side keystone -- walk + switch on each character's type, build each native struct,
// recurse into AptMovie::resolve (sprites) / AptLoader::Load (imports). Stubbed so the
// load-completion entry links; reconstructed next (shares the record walk with
// FixupInPlace, differing only in slot width + whether it relocates in place or rebuilds).
AptCharacterAnimation* AptCharacterAnimation::FixupTranscode(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    (void)pBase; (void)pConstFile; (void)pBlock;
    return this;
}

// GetIDFromImportFile @0x82ADE998 -- given an import-table slot index, resolve the
// id of the character that the import's class name names in the imported movie's
// export table. Mirrors AptFile::FindExport's inline strcmp; returns -1 if the
// imported movie has no exports or the class name is absent.
int32_t AptCharacterAnimation::GetIDFromImportFile(int32_t nImportIndex)
{
    const AptImportEntry& import = mpImportTable[nImportIndex];
    const AptMovieData* pMovie = static_cast<const AptMovieData*>(import.mpFile->mpData);

    const int32_t nExportCount = pMovie->mnExportCount;
    if (nExportCount <= 0)
        return -1;

    const char* pClassName = import.mpClassName;
    for (int32_t iExport = 0; iExport < nExportCount; ++iExport)
    {
        // Inline strcmp: walk until the SEARCH key (class name) terminates,
        // breaking early on any mismatch. Bytes are unsigned (asm lbz).
        const char* pSearch = pClassName;
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
            return pMovie->mpExportTable[iExport].mnCharacterId;
    }
    return -1;
}
