#pragma once

// Near-miss bookkeeping element type. DWARF home
// GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h.
//
// BrnWorld::NearMissData<KU_NUM_SECTIONS, KU_MAX_REMEMBERED> tracks recently
// near-missed / contacted / crashed / taken-down / checked traffic vehicles.
// Each "remembered" list is an inline Array<VehicleTimePair, N> of (entity-id, time)
// records that age out over time. VehicleTimePair is the per-record element type
// (DWARF BrnNearMissData.h:122-124): an entity id and the timestamp it was logged.
//
// sizeof(VehicleTimePair) == 8 is X360-authoritative: the
// Array<VehicleTimePair,4>::Append (@0x822AF0E0) and Array<VehicleTimePair,7>::Append
// (@0x822AF2C0) bodies index the inline buffer with an 8-byte stride (slwi r,count,3)
// and write the element as two 4-byte stores (the u32 id @0 + the f32 time @4); the
// live-count word follows the buffer (count @0x20 == 4*8 for the ,4 array; count
// @0x38 == 7*8 for the ,7 array).
#include "types.hpp"   // u32 / f32

namespace BrnWorld
{
    // One remembered traffic vehicle and the time it was logged. The DWARF spells the
    // members miEntityId (uint32_t) and mfTime (the project f32; the DWARF's float32_t
    // is the same 32-bit IEEE float).
    struct VehicleTimePair
    {
        u32 miEntityId;   // BrnNearMissData.h:123
        f32 mfTime;       // BrnNearMissData.h:124
    };
}
