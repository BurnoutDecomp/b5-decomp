#ifndef CHALLENGE_LIST_RESOURCE_TYPE_H
#define CHALLENGE_LIST_RESOURCE_TYPE_H

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnResource
{
struct ChallengeListEntry;

// ADDITIVE GROW (FLAG): the loaded challenge-list resource payload. Layout from the
// DecFIGS DWARF (ChallengeListResourceType.h:43) and confirmed by the X360 binary --
// ChallengeListResourceType::FixUp/FixDown relocate word[1] (the entry-array slot), and
// ChallengeList::GetChallengeData @0x82326080 reads it (`lwz 4`) then indexes it
// by 216 (== sizeof(ChallengeListEntry)).
//
// ⚠️ CORRECTED 2026-08-27 (challenge-list wave), the SAME defect the vehicle-load wave
// found in WheelListResource and fixed there. The entry-array base used to be declared
// `ChallengeListEntry* mpEntries` "at +0x04"; on MSVC x64 an 8-byte pointer there
// actually lands at +0x08 (the compiler inserts 4 bytes of alignment padding), while the
// paired FixUp/FixDown do a THIRTY-TWO-BIT add at +0x04 -- so the rebase landed in the
// padding and the pointer stayed garbage. The serialised record really carries a 4-byte
// slot at +0x04, so this now matches the correct sibling model (VehicleListResource /
// WheelListResource): a u32 slot converted with the project's low-4 GB PointerFromU32
// convention. MEASURED against the shipped build/game/ONLINECHALLENGES.BNDL: its single
// resource is type 0x1001F with payload {muNumChallenges = 458, muEntriesOffset = 0x10}
// followed by 458 * 216 bytes -- 16 + 216*458 == 98944 == the entry's uncompressed size
// exactly, which pins BOTH the 16-byte header and the 4-byte slot at +0x04.
class ChallengeListResource
{
public:
    // X360: `*BrnResource::ChallengeListRes(a2)` inside ChallengeList::AddListResource
    // @0x8267B598 -- the count word at +0x00.
    u32 GetNumChallenges() const;

    // X360 (ChallengeList::GetChallengeData @0x82326080): the entry array base is the
    // 32-bit slot at +0x04 and the per-entry stride is 216 (sizeof ChallengeListEntry);
    // returns &entries[liEntryIndex]. No bounds check -- the caller owns the index assert.
    const ChallengeListEntry* GetEntry(s32 liEntryIndex) const;

private:
    friend class ChallengeListResourceType;   // FixUp/FixDown rebase muEntriesOffset

    u32 muNumChallenges;   // +0x00  (ChallengeListResourceType.h:46) challenge count
    u32 muEntriesOffset;   // +0x04  serialised 32-bit entry-array slot (FixUp-rebased)
    u64 muHeaderPad;       // +0x08  (ChallengeListResourceType.h:48; the header is 16 bytes)
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
