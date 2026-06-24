#include "SharedClasses/Sound/World/BrnStaticSoundMapResourceType.h"

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

// GetSerialisedResourceDescriptor @ 0x8267AFC0 (store-for-store). The serialised
// StaticSoundMap occupies one 16-byte-aligned block sized from the sub-region grid
// and the entity count. The X360 reads three serialised dwords:
//   r8 = miNumSubRegionsX (+0x28), r7 = miNumSubRegionsZ (+0x2C), r9 = miNumEntities (+0x34)
// and computes  size = 4 * ( miNumSubRegionsX * miNumSubRegionsZ + 4 * (miNumEntities + 4) ).
//   r10 = (miNumEntities + 4) << 2 ; r11 = miNumSubRegionsX * miNumSubRegionsZ
//   r11 = r11 + r10 ; r11 = r11 << 2     -> entry0 size ; entry0 align = 0x10 ; entries 1..4 {0,1}.
// Members are read BY NAME (private; StaticSoundMapResourceType is a friend struct);
// the X360 +0x28/+0x2C/+0x34 byte offsets are over the console 16-byte Vector2 layout.
CgsResource::ResourceDescriptor
StaticSoundMapResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
{
    const StaticSoundMap* lpMap = static_cast<const StaticSoundMap*>(lpResource);

    const u32 luGridCells  = static_cast<u32>(lpMap->miNumSubRegionsX)
                           * static_cast<u32>(lpMap->miNumSubRegionsZ);
    const u32 luEntityTerm = (static_cast<u32>(lpMap->miNumEntities) + 4u) << 2;  // 4*(miNumEntities+4)
    const u32 luSize       = (luGridCells + luEntityTerm) << 2;                    // *4

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
// load base (X360 *a3 = CgsResource::GetLoadBase(lrResource)). The X360 accessed
// the resource by dword index (a2[9]=mpSubRegions, a2[12]=mpEntities,
// a2[13]=miNumEntities under the console 16-byte Vector2); NAMED member access here
// relocates the right fields regardless of the PC Vector2 byte size (semantic
// parity, not byte parity). Then a per-entity validation loop.
void StaticSoundMapResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
{
    StaticSoundMap* lpMap = static_cast<StaticSoundMap*>(lpResource);

    const u32 luBase = CgsResource::GetLoadBase(lrResource);

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
