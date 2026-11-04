#ifndef CGS_LANGUAGE_RESOURCE_TYPE_H
#define CGS_LANGUAGE_RESOURCE_TYPE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // Native X360 language resource: FixUp @ 0x82864600 rebases the
    // array slot at +8 and each string slot at +4, with an 8-byte stride.
    struct LanguageResourceHashEntry
    {
        u32 muHash;
        u32 mpString;
    };

    struct LanguageResource
    {
        u32 meLanguageID;
        u32 muSize;
        u32 mpEntries;

        const LanguageResourceHashEntry* GetEntries() const
        {
            return reinterpret_cast<const LanguageResourceHashEntry*>(
                static_cast<uintptr_t>(mpEntries));
        }
    };

    typedef char LanguageResourceEntrySizeCheck[sizeof(LanguageResourceHashEntry) == 8 ? 1 : -1];
    typedef char LanguageResourceSizeCheck[sizeof(LanguageResource) == 12 ? 1 : -1];

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
        uint32_t           GetTypeID() const ;
        ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
        void               FixDown(void* lpResource, const rw::Resource& lrResource) const ;
        void               FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    };
}

#endif
