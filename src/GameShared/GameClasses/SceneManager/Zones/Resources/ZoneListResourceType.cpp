#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ZoneListResourceType::FixDown                       @ 0x828D1E80
//   CgsResource::ZoneListResourceType::FixUp                         @ 0x828D1E70
//   CgsResource::ZoneListResourceType::GetSerialisedResourceDescriptor @ 0x828C34E0
//   CgsResource::ZoneListResourceType::GetTypeID                     @ 0x828AC420
//   CgsResource::ZoneListResourceType::Serialise                     @ 0x828D1E08
//
// A resource-type handler for a serialised CgsSceneManager::ZoneList blob.
// FixDown/FixUp just forward to the ZoneList's own pointer (un)relocation.
// Serialise relocates the source to file-relative pointers, copies the whole
// blob to the destination, then re-relocates both. GetSerialisedResourceDescriptor
// builds the five-entry rw descriptor whose payload size spans the blob.

namespace CgsSceneManager
{
    // Forward references to the ZoneList relocation helpers (own TUs); trap stubs
    // until those land. The blob pointer is returned for chaining.
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
    // The serialised ZoneList header: a leading block pointer at +12 and a zone
    // count at +16 (the blob payload runs to base + 2*count + headerPtr).
    struct ZoneListResource
    {
        u8  mPad0[12];
        u32 muBlockEnd;   // +12
        u32 muZoneCount;  // +16
    };

    class ZoneListResourceType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta);
        void* FixUp(void* pResource, const int* pDelta);
        void* GetSerialisedResourceDescriptor(void* pOut, const void* pResource);
        int   GetTypeID() { return KI_TYPE_ID; }
        void* Serialise(void* pResource, void** ppDest);

    private:
        static const int KI_TYPE_ID = 45056;
    };

    void* ZoneListResourceType::FixDown(void* pResource, const int* pDelta)
    {
        return CgsSceneManager::ZoneList::FixDown(pResource, *pDelta);
    }

    void* ZoneListResourceType::FixUp(void* pResource, const int* pDelta)
    {
        return CgsSceneManager::ZoneList::FixUp(pResource, *pDelta);
    }

    void* ZoneListResourceType::GetSerialisedResourceDescriptor(void* pOut, const void* pResource)
    {
        u32*      lpOut = reinterpret_cast<u32*>(pOut);
        uintptr_t lRes  = reinterpret_cast<uintptr_t>(pResource);

        u32 luZoneCount = *reinterpret_cast<const u32*>(lRes + 16);
        u32 luBlockEnd  = *reinterpret_cast<const u32*>(lRes + 12);

        lpOut[2] = 0; lpOut[3] = 1;
        lpOut[4] = 0; lpOut[5] = 1;
        lpOut[6] = 0; lpOut[7] = 1;
        lpOut[8] = 0; lpOut[9] = 1;

        // 64-bit first entry: high word = payload size, low word = alignment (16).
        lpOut[0] = static_cast<u32>(2 * luZoneCount + luBlockEnd - lRes);
        lpOut[1] = 16;
        return lpOut;
    }

    void* ZoneListResourceType::Serialise(void* pResource, void** ppDest)
    {
        uintptr_t lSrc  = reinterpret_cast<uintptr_t>(pResource);
        void*     lpDst = *ppDest;
        usize     luSize = (2 * *reinterpret_cast<const u32*>(lSrc + 16)
                            + *reinterpret_cast<const u32*>(lSrc + 12)) - lSrc;

        CgsSceneManager::ZoneList::FixDown(pResource, 0);
        std::memcpy(lpDst, pResource, luSize);
        CgsSceneManager::ZoneList::FixUp(lpDst, reinterpret_cast<int>(lpDst));
        CgsSceneManager::ZoneList::FixUp(pResource, static_cast<int>(lSrc));
        return *ppDest;
    }
}
