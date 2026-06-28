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

// Fixup @0x80E9E4 -- FLAG (deep follow-on): the recursive relocate/transcode of
// the serialised character tree. The console walks the character table, adds the
// load base to every record's file-relative offset, switches on each character's
// type to relocate its type-specific fields, recurses into AptMovie::resolve for
// sprites + AptLoader::Load for imports. On x64 this must TRANSCODE the 32-bit
// serialised records into the native 64-bit runtime structs (the in-place
// relocation cannot work). It is the data-side keystone -- stubbed so the
// load-completion entry links; reconstructed next.
AptCharacterAnimation* AptCharacterAnimation::Fixup(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    (void)pBase; (void)pConstFile; (void)pBlock;
    return this;
}
