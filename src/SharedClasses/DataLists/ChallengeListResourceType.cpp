#include "SharedClasses/DataLists/ChallengeListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::ChallengeListResourceType::GetSerialisedResourceDescriptor @ 0x8267BCC0
//   BrnResource::ChallengeListResourceType::FixDown  @ 0x8267DDE8
//   BrnResource::ChallengeListResourceType::FixUp    @ 0x8267DE80
//   BrnResource::ChallengeListResourceType::GetTypeID@ 0x826757A8
//
// The resource is { u32 muCount; ChallengeRecord* mpRecords }; the only relocatable
// pointer is mpRecords (word 1). FixUp adds the delta (the rw::Resource's load base),
// FixDown subtracts it. (The X360 also walks the nested records, but that traversal
// writes nothing here — the nested fields are offsets, not pointers — so it is a
// no-op and omitted. The X360 FixUp returned the record count; the virtual is void.)

namespace BrnResource
{
    static const uint32_t KU_CHALLENGE_LIST_RESOURCE_TYPE_ID = 65567;
    static const u32      KU_CHALLENGE_LIST_ENTRY_SIZE        = 216;  // 0xD8 == sizeof(ChallengeListEntry)
    static const u32      KU_CHALLENGE_LIST_HEADER_SIZE       = 16;   // 16-byte-aligned ChallengeListResource header

    uint32_t ChallengeListResourceType::GetTypeID() const
    {
        return KU_CHALLENGE_LIST_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x8267BCC0 (store-for-store). The serialised payload is a
    // ChallengeListResource ({ muNumChallenges; ChallengeListEntry* mpEntries; 16-byte pad }) followed
    // by muNumChallenges ChallengeListEntry records of 216 (0xD8) bytes each. The X360:
    //   r9 = *a3 (muNumChallenges); r9 = 216 * r9; r9 += 16   -> entry0 size = 216*count + 16
    // then writes all five alignments (1) and the four trailing sizes (0), and a single 64-bit store
    // of { m_size = 216*count+16, m_alignment = 16 } overwrites entry0. The 16 is the
    // 16-byte-aligned ChallengeListResource header that precedes the record array.
    CgsResource::ResourceDescriptor
    ChallengeListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const ChallengeListResource* lpRes = static_cast<const ChallengeListResource*>(lpResource);
        const u32 luSize = KU_CHALLENGE_LIST_ENTRY_SIZE * lpRes->muNumChallenges + KU_CHALLENGE_LIST_HEADER_SIZE;

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;   // entry0 size
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;      // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    void ChallengeListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        reinterpret_cast<u32*>(lpResource)[1] -= CgsResource::GetLoadBase(lrResource);
    }

    void ChallengeListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        reinterpret_cast<u32*>(lpResource)[1] += CgsResource::GetLoadBase(lrResource);
    }
}
