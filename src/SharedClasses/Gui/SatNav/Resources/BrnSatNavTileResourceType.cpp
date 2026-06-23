#include "SharedClasses/Gui/SatNav/Resources/BrnSatNavTileResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource + rw::BaseResourceDescriptors<5> complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::SatNavTileDirectoryResourceType::FixDown/FixUp/GetTypeID  @ 0x8245F4A8 / 0x8245F4C0 / 0x824481A0
//   CgsResource::SatNavTileDirectoryResourceType::GetSerialisedResourceDescriptor @ 0x824584E0
//   CgsResource::SatNavTileResourceType::FixDown/FixUp                     @ 0x8244F550 / 0x8244F590
//   CgsResource::SatNavTileResourceType::GetSerialisedResourceDescriptor   @ 0x82458440
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

    // X360 0x824584E0: a single-pool serialised descriptor. The resource's first u32 is the tile-entry
    // count; the main-memory pool needs 88 (0x58) bytes per entry plus a 56-byte (0x38) header. The
    // descriptor is returned by value (X360 sret r3=out): slot0 = {size = 88*count + 56, alignment = 1};
    // the remaining four serialised entries are seeded {size = 0, alignment = 1}.
    ResourceDescriptor SatNavTileDirectoryResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        CGS_ASSERT(lpResource != 0, "NULL resource passed to GetSerialisedResourceDescriptor");

        const u32 luEntryCount = *static_cast<const u32*>(lpResource);
        const u32 luMainSize   = 88u * luEntryCount + 56u;   // r9 = 0x58 * *a3 + 0x38

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luMainSize;   // *a1 (qword lo) = v5 + 56
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 1u;           // a1[1] = 1
        lDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;           // a1[2] = 0
        lDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;           // a1[3] = 1
        lDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;           // a1[4] = 0
        lDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;           // a1[5] = 1
        lDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;           // a1[6] = 0
        lDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;           // a1[7] = 1
        lDescriptor.m_baseResourceDescriptors[4].m_size      = 0u;           // asm stw r10(=0),0x20 -> slot4.size=0 (the "+56" was a Hex-Rays HIDWORD-alias artifact, not a store)
        lDescriptor.m_baseResourceDescriptors[4].m_alignment = 1u;           // a1[9] = 1
        return lDescriptor;
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

    // X360 0x82458440: a fixed-size single-pool serialised descriptor (the per-tile resource is a
    // constant 156-byte block). Returned by value (X360 sret r3=out): the four trailing serialised
    // entries are seeded {size = 0, alignment = 1}, then slot0 is overwritten with {size = 0x9C = 156,
    // alignment = 0x10 = 16} via the final qword store (lo dword -> +0 size = 156, hi dword -> +4
    // align = 16, on the big-endian image).
    ResourceDescriptor SatNavTileResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        CGS_ASSERT(lpResource != 0, "NULL resource passed to GetSerialisedResourceDescriptor");

        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 156u;  // *a1 (qword) -> {0x9C, 0x10}
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
        lDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;    // a1[2] = 0
        lDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;    // a1[3] = 1
        lDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;    // a1[4] = 0
        lDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;    // a1[5] = 1
        lDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;    // a1[6] = 0
        lDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;    // a1[7] = 1
        lDescriptor.m_baseResourceDescriptors[4].m_size      = 0u;    // a1[8] = 0
        lDescriptor.m_baseResourceDescriptors[4].m_alignment = 1u;    // a1[9] = 1
        return lDescriptor;
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
