#include "GameShared/GameClasses/SceneManager/Zones/Resources/ZoneListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/SceneManager/Zones/ZoneList.h"   // the REAL CgsSceneManager::ZoneList

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ZoneListResourceType::FixDown                       @ 0x828D1E80
//   CgsResource::ZoneListResourceType::FixUp                         @ 0x828D1E70
//   CgsResource::ZoneListResourceType::GetSerialisedResourceDescriptor @ 0x828C34E0
//   CgsResource::ZoneListResourceType::GetTypeID                     @ 0x828AC420
//   CgsResource::ZoneListResourceType::Serialise                     @ 0x828D1E08
//
// FixDown/FixUp forward to the ZoneList's own (un)relocation, passing the load base
// (the leading word of the rw::Resource) as the delta. Serialise relocates the
// source to file-relative pointers, copies the blob to the destination resource's
// buffer, then re-relocates both. GetSerialisedResourceDescriptor returns the
// five-entry descriptor whose first entry's size spans the blob.

// SEAM (platform-4 widened data): the local __debugbreak trap stubs for
// ZoneList::FixUp/FixDown are gone -- the REAL members are committed in
// GameShared/GameClasses/SceneManager/Zones/ZoneList.cpp (@0x828D05B8/0x828D0640)
// and are called on the resource blob itself. The serialised header is the natural
// x64 widening of CgsSceneManager::ZoneList (pointers @0/8/0x10/0x18 u64,
// muTotalZones @0x20, muTotalPoints @0x24), so the count/blockEnd reads use the
// NAMED accessors instead of the X360 +16/+12 byte offsets.

namespace CgsResource
{
    static const uint32_t KU_ZONE_LIST_RESOURCE_TYPE_ID = 45056;

    uint32_t ZoneListResourceType::GetTypeID() const
    {
        return KU_ZONE_LIST_RESOURCE_TYPE_ID;
    }

    // SEAM: forward to the real member (full-width base -- the x64 heap base does not
    // fit the console's 32-bit delta; ZoneList::FixDown takes the base as void*).
    void ZoneListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsSceneManager::ZoneList*>(lpResource)->FixDown(
            reinterpret_cast<void*>(CgsResource::GetLoadBase64(lrResource)));
    }

    void ZoneListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsSceneManager::ZoneList*>(lpResource)->FixUp(
            reinterpret_cast<void*>(CgsResource::GetLoadBase64(lrResource)));
    }

    ResourceDescriptor ZoneListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        // SEAM: X360 read muTotalZones @+16 (lwz 0x10) and the mpiZonePointCounts block-end
        // pointer @+12 (lwz 0xC); the widened header homes them at +0x20 / +0x18 -- read
        // via the NAMED ZoneList accessors. Size = counts-table end (s16 per zone) - base.
        const CgsSceneManager::ZoneList* lpList = static_cast<const CgsSceneManager::ZoneList*>(lpResource);
        u32       luZoneCount = lpList->GetTotalZones();
        uintptr_t luBlockEnd  = reinterpret_cast<uintptr_t>(lpList->GetZonePointCounts());

        // First entry holds the payload size (alignment 16); the remaining four are
        // empty (size 0, alignment 1).
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(2 * luZoneCount + luBlockEnd - lRes);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }

    void* ZoneListResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        // SEAM: the count/blockEnd reads and the FixDown/FixUp calls go through the real
        // widened CgsSceneManager::ZoneList (named members / committed member bodies),
        // replacing the X360 +16/+12 offset reads and the local trap stubs.
        CgsSceneManager::ZoneList* lpSrcList =
            static_cast<CgsSceneManager::ZoneList*>(const_cast<void*>(lpResource));
        uintptr_t lSrc  = reinterpret_cast<uintptr_t>(lpResource);
        void*     lpDst = lrDest.m_baseResources[0];
        usize     luSize = (2 * static_cast<usize>(lpSrcList->GetTotalZones())
                            + reinterpret_cast<uintptr_t>(lpSrcList->GetZonePointCounts())) - lSrc;

        // FLAG (ARTIST 0x828D1E08): the serialise-out FixDown rebases the SOURCE by its
        // own address (delta == src), NOT by 0. ARTIST asm calls ZoneList::FixDown(r3=src,
        // r4=src) -- r4 is never reloaded after entry so it still holds lpResource; DecFIGS
        // confirms `ZoneList::FixDown(v5, v5)`. (Was erroneously passing 0.)
        lpSrcList->FixDown(reinterpret_cast<void*>(lSrc));
        std::memcpy(lpDst, lpResource, luSize);
        static_cast<CgsSceneManager::ZoneList*>(lpDst)->FixUp(lpDst);
        lpSrcList->FixUp(reinterpret_cast<void*>(lSrc));
        return lpDst;
    }
}
