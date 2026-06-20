#ifndef SHAREDCLASSES_DATALISTS_ICELIST_H
#define SHAREDCLASSES_DATALISTS_ICELIST_H

// ICEList.h
// BrnResource::ICEList -- the runtime aggregation of loaded ICE take-dictionary
// resources. Each added resource is a CgsResource::ResourcePtr onto a
// CgsContainers::Dictionary<ICE::ICETakeData> (a key -> camera-take-data map);
// ICEList holds up to KI_MAX_ICE_LISTS of them and serves take lookups by
// dictionary key or by take guid across all loaded lists.
//
// Shape recovered from the DecFIGS DWARF (SharedClasses/DataLists/ICEList.h);
// behaviour of the three exported methods verified against the X360 pseudocode
// (AddListResource @0x8267BD18, GetICETakeData @0x8267BDF0,
// GetICETakeDataFromGuid @0x8267BEC0). Construct/Destruct/GetICEMovieCount are
// inline-away helpers (no separate X360 export); their bodies live in ICEList.cpp
// next to the exported methods, mirroring the sibling ChallengeList TU.

#include "types.hpp"                                                        // s32 widths
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"          // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/Containers/CgsDictionary.h"                 // CgsContainers::Dictionary<T>, DictEntry::DictionaryKey
#include "SDKs/Packages/ICE/ICEData.hpp"                                    // ICE::ICETakeData

namespace BrnResource
{

// ICEList.h:51
class ICEList
{
public:
    // ICEList.h:40 (DWARF). Each loaded list is a resource pointer onto a take
    // dictionary keyed by DictEntry::DictionaryKey.
    typedef CgsResource::ResourcePtr<CgsContainers::Dictionary<ICE::ICETakeData> >
        ICETakeDictionaryResourcePtr;

    // ICEList.h:53 (DWARF). Max number of take-dictionary resources held.
    static const s32 KI_MAX_ICE_LISTS = 32;

    void Construct();   // :56  RECONSTRUCTED (inline-away helper)
    void Destruct();    // :59  RECONSTRUCTED (inline-away helper)

    // :62  RECONSTRUCTED @0x8267BD18
    void AddListResource(const ICETakeDictionaryResourcePtr& lResource);

    // :65  RECONSTRUCTED (inline-away helper)
    s32 GetICEMovieCount() const;

    // :68  RECONSTRUCTED @0x8267BDF0
    const ICE::ICETakeData* GetICETakeData(CgsContainers::DictEntry::DictionaryKey lKey) const;

    // :72  RECONSTRUCTED @0x8267BEC0
    const ICE::ICETakeData* GetICETakeDataFromGuid(s32 liGuid) const;

private:
    // ---- layout (DWARF offsets, proven by X360 pseudocode) ----
    ICETakeDictionaryResourcePtr maTakeDictionary[KI_MAX_ICE_LISTS]; // :91  @+0x000 (32-byte stride)
    s32                          miCount;                            // :92  @+0x400 accumulated take count
    s32                          miListCount;                        // :93  @+0x404 lists added (<= KI_MAX_ICE_LISTS)
};

} // namespace BrnResource

#endif // SHAREDCLASSES_DATALISTS_ICELIST_H
