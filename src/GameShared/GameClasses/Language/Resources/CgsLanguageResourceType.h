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
    // muHash @h:87, mpString @h:88).
    //
    // x64 WIDENED DATA (FLAG; the staged on-disk layout): the staged LANGUAGE\000N bundle
    // carries the x64-converted table -- 16-byte entries {u64 hash, u64 stringOff} and a 16-byte
    // header {u32 langid, u32 count, u64 entriesOff} -- so the slots are modelled at the widened
    // stride the on-disk data attests (the console stride is 8/12; semantic parity is by named
    // member). The FixUp/FixDown +=/-= delta arithmetic operates on the widened offset slots.
    struct LanguageResourceHashEntry
    {
        u64 muHash;      // console +0 (u32)  key/hash — NOT touched by FixUp/FixDown/descriptor
        u64 mpString;    // console +4 (u32)  load-relative ptr -> CgsUtf8 string (rebased)
    };

    // The loaded language string table. DWARF (CgsLanguageResourceType.h:166-171) names the first two
    // members meLanguageID (+0) and muSize (+4); the X360 FixUp/FixDown read a2+4 as the ENTRY COUNT
    // that bounds the per-entry loop, so muSize is the count of LanguageResourceHashEntry in mpEntries.
    // FLAG: meLanguageID (+0) is untouched by the relocation fns (DWARF name carried); muSize (+4)
    //       is the entry count; mpEntries widened to u64 (the staged x64 data -- see above).
    struct LanguageResource
    {
        u32 meLanguageID;  // +0  language id (LoadStringTable asserts < 24 and installs it)
        s32 muSize;        // +4  number of LanguageResourceHashEntry in mpEntries (the loop bound)
        u64 mpEntries;     // console +8 (u32)  load-relative ptr -> LanguageResourceHashEntry[muSize]

        // The relocated entry array (valid after FixUp).
        const LanguageResourceHashEntry* GetEntries() const
        {
            return reinterpret_cast<const LanguageResourceHashEntry*>(
                static_cast<uintptr_t>(mpEntries));
        }
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
