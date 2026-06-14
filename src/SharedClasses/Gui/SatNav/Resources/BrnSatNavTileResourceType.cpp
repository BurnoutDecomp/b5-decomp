#include "SharedClasses/Gui/SatNav/Resources/BrnSatNavTileResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::SatNavTileDirectoryResourceType::FixDown/FixUp/GetTypeID  @ 0x8245F4A8 / 0x8245F4C0 / 0x824481A0
//   CgsResource::SatNavTileResourceType::FixDown/FixUp                     @ 0x8244F550 / 0x8244F590
//   CgsResource::SatNavTileResourceType::GetImportCount/GetImportPointer   @ 0x82448160 / 0x82448168
//   CgsResource::SatNavTileResourceType::GetTypeID                         @ 0x82448158
//
// The directory relocates a single pointer at +20. The tile holds an import table:
// count at +104, `count` pointers packed from +108 (4*27). FixUp/FixDown rebase each
// by the delta (the rw::Resource's load base); GetImportPointer returns one entry.

namespace CgsResource
{
    static const uint32_t KU_SAT_NAV_TILE_DIRECTORY_RESOURCE_TYPE_ID = 41;
    static const uint32_t KU_SAT_NAV_TILE_RESOURCE_TYPE_ID = 40;

    uint32_t SatNavTileDirectoryResourceType::GetTypeID() const
    {
        return KU_SAT_NAV_TILE_DIRECTORY_RESOURCE_TYPE_ID;
    }

    void SatNavTileDirectoryResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        *reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(lpResource) + 20) -= CgsResource::GetLoadBase(lrResource);
    }

    void SatNavTileDirectoryResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        *reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(lpResource) + 20) += CgsResource::GetLoadBase(lrResource);
    }

    uint32_t SatNavTileResourceType::GetTypeID() const
    {
        return KU_SAT_NAV_TILE_RESOURCE_TYPE_ID;
    }

    void SatNavTileResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);
        u32 luCount = *reinterpret_cast<u32*>(lBase + 104);
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            *reinterpret_cast<u32*>(lBase + 108 + 4 * luIndex) -= luDelta;
    }

    void SatNavTileResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        const u32 luDelta = CgsResource::GetLoadBase(lrResource);
        u32 luCount = *reinterpret_cast<u32*>(lBase + 104);
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            *reinterpret_cast<u32*>(lBase + 108 + 4 * luIndex) += luDelta;
    }

    uint32_t SatNavTileResourceType::GetImportCount(const void* lpResource) const
    {
        return *reinterpret_cast<const u32*>(reinterpret_cast<uintptr_t>(lpResource) + 104);
    }

    void SatNavTileResourceType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                                  uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lBase = reinterpret_cast<uintptr_t>(lpResource);
        u32 luCount = *reinterpret_cast<const u32*>(lBase + 104);
        if (luIndex >= luCount)
        {
            *lppValue = nullptr;
            *lpuOffset = 0;
        }
        else
        {
            u32 luOffset = 4 * (luIndex + 27);
            *lppValue = reinterpret_cast<const void*>(*reinterpret_cast<const u32*>(lBase + luOffset));
            *lpuOffset = luOffset;
        }
    }
}
