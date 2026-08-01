// ICEList.cpp
// BrnResource::ICEList -- aggregation of loaded ICE take-dictionary resources.
//
// Reconstructed from the X360 ARTIST build:
//   ICEList::AddListResource        @ 0x8267BD18
//   ICEList::GetICETakeData         @ 0x8267BDF0
//   ICEList::GetICETakeDataFromGuid @ 0x8267BEC0
// plus the inline-away helpers Construct / Destruct / GetICEMovieCount (no separate
// X360 export; bodies recovered from the DecFIGS DWARF + the sibling ChallengeList
// pattern).
//
// The two lookup methods each loop the KI_MAX_ICE_LISTS slots and STOP at the first
// slot whose ResourcePtr is the null image (the lists are packed from 0 by
// AddListResource, so the first empty slot is the end of the loaded set). In the
// binary that test is an inline 3-word compare of the ResourcePtr against the
// &dword_82FFB248 image -- the BaseResourcePtr IDENTITY REGION, i.e. mpResourceMemory
// plus the two mHandle words (an earlier note here said mpResourceMemory/mpNext/mpPrev;
// that predates the X360-authoritative member reorder in CgsResourcePtr.h and is
// corrected). The X360 returns 0 the moment a slot equals the image
// (GetICETakeData/GetICETakeDataFromGuid @ 0x8267BDF0/0x8267BEC0 -> goto LABEL_6),
// matching the PS3 DecFIGS `if (NULLResourcePtr == slot) break;` (0x814C48/0x8197DC).
// It is restored as an early `return 0` on the first invalid slot. For a valid slot
// the X360 inlined the
// take-dictionary linear scan (Find / GetAt over mpaIndex); those are restored to
// the logical Dictionary accessors (dict->Find / dict->GetNumEntries / dict->GetAt),
// which is exact semantic parity.
//
// ⭐ MOUNTED 2026-08-01 (ICEList wave). Nothing about this TU was wrong except one
// unlinkable spelling (see skNullIdentity below); it had simply never been in the exe
// source list, and its consumers -- GameDataModule::mICEList (X360 member +457664) and
// DirectorResourceManager::mpICEDictionaryList -- only lit up with the shot-group bank.

#include "SharedClasses/DataLists/ICEList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (sentinel value)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace BrnResource
{

namespace
{
    // X360 &dword_82FFB248 -- the NULL image both lookup loops compare each slot
    // against. CORRECTED 2026-08-01 from the recovered disassembly of
    // GetICETakeData @0x8267BDF0 / GetICETakeDataFromGuid @0x8267BEC0: the compare is
    //     r10 = &dword_82FFB248; r11 = &slot; r9 = 0
    //     do { if (*(u32*)r10 != *(u32*)r11) break; r9 += 4; r10 += 4; r11 += 4; }
    //     while (r9 < 12);
    // i.e. CgsResource::BaseResourcePtr::IsEqual @0x8227D298 inlined -- a word-for-word
    // compare of the ResourcePtr IDENTITY REGION (mpResourceMemory + the two mHandle
    // words), NOT a two-pointer ResourceHandle compare.
    //
    // The earlier spelling here was `slot == skInvalidHandle` against a bare
    // ResourceHandle. That reached ResourcePtr<Type>::operator==(const ResourceHandle&),
    // which is DECLARED in CgsResourcePtr.h and DEFINED NOWHERE IN THE TREE -- so this TU
    // could never link, which is why it was never in the exe source list. It is retired in
    // favour of the identity image the console actually compares against, spelled through
    // the real (bodied, X360-attested) IsEqual. Sized from the identity region itself, per
    // the project's never-transcribe-a-console-byte-size rule.
    struct NullIdentityImage
    {
        void*                        mpResourceMemory;
        CgsResource::ResourceHandle  mHandle;
    };
    const NullIdentityImage skNullIdentity = { 0, { 0, 0 } };
}

// ICEList::Construct -- reset every list slot to the invalid handle and zero the
// counts. Recovered from the DWARF (32x ResourcePtr::operator= loop + count resets),
// mirroring ChallengeList::Construct.
void ICEList::Construct()
{
    for ( s32 liIndex = 0; liIndex < KI_MAX_ICE_LISTS; ++liIndex )
    {
        maTakeDictionary[ liIndex ] = CgsResource::NULLResourceHandle;
    }

    miCount     = 0;
    miListCount = 0;
}

// ICEList::Destruct -- release every list slot back to the invalid handle. Recovered
// from the DWARF (32x ResourcePtr::operator= loop), mirroring ChallengeList::Destruct.
void ICEList::Destruct()
{
    for ( s32 liIndex = 0; liIndex < KI_MAX_ICE_LISTS; ++liIndex )
    {
        maTakeDictionary[ liIndex ] = CgsResource::NULLResourceHandle;
    }
}

// ICEList::AddListResource @ 0x8267BD18
// Assert there is space, store the resource in the next slot, accumulate its entry
// count into miCount, and advance miListCount.
void ICEList::AddListResource(const ICETakeDictionaryResourcePtr& lResource)
{
    CGS_ASSERT(miListCount < KI_MAX_ICE_LISTS, "No space for more ICE lists\n");

    // X360: CreateFromHandle(&maTakeDictionary[miListCount], &lResource.mHandle) --
    // the inlined form of the public ResourcePtr assignment. Restored as operator=
    // (DWARF shows ResourcePtr::operator=), which is the faithful source-level write.
    maTakeDictionary[ miListCount ] = lResource;

    // X360: result = CgsContainers_(a2) (GetMemoryResource -> dictionary), then
    // miCount += *result (the dictionary's entry count) -- accumulate movie/take total.
    const CgsContainers::Dictionary<ICE::ICETakeData>* lpTakeDictionary =
        lResource.GetMemoryResource();
    miCount += lpTakeDictionary->GetNumEntries();

    ++miListCount;
}

// ICEList::GetICETakeData @ 0x8267BDF0
// Scan all loaded lists; for each valid one look the key up in its take dictionary
// and return the first non-null hit, else null.
const ICE::ICETakeData* ICEList::GetICETakeData(CgsContainers::DictEntry::DictionaryKey lKey) const
{
    for ( s32 liIndex = 0; liIndex < KI_MAX_ICE_LISTS; ++liIndex )
    {
        // X360: the inlined 3-word IsEqual against the null identity image -- the
        // first empty slot ends the loaded set, so bail out (return null) here.
        if ( maTakeDictionary[ liIndex ].IsEqual(&skNullIdentity) )
        {
            return 0;
        }

        const CgsContainers::Dictionary<ICE::ICETakeData>* lpTakeDictionary =
            maTakeDictionary[ liIndex ].GetMemoryResource();

        // X360: inlined linear scan over mpaIndex matching the key -> entry mpData.
        const ICE::ICETakeData* lpTakeData = lpTakeDictionary->Find(lKey);
        if ( lpTakeData )
        {
            return lpTakeData;
        }
    }

    return 0;
}

// ICEList::GetICETakeDataFromGuid @ 0x8267BEC0
// Scan all loaded lists; for each valid one walk every entry and return the first
// take whose miGuid matches, else null.
const ICE::ICETakeData* ICEList::GetICETakeDataFromGuid(s32 liGuid) const
{
    for ( s32 liDicIndex = 0; liDicIndex < KI_MAX_ICE_LISTS; ++liDicIndex )
    {
        // X360: the inlined 3-word IsEqual against the null identity image -- the
        // first empty slot ends the loaded set, so bail out (return null) here.
        if ( maTakeDictionary[ liDicIndex ].IsEqual(&skNullIdentity) )
        {
            return 0;
        }

        const CgsContainers::Dictionary<ICE::ICETakeData>* lpTakeDictionary =
            maTakeDictionary[ liDicIndex ].GetMemoryResource();

        // X360: iterate all miNumEntries entries (GetAt asserts the index bound);
        // return the first take whose miGuid == liGuid.
        const s32 liNumTakes = lpTakeDictionary->GetNumEntries();
        for ( s32 liTakeIndex = 0; liTakeIndex < liNumTakes; ++liTakeIndex )
        {
            const ICE::ICETakeData* lpTakeData = lpTakeDictionary->GetAt(liTakeIndex);
            if ( lpTakeData->miGuid == liGuid )
            {
                return lpTakeData;
            }
        }
    }

    return 0;
}

// ICEList::GetICEMovieCount -- the accumulated take count. Trivial accessor (X360
// reads miCount @+0x400); recovered from the DWARF declaration.
s32 ICEList::GetICEMovieCount() const
{
    return miCount;
}

} // namespace BrnResource
