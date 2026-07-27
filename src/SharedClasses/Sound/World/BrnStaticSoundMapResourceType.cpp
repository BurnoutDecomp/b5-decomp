#include "SharedClasses/Sound/World/BrnStaticSoundMapResourceType.h"

#include <cstddef>   // offsetof (porter-contract asserts)

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete for the descriptor body
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"  // CgsResource::GetLoadBase
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
// The x64 serialised header is the host StaticSoundMap (0x50 -- the porter
// contract asserted in FixUp below), so the header term is sizeof-derived; the
// entity/grid terms are unchanged (both records are pointer-free).
// entry0 align = 0x10; entries 1..4 = {0,1}. Members are read BY NAME (private;
// StaticSoundMapResourceType is a friend struct).
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

// FixUp @ 0x826775C8. A relocation fix-up: the two on-disk pointers (mpSubRegions,
// mpEntities) are stored as load-relative offsets and are rebased by the resource
// load base (X360 *a3 = the 32-bit load base; the x64 heap base needs the full-width
// form). The X360 accessed the resource by dword index (a2[9]=mpSubRegions,
// a2[12]=mpEntities, a2[13]=miNumEntities under the console 16-byte Vector2); NAMED
// member access here relocates the right fields over the x64 layout (semantic
// parity, not byte parity). Then a per-entity validation loop.
void StaticSoundMapResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    // Porter-contract pins (world_type_transcode.py transcode_staticsoundmap): the host
    // layout below is the platform-4 serialised header form.
    static_assert(offsetof(StaticSoundMap, mMin)             == 0x00, "StaticSoundMap::mMin @ +0");
    static_assert(offsetof(StaticSoundMap, mMax)             == 0x10, "StaticSoundMap::mMax @ +0x10");
    static_assert(offsetof(StaticSoundMap, mfSubRegionSize)  == 0x20, "StaticSoundMap::mfSubRegionSize @ +0x20");
    static_assert(offsetof(StaticSoundMap, mpSubRegions)     == 0x28, "StaticSoundMap::mpSubRegions @ +0x28");
    static_assert(offsetof(StaticSoundMap, miNumSubRegionsX) == 0x30, "StaticSoundMap::miNumSubRegionsX @ +0x30");
    static_assert(offsetof(StaticSoundMap, mpEntities)       == 0x38, "StaticSoundMap::mpEntities @ +0x38");
    static_assert(offsetof(StaticSoundMap, miNumEntities)    == 0x40, "StaticSoundMap::miNumEntities @ +0x40");
    static_assert(sizeof(StaticSoundMap) == 0x50, "StaticSoundMap header 0x50 (porter contract)");

    StaticSoundMap* lpMap = static_cast<StaticSoundMap*>(lpResource);

    const uintptr_t luBase = CgsResource::GetLoadBase64(lrResource);

    // Relocate the two serialised pointers by the load base.
    lpMap->mpSubRegions = reinterpret_cast<const SubRegionDescriptor*>(
        reinterpret_cast<uintptr_t>(lpMap->mpSubRegions) + luBase);
    lpMap->mpEntities = reinterpret_cast<const StaticSoundEntity*>(
        reinterpret_cast<uintptr_t>(lpMap->mpEntities) + luBase);

    // Per-entity validation. The X360 loop body is just the inlined Array-access
    // null + bounds asserts (per-entity work inlined away), so in retail it is a
    // no-op; reconstructed faithfully.
    for (s32 liEntityIndex = 0; liEntityIndex < lpMap->miNumEntities; ++liEntityIndex)
    {
        CGS_ASSERT(lpMap->mpEntities != nullptr, "mpEntities");
        CGS_ASSERT(liEntityIndex < lpMap->miNumEntities, "liEntityIndex < miNumEntities");
    }
}

}
}
