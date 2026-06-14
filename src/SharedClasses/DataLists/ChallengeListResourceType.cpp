#include "SharedClasses/DataLists/ChallengeListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
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

    uint32_t ChallengeListResourceType::GetTypeID() const
    {
        return KU_CHALLENGE_LIST_RESOURCE_TYPE_ID;
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
