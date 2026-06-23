#ifndef CHALLENGE_LIST_RESOURCE_TYPE_H
#define CHALLENGE_LIST_RESOURCE_TYPE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnResource
{
struct ChallengeListEntry;

// ADDITIVE GROW (FLAG): the loaded challenge-list resource payload. Layout from the
// DecFIGS DWARF (ChallengeListResourceType.h:43) and confirmed by the X360 binary --
// ChallengeListResourceType::FixUp/FixDown relocate word[1] (mpEntries), and
// ChallengeList::GetChallengeData @0x82326080 reads mpEntries (`lwz 4`) then indexes it
// by 216 (== sizeof(ChallengeListEntry)). Only the byte LAYOUT is reconstructed here
// (no methods); the prior commit forward-declared this struct in ChallengeList.h.
struct ChallengeListResource
{
    u32                 muNumChallenges;  // +0x00  (ChallengeListResourceType.h:46)
    ChallengeListEntry* mpEntries;        // +0x04  (relocatable; the FixUp/FixDown target)
    u64                 mu16BytePad;      // +0x08  (ChallengeListResourceType.h:48; align/pad to 0x10)
};

class ChallengeListResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
