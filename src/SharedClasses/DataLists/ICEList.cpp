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
// The two lookup methods each loop the KI_MAX_ICE_LISTS slots and stop at the first slot
// equal to CgsResource::NULLResourcePtr (the lists are packed from 0 by AddListResource):
// ResourcePtr::operator!= as DWARF names it, the inlined BaseResourcePtr::IsEqual against this
// TU's NULLResourcePtr (0x82FFB248). For a valid slot the X360 inlined the take-dictionary
// scans (Find / GetAt over mpaIndex), written as the Dictionary accessors.

#include "SharedClasses/DataLists/ICEList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (sentinel value)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

namespace BrnResource
{

// Both lookups stop at the first slot equal to CgsResource::NULLResourcePtr: ARTIST compares
// each slot with this TU's NULLResourcePtr (0x82FFB248, internal linkage per DecFIGS
// CgsResourcePtr.h:149) through the inlined BaseResourcePtr::IsEqual. DWARF (ICEList.cpp:121 /
// :141) names ResourcePtr<Dictionary<ICETakeData>>::operator!= in both loops.

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
    // ARTIST 0x8267BD9C: streamed, on the unity path (0x820A88F8) line 79.
    CGS_ASSERT_STREAM_AT(miListCount < KI_MAX_ICE_LISTS, "No space for more ICE lists\n",
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../../SharedClasses/DataLists/ICEList.cpp", 79);

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
        // The first empty slot ends the loaded set.
        if ( maTakeDictionary[ liIndex ] != CgsResource::NULLResourcePtr )
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
        else
        {
            break;
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
        // The first empty slot ends the loaded set.
        if ( maTakeDictionary[ liDicIndex ] != CgsResource::NULLResourcePtr )
        {
            const CgsContainers::Dictionary<ICE::ICETakeData>* lpTakeDictionary =
                maTakeDictionary[ liDicIndex ].GetMemoryResource();

            // X360: iterate all miNumEntries entries (GetAt asserts the index bound; ARTIST
            // re-reads the count each pass, 0x8267BF8C: DWARF scopes no count local);
            // return the first take whose miGuid == liGuid.
            for ( s32 liTakeIndex = 0; liTakeIndex < lpTakeDictionary->GetNumEntries(); ++liTakeIndex )
            {
                const ICE::ICETakeData* lpTakeData = lpTakeDictionary->GetAt(liTakeIndex);
                if ( lpTakeData->miGuid == liGuid )
                {
                    return lpTakeData;
                }
            }
        }
        else
        {
            break;
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
