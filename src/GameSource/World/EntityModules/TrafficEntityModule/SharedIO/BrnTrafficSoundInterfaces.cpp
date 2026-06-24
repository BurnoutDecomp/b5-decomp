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

}
}
