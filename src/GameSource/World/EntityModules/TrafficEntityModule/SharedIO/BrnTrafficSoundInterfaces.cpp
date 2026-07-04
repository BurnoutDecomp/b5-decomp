// =============================================================================
// BrnTrafficSoundInterfaces.cpp  (owning .cpp for TrafficSoundOutputInterface)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Bodies the two attested members of the
// traffic->sound output buffer:
//   BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface::AddTrafficEntity     @ 0x827107A8
//   BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface::GetTrafficEntityCount @ 0x82681E70
// Layout in BrnTrafficSoundInterfaces.h.
// =============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"

namespace BrnTraffic
{
namespace BrnTrafficIO
{

// X360 @ 0x827107A8. Append-one-entity into the fixed 32-slot list IF there is room
// for one more (the guard tests count+1 < KU_MAX_ENTITIES, i.e. count < 31). The store
// is an 80-byte memberwise copy of the TrafficSoundEntity into maActiveEntityList[count]
// (4x stvx128 for the 64-byte transform + the trailing scalar/u8 fields), then count++.
// The element address it writes is base + count*80 + 0x10, which is &maActiveEntityList[count].
void TrafficSoundOutputInterface::AddTrafficEntity(const TrafficSoundEntity& lrEntity)
{
    const u16 lu16Next = static_cast<u16>(mu16EntityCount + 1);
    if (lu16Next < KU_MAX_ENTITIES)
    {
        maActiveEntityList[mu16EntityCount] = lrEntity;
        mu16EntityCount = lu16Next;
    }
}

// X360 @ 0x82681E70. Bounds-asserts the live count against KU_MAX_ENTITIES, then returns it.
u16 TrafficSoundOutputInterface::GetTrafficEntityCount() const
{
    CGS_ASSERT(mu16EntityCount < KU_MAX_ENTITIES, "mu16EntityCount < KU_MAX_ENTITIES");
    return mu16EntityCount;
}

// X360 @ 0x82681EC8. Linear scan of the live maActiveEntityList[0..mu16EntityCount) for the
// entry whose mu16EntityIndex (@0x48 in TrafficSoundEntity) equals lu16Index, returning a
// pointer to it (&maActiveEntityList[index]), or nullptr if the count is zero or no entry
// matches. The loop bound is the raw count (the key compare uses a zero-extended u16).
// Callers: BrnSound::Logic::Collision::CollisionStateManager::FindEntity, ::MapEntityIdToMaterial.
const TrafficSoundEntity* TrafficSoundOutputInterface::GetTrafficEntityIndex(u16 lu16Index) const
{
    if (mu16EntityCount == 0)
        return nullptr;

    u32 luIndex = 0;
    while (maActiveEntityList[luIndex].mu16EntityIndex != lu16Index)
    {
        ++luIndex;
        if (luIndex >= mu16EntityCount)
            return nullptr;
    }
    return &maActiveEntityList[luIndex];
}

// X360 @ 0x823A7F18. TrafficSoundOutputInterface copy-assignment. Copies the live count
// (mu16EntityCount @0) then whole-copies all KU_MAX_ENTITIES (32) TrafficSoundEntity records
// of maActiveEntityList (X360 unrolls 8 entities/loop x 4, element stride 0x50 == 80).
// Returns *this. Callers: BrnSound::Module::Io::RootInputBuffer::SetTrafficOutputInterface,
// BrnWorldIO::UpdateOutputBuffer::SetTrafficSoundOutputInterface.
TrafficSoundOutputInterface& TrafficSoundOutputInterface::operator=(
    const TrafficSoundOutputInterface& lrSource)
{
    mu16EntityCount = lrSource.mu16EntityCount;                     // @0x00
    for (u32 luIndex = 0; luIndex < KU_MAX_ENTITIES; ++luIndex)     // 32 entities x 80B
        maActiveEntityList[luIndex] = lrSource.maActiveEntityList[luIndex];
    return *this;
}

}
}
