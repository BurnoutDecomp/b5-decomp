#include "SharedClasses/Sound/World/BrnStaticSoundMapResourceType.h"

#include <cstddef>   // native field offsets

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the descriptor body
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnSound
{
namespace World
{

// Resource registry type id for the StaticSoundMap resource (0x10016).
// Recovered verbatim from GetTypeID @ 0x826755F8.
static const u32 KU_STATIC_SOUND_MAP_RESOURCE_TYPE_ID = 65558;

// GetTypeID @ 0x826755F8 — `return 65558;` (the DWARF attests virtual uint32_t).
uint32_t StaticSoundMapResourceType::GetTypeID() const
{
    return KU_STATIC_SOUND_MAP_RESOURCE_TYPE_ID;
}

// GetSerialisedResourceDescriptor @ 0x8267AFC0. The serialised StaticSoundMap
// occupies one 16-byte-aligned block: header + 16-byte entities + 4-byte grid
// cells. The X360 reads three serialised dwords:
//   r8 = miNumSubRegionsX (+0x28), r7 = miNumSubRegionsZ (+0x2C), r9 = miNumEntities (+0x34)
// and computes  size = 4 * ( miNumSubRegionsX * miNumSubRegionsZ + 4 * (miNumEntities + 4) )
//             ==  0x40 (the console header) + 16*miNumEntities + 4*gridCells.
// Native header size is pinned below; the records keep their original strides.
CgsResource::ResourceDescriptor
StaticSoundMapResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const StaticSoundMap* lpMap = static_cast<const StaticSoundMap*>(lpResource);

    const u32 luGridCells  = static_cast<u32>(lpMap->miNumSubRegionsX)
                           * static_cast<u32>(lpMap->miNumSubRegionsZ);
    const u32 luSize       = static_cast<u32>(sizeof(StaticSoundMap))      // X360: 0x40
                           + (static_cast<u32>(lpMap->miNumEntities) << 4) // 16-byte entities
                           + (luGridCells << 2);                           // 4-byte grid cells

    CgsResource::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;  // entry0 size
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;   // entry0 align
    for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
    {
        lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
        lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
    }
    return lDescriptor;
}

// ARTIST826775C8 adds resource segment0 to both stored offsets, then validates entities.
void StaticSoundMapResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, mMin) == 0x00), "ARTIST min");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, mMax) == 0x10), "ARTIST max");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, mfSubRegionSize) == 0x20), "ARTIST cell size");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, mpSubRegions) == 0x24), "ARTIST cells");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, miNumSubRegionsX) == 0x28), "ARTIST grid X");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, miNumSubRegionsZ) == 0x2c), "ARTIST grid Z");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, mpEntities) == 0x30), "ARTIST entities");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, miNumEntities) == 0x34), "ARTIST entity count");
    CGS_STATIC_ASSERT((offsetof(StaticSoundMap, meRootType) == 0x38), "ARTIST root type");
    CGS_STATIC_ASSERT((sizeof(StaticSoundMap) == 0x40), "ARTIST map extent");

    StaticSoundMap* lpMap = static_cast<StaticSoundMap*>(lpResource);

    const u32 luBase = reinterpret_cast<u32>(lrResource.m_baseResources[0]);
    lpMap->mpEntities = reinterpret_cast<const StaticSoundEntity*>(
        reinterpret_cast<u32>(lpMap->mpEntities) + luBase);
    lpMap->mpSubRegions = reinterpret_cast<const SubRegionDescriptor*>(
        reinterpret_cast<u32>(lpMap->mpSubRegions) + luBase);

    // Per-entity validation. The X360 loop body is just the inlined Array-access
    // null + bounds asserts (per-entity work inlined away), so in retail it is a
    // no-op; reconstructed faithfully.
    for (s32 liEntityIndex = 0; liEntityIndex < lpMap->miNumEntities; ++liEntityIndex)
    {
        CGS_ASSERT(lpMap->mpEntities != 0, "mpEntities");
        CGS_ASSERT(liEntityIndex < lpMap->miNumEntities, "liEntityIndex < miNumEntities");
    }
}

}
}
