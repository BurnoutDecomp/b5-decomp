#include "SharedClasses/Sound/World/BrnStaticSoundMapResourceType.h"

#include "types.hpp"
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
