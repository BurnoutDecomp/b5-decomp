#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::SatNavTileDirectoryResourceType::FixDown   @ 0x8245F4A8
//   CgsResource::SatNavTileDirectoryResourceType::FixUp     @ 0x8245F4C0
//   CgsResource::SatNavTileDirectoryResourceType::GetTypeID @ 0x824481A0
//   CgsResource::SatNavTileResourceType::FixDown            @ 0x8244F550
//   CgsResource::SatNavTileResourceType::FixUp              @ 0x8244F590
//   CgsResource::SatNavTileResourceType::GetImportCount     @ 0x82448160
//   CgsResource::SatNavTileResourceType::GetImportPointer   @ 0x82448168
//   CgsResource::SatNavTileResourceType::GetTypeID          @ 0x82448158
//
// The directory resource relocates a single pointer at offset 20. The tile resource
// holds an import table: a count at offset 104 and `count` import pointers packed
// from offset 108 (== 4*27). FixUp/FixDown rebase each import pointer by the delta;
// GetImportPointer returns the value/offset of one entry.

namespace CgsResource
{
    class SatNavTileDirectoryResourceType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta)
        {
            *reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(pResource) + 20) -= static_cast<u32>(*pDelta);
            return pResource;
        }
        void* FixUp(void* pResource, const int* pDelta)
        {
            *reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(pResource) + 20) += static_cast<u32>(*pDelta);
            return pResource;
        }
        int GetTypeID() { return 41; }
    };

    class SatNavTileResourceType
    {
    public:
        void* FixDown(void* pResource, const int* pDelta);
        void* FixUp(void* pResource, const int* pDelta);
        int   GetImportCount(const void* pResource) { return *reinterpret_cast<const int*>(reinterpret_cast<uintptr_t>(pResource) + 104); }
        void* GetImportPointer(void* pResource, u32 luIndex, u32* pOutOffset, u32* pOutValue);
        int   GetTypeID() { return 40; }
    };

    void* SatNavTileResourceType::FixDown(void* pResource, const int* pDelta)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);
        const int liDelta = *pDelta;
        u32 luCount = *reinterpret_cast<u32*>(lBase + 104);
        if (luCount)
        {
            u32* lpEntry = reinterpret_cast<u32*>(lBase + 108);
            u32 luIndex = 0;
            do
            {
                ++luIndex;
                *lpEntry++ -= static_cast<u32>(liDelta);
            } while (luIndex < luCount);
        }
        return pResource;
    }

    void* SatNavTileResourceType::FixUp(void* pResource, const int* pDelta)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);
        const int liDelta = *pDelta;
        u32 luCount = *reinterpret_cast<u32*>(lBase + 104);
        if (luCount)
        {
            u32* lpEntry = reinterpret_cast<u32*>(lBase + 108);
            u32 luIndex = 0;
            do
            {
                ++luIndex;
                *lpEntry++ += static_cast<u32>(liDelta);
            } while (luIndex < luCount);
        }
        return pResource;
    }

    void* SatNavTileResourceType::GetImportPointer(void* pResource, u32 luIndex, u32* pOutOffset, u32* pOutValue)
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(pResource);
        u32 luCount = *reinterpret_cast<u32*>(lBase + 104);
        if (luIndex >= luCount)
        {
            *pOutValue = 0;
            *pOutOffset = 0;
        }
        else
        {
            u32 luOffset = 4 * (luIndex + 27);
            *pOutValue = *reinterpret_cast<u32*>(lBase + luOffset);
            *pOutOffset = luOffset;
        }
        return pResource;
    }
}
