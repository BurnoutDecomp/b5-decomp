#ifndef CGS_LANGUAGE_RESOURCE_TYPE_H
#define CGS_LANGUAGE_RESOURCE_TYPE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // ---- Runtime data layout (the loaded LanguageResource) ---------------------------------------
    //
    // The LanguageResource data type is NOT committed; only the relocation slice the four resource
    // virtuals touch is modelled here. Members carry the DecFIGS DWARF names where the DWARF gives
    // them (CgsLanguageResourceType.h:166-171 / :47/:87-88) and are FLAGGED where inferred.
    //
    // FixUp/FixDown read/rebase three fields: the entry count (a2+4) and the load-relative entry
    // array pointer (a2+8); per 8-byte entry they rebase the string pointer (entry+4). The first
    // dword of the resource (a2+0) and the first dword of each entry (entry+0) are not touched by
    // these functions.

    // One string-table slot. X360 v7 stride 8 (a single (size,align)-free pair of dwords): a key/
    // hash + a load-relative pointer to its UTF-8 string. Names from DWARF (LanguageResourceHashEntry
    // muHash @h:87, mpString @h:88); stored here as load-relative u32 (the on-disk serialised form
    // the FixUp/FixDown += / -= delta arithmetic operates on, like VehicleList/TimeLine entries).
    struct LanguageResourceHashEntry
    {
        u32 muHash;      // +0  key/hash — NOT touched by FixUp/FixDown/GetSerialisedResourceDescriptor
        u32 mpString;    // +4  load-relative ptr -> CgsUtf8 string (rebased; ByteLength'd in descriptor)
    };

    // The loaded language string table. DWARF (CgsLanguageResourceType.h:166-171) names the first two
    // members meLanguageID (+0) and muSize (+4); the X360 FixUp/FixDown read a2+4 as the ENTRY COUNT
    // that bounds the per-entry loop, so muSize is the count of LanguageResourceHashEntry in mpEntries.
    // FLAG: meLanguageID (+0) is untouched by these fns (DWARF name carried, semantics not exercised
    //       here); muSize (+4) is used purely as the entry count.
    struct LanguageResource
    {
        u32 meLanguageID;  // +0  language id — NOT touched by these fns (DWARF name) — FLAG
        s32 muSize;        // +4  number of LanguageResourceHashEntry in mpEntries (the loop bound, a2+4)
        u32 mpEntries;     // +8  load-relative ptr -> LanguageResourceHashEntry[muSize] (rebased)
    };

    // The language-string-table resource-type handler -- resource type id 0x27 ("Language", 39).
    // Reconstructed from the X360 ARTIST bodies (no Feb-2007 partial source source for this TU):
    //   GetTypeID                        0x82860F18  (= 39)
    //   GetSerialisedResourceDescriptor  0x82863D00  (12 + Sum(ByteLength(string) + 9); block0 size)
    //   FixDown                          0x82864560  (un-rebase each string, then the entry array)
    //   FixUp                            0x82864600  (rebase the entry array, then each string)
    //
    // Overrides only the four virtuals exercised by the load/save spine; the rest are inherited from
    // CgsResource::Type (non-pure base). Declared in X360 vtable order — DO NOT REORDER.
    struct LanguageResourceType : public CgsResource::Type
    {
        uint32_t           GetTypeID() const override;
        ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
        void               FixDown(void* lpResource, const rw::Resource& lrResource) const override;
        void               FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    };
}

#endif
