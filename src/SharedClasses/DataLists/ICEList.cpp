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
// The two lookup methods each loop all KI_MAX_ICE_LISTS slots and SKIP a slot whose
// ResourcePtr is the invalid/null handle. In the binary that skip is an inline
// 3-dword (12-byte) compare of the ResourcePtr against the &dword_82FFB248 sentinel
// (mpResourceMemory/mpNext/mpPrev == the null-handle image); the DecFIGS DWARF
// renders the pre-inline source as ResourcePtr::operator!=(...), so it is restored
// as `maTakeDictionary[i] != skInvalidHandle`. For a valid slot the X360 inlined the
// take-dictionary linear scan (Find / GetAt over mpaIndex); those are restored to
// the logical Dictionary accessors (dict->Find / dict->GetNumEntries / dict->GetAt),
// which is exact semantic parity.

#include "SharedClasses/DataLists/ICEList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (sentinel value)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace BrnResource
{

namespace
{
    // X360: &dword_82FFB248 -- the invalid/default resource-handle sentinel a list
    // slot is compared against ("is this ResourcePtr null?"). Modeled as a file-local
    // default (zero) ResourceHandle: {nullptr, nullptr}, matching the ChallengeList
    // sibling's skInvalidHandle.
    // FLAG: the exact .rodata bytes of dword_82FFB248 are not recovered; an all-zero /
    // invalid handle is the modeled value (the 12-byte compare covers the null
    // mpResourceMemory/mpNext/mpPrev image).
    const CgsResource::ResourceHandle skInvalidHandle = {};
}

// ICEList::Construct -- reset every list slot to the invalid handle and zero the
// counts. Recovered from the DWARF (32x ResourcePtr::operator= loop + count resets),
// mirroring ChallengeList::Construct.
void ICEList::Construct()
{
    for ( s32 liIndex = 0; liIndex < KI_MAX_ICE_LISTS; ++liIndex )
    {
        maTakeDictionary[ liIndex ] = skInvalidHandle;
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
        maTakeDictionary[ liIndex ] = skInvalidHandle;
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
        // X360: 12-byte compare of the ResourcePtr against the null sentinel -- skip
        // empty slots.
        if ( maTakeDictionary[ liIndex ] != skInvalidHandle )
        {
            const CgsContainers::Dictionary<ICE::ICETakeData>* lpTakeDictionary =
                maTakeDictionary[ liIndex ].GetMemoryResource();

            // X360: inlined linear scan over mpaIndex matching the key -> entry mpData.
            const ICE::ICETakeData* lpTakeData = lpTakeDictionary->Find(lKey);
            if ( lpTakeData )
            {
                return lpTakeData;
            }
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
        // X360: 12-byte compare of the ResourcePtr against the null sentinel -- skip
        // empty slots.
        if ( maTakeDictionary[ liDicIndex ] != skInvalidHandle )
        {
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
