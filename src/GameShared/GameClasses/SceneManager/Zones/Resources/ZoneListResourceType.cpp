#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ZoneListResourceType::FixDown                       @ 0x828D1E80
//   CgsResource::ZoneListResourceType::FixUp                         @ 0x828D1E70
//   CgsResource::ZoneListResourceType::GetSerialisedResourceDescriptor @ 0x828C34E0
//   CgsResource::ZoneListResourceType::GetTypeID                     @ 0x828AC420
//   CgsResource::ZoneListResourceType::Serialise                     @ 0x828D1E08
//
// Thin resource-type wrapper forwarding relocation to CgsSceneManager::ZoneList. The zone
// list's serialised footprint is {2 * count(+16) + dataBase(+12)} bytes (16-byte aligned).
// GetSerialisedResourceDescriptor emits the five-entry descriptor; Serialise relativises the
// source, copies the block, rebases the copy, then restores the source. ZoneList is a
// separate TU (forward-declared).

namespace CgsSceneManager
{
    struct ZoneList
    {
        void* FixDown(int liDelta = 0);
        void* FixUp(int liDelta);

        u8  mPad0[12];
        u32 muDataBase;   // +12  absolute base of the zone data
        u32 muCount;      // +16
    };
}

namespace CgsResource
{
    class ZoneListResourceType
    {
        typedef CgsSceneManager::ZoneList ZoneList;

    public:
        void* FixDown(void* pResource, const int* pDelta)
        {
            return static_cast<ZoneList*>(pResource)->FixDown(*pDelta);
        }

        void* FixUp(void* pResource, const int* pDelta)
        {
            return static_cast<ZoneList*>(pResource)->FixUp(*pDelta);
        }

        void* GetSerialisedResourceDescriptor(void* pOut, void* pUnused, void* pResource);
        int   GetTypeID() { return KI_TYPE_ID; }
        void* Serialise(void* pResource, int* pDestination);

    private:
        static const int KI_TYPE_ID = 45056;
    };

    void* ZoneListResourceType::GetSerialisedResourceDescriptor(void* pOut, void* /*pUnused*/, void* pResource)
    {
        const ZoneList* lpZones = static_cast<const ZoneList*>(pResource);
        u32* lpOut = static_cast<u32*>(pOut);

        lpOut[0] = 2 * lpZones->muCount + lpZones->muDataBase
                 - static_cast<u32>(reinterpret_cast<uintptr_t>(lpZones));
        lpOut[1] = 16;
        lpOut[2] = 0; lpOut[3] = 1;
        lpOut[4] = 0; lpOut[5] = 1;
        lpOut[6] = 0; lpOut[7] = 1;
        lpOut[8] = 0; lpOut[9] = 1;
        return pOut;
    }

    void* ZoneListResourceType::Serialise(void* pResource, int* pDestination)
    {
        ZoneList* lpSource = static_cast<ZoneList*>(pResource);
        uintptr_t lDest    = static_cast<uintptr_t>(*pDestination);

        uintptr_t lEnd  = 2 * lpSource->muCount + lpSource->muDataBase;
        size_t    luLen = lEnd - reinterpret_cast<uintptr_t>(lpSource);

        lpSource->FixDown();
        memcpy(reinterpret_cast<void*>(lDest), lpSource, luLen);
        reinterpret_cast<ZoneList*>(lDest)->FixUp(static_cast<int>(lDest));
        lpSource->FixUp(static_cast<int>(reinterpret_cast<uintptr_t>(lpSource)));

        return reinterpret_cast<void*>(lDest);
    }
}
