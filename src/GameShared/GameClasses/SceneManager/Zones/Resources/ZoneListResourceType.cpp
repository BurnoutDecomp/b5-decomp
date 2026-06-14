#include "GameShared/GameClasses/SceneManager/Zones/Resources/ZoneListResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

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

namespace CgsSceneManager
{
    // ZoneList relocation helpers (own TUs); trap stubs until they land.
    class ZoneList
    {
    public:
        static void* FixDown(void* pZoneList, int liDelta);
        static void* FixUp(void* pZoneList, int liDelta);
    };

    void* ZoneList::FixDown(void*, int) { __debugbreak(); return nullptr; }
    void* ZoneList::FixUp(void*, int)   { __debugbreak(); return nullptr; }
}

namespace CgsResource
{
    static const uint32_t KU_ZONE_LIST_RESOURCE_TYPE_ID = 45056;

    uint32_t ZoneListResourceType::GetTypeID() const
    {
        return KU_ZONE_LIST_RESOURCE_TYPE_ID;
    }

    void ZoneListResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        CgsSceneManager::ZoneList::FixDown(lpResource, static_cast<s32>(CgsResource::GetLoadBase(lrResource)));
    }

    void ZoneListResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        CgsSceneManager::ZoneList::FixUp(lpResource, static_cast<s32>(CgsResource::GetLoadBase(lrResource)));
    }

    ResourceDescriptor ZoneListResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        u32 luZoneCount = *reinterpret_cast<const u32*>(lRes + 16);
        u32 luBlockEnd  = *reinterpret_cast<const u32*>(lRes + 12);

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
        void*     lpRes = const_cast<void*>(lpResource);
        uintptr_t lSrc  = reinterpret_cast<uintptr_t>(lpResource);
        void*     lpDst = lrDest.m_baseResources[0];
        usize     luSize = (2 * *reinterpret_cast<const u32*>(lSrc + 16)
                            + *reinterpret_cast<const u32*>(lSrc + 12)) - lSrc;

        CgsSceneManager::ZoneList::FixDown(lpRes, 0);
        std::memcpy(lpDst, lpResource, luSize);
        CgsSceneManager::ZoneList::FixUp(lpDst, reinterpret_cast<int>(lpDst));
        CgsSceneManager::ZoneList::FixUp(lpRes, static_cast<int>(lSrc));
        return lpDst;
    }
}
