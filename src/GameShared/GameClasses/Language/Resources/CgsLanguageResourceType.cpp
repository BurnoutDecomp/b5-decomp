#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h"

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource + rw::BaseResourceDescriptors<5> complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // CgsUnicode::ByteLength
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsResource::LanguageResourceType -- the language string-table handler (id 0x27 = 39). A faithful
// port of the X360 ARTIST bodies (no Feb-2007 leak source for this TU):
//   GetTypeID                        0x82860F18
//   GetSerialisedResourceDescriptor  0x82863D00
//   FixDown                          0x82864560
//   FixUp                            0x82864600
//
// The resource is a flat string table: a header (meLanguageID, entry count, entry-array pointer)
// followed by an array of (hash, string-pointer) entries; the entry array and each entry's string
// pointer are load-relative self-references rebased by FixUp/FixDown (ordinary serialised-pointer
// fixup, mirroring BrnEnvironmentTimeLineResourceType). The serialised size is the 12-byte header
// plus, per entry, the string's UTF-8 byte length + 9 bytes of per-entry overhead.

namespace CgsResource
{
    // Load-relative on-disk pointer -> absolute. Pointers in the serialised LanguageResource are
    // stored as u32 offsets; FixUp/FixDown add/subtract the load base. (Same helper shape as
    // BrnEnvironmentTimeLineResourceType / VehicleList.)
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    // Resource registry type id for the language string-table resource. Recovered verbatim from
    // GetTypeID @ 0x82860F18 ("return 39"); 39 = 0x27 ("Language" in the wiki resource-type table).
    static const uint32_t KU_LANGUAGE_RESOURCE_TYPE_ID = 39;

    // GetTypeID @ 0x82860F18: return 39.
    uint32_t LanguageResourceType::GetTypeID() const
    {
        return KU_LANGUAGE_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x82863D00. Compute the serialised size of the string table
    // (12-byte header + per entry: ByteLength(string) + 9) and report it as block 0's size with
    // alignment 1; blocks 1..4 are {0, 1}. Returns the descriptor by value (X360 a1 = hidden sret).
    ResourceDescriptor LanguageResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // X360: if (!a3) Begin/Fire/EndAssert("NULL resource passed to...","...cpp",117). The baked
        // file/line 117 is NOT reproduced (CGS_ASSERT injects __FILE__/__LINE__).
        CGS_ASSERT(lpResource != 0, "NULL resource passed to GetSerialisedResourceDescriptor");

        const LanguageResource* lpRes = static_cast<const LanguageResource*>(lpResource);

        u32 luSize = 12;                       // v5 = 12 (header)
        if (lpRes->muSize)                     // if (*(a3 + 4))
        {
            const LanguageResourceHashEntry* lpEntries =
                PointerFromU32<const LanguageResourceHashEntry>(lpRes->mpEntries);
            for (s32 li = 0; li < lpRes->muSize; ++li)   // do/while v6 < *(a3+4), v7 stride 8
            {
                // v8 = CgsUnicode::ByteLength(*(v7 + *(a3+8) + 4));  v5 += v8 + 9;
                const u32 luLen = CgsUnicode::ByteLength(PointerFromU32<const u8>(lpEntries[li].mpString));
                luSize += luLen + 9;
            }
        }

        // X360 a1[0..9]: block 0 = { m_size = luSize, m_alignment = 1 }; blocks 1..4 = { 0, 1 }.
        //   a1[0]=size a1[1]=1  a1[2]=0 a1[3]=1  a1[4]=0 a1[5]=1  a1[6]=0 a1[7]=1  a1[8]=0 a1[9]=1
        // (mirrors CgsFontResourceType's descriptor fill; block0 carries the computed size, every
        // alignment is 1.)
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 1u;
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // FixDown @ 0x82864560. While mpEntries is still the runtime pointer, un-rebase each entry's
    // string pointer, THEN un-rebase the entry array pointer after the loop. void by contract (the
    // X360 `result` is the EndAssert() artifact). The per-entry assert tests the DELTA (*a3 != 0),
    // not the entry.
    void LanguageResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        LanguageResource* lpRes = static_cast<LanguageResource*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);   // v6 = *a3 (read at top)

        if (lpRes->muSize)                     // if (*(a2 + 4))
        {
            // *(a2 + 8) (mpEntries) is still the runtime pointer here.
            LanguageResourceHashEntry* lpEntries =
                PointerFromU32<LanguageResourceHashEntry>(lpRes->mpEntries);
            for (s32 li = 0; li < lpRes->muSize; ++li)   // do/while v5, v7 stride 8
            {
                // X360: if (!*a3) Begin/Fire/EndAssert("No Memory Resource","...cpp",46). The assert
                // tests the DELTA is non-zero (not the entry); baked line 46 NOT reproduced.
                CGS_ASSERT(luDelta != 0, "No Memory Resource");
                lpEntries[li].mpString -= luDelta;       // *(v9 + 4) -= v8
            }
        }

        lpRes->mpEntries -= luDelta;           // *(a2 + 8) -= v6 (AFTER the loop)
    }

    // FixUp @ 0x82864600. Rebase the entry array pointer first (BEFORE the loop), then per entry
    // rebase its string pointer. void by contract. The per-entry assert tests the DELTA (*a3 != 0).
    void LanguageResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        LanguageResource* lpRes = static_cast<LanguageResource*>(lpResource);

        const u32 luDelta = CgsResource::GetLoadBase(lrResource);   // *a3

        lpRes->mpEntries += luDelta;           // *(a2 + 8) += *a3 (BEFORE the loop)

        if (lpRes->muSize)                     // v6 = *(a2 + 4)
        {
            LanguageResourceHashEntry* lpEntries =
                PointerFromU32<LanguageResourceHashEntry>(lpRes->mpEntries);
            for (s32 li = 0; li < lpRes->muSize; ++li)   // do/while v5, v7 stride 8
            {
                // X360: if (!*a3) Begin/Fire/EndAssert("No Memory Resource","...cpp",66). Assert the
                // DELTA is non-zero (not the entry); baked line 66 NOT reproduced.
                CGS_ASSERT(luDelta != 0, "No Memory Resource");
                lpEntries[li].mpString += luDelta;       // *(v9 + 4) += v8
            }
        }
    }
}
