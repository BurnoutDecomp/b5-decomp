#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_ALL_VEHICLE_DATA_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_ALL_VEHICLE_DATA_H

#include <cstddef>                                        // offsetof (layout static_asserts)

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> (the NearestCarInfo,8 container)

// ============================================================================
// GameSource/Director/Utils/BrnDirectorAllVehicleData.h
//
// BrnDirector::AllVehicleData -- the director's per-frame snapshot of every tracked vehicle
// (the player car, the race cars, the traffic). HOME for the NearestCarInfo element type and
// the Array<NearestCarInfo, 8> container instantiation whose Append this TU bodies.
//
// ----------------------------------------------------------------------------
// The TU this header anchors is the Array<NearestCarInfo, 8>::Append instantiation
// @0x821FBA48 (the X360 names it BrnDirector::AllVehicleData::NearestCarInfo,8>::Append). The
// Append body itself is the generic Array<T,N>::Append in CgsArray.h; this header just gives
// that instantiation its element type (NearestCarInfo) and a real .cpp home.
//
// NearestCarInfo layout is pinned store-for-store from the Append asm (a 12-byte / 3-word
// element: the element pointer is computed as base + count*3words, then three 32-bit words are
// copied in) and from the caller AllVehicleData::Update @0x8221D938 (which assembles the three
// words as: a vehicle index, the squared distance to that car, and the per-team value indexed
// out of the vehicle-team array). Field roles are named from that caller; the OFFSETS/SIZE are
// authoritative. The rest of AllVehicleData (the full vehicle snapshot) lands with its own TU.
// ----------------------------------------------------------------------------

namespace BrnDirector
{

class AllVehicleData
{
public:

    // One "nearest car" record: a vehicle index, the squared distance to it, and the value
    // pulled from the per-vehicle team array for that vehicle. A 12-byte (3-word) POD; the
    // Append asm copies exactly three 32-bit words, so the three fields below are 4 bytes each
    // with no trailing padding (the container packs them at a 12-byte stride).
    struct NearestCarInfo
    {
        s32 miVehicleIndex;     // +0x00  the vehicle's index (v135[0] = v99 in Update)
        f32 mfDistanceSquared;  // +0x04  squared distance to that vehicle (the vmsum3fp128 dot)
        u32 muTeamValue;        // +0x08  per-vehicle team value (*(4*index + lpaVehicleTeams))
    };

    // The director keeps up to 8 nearest-car records (Array<NearestCarInfo, 8>; the inline
    // buffer is 8 * 12 = 0x60 bytes, then the trailing live-count word at +0x60 -- matching the
    // Append asm, which reads the count from +0x60 and computes element = base + count*3words).
    typedef Array<NearestCarInfo, 8> NearestCarInfoArray;

    // ---- nearest-car queries the director's FX path uses (X360-attested) -----------------
    // The squared distance from the player to the nearest tracked car (bubble-sorts the
    // nearest-car list on first call, then returns the front record's distance). @0x82233488.
    // X360 returns the float via the double FP-ABI; modelled as f32.
    f32 GetSqDistanceOfNearestCarToPlayer() const;

    // The squared distance to the nearest car that is NOT on liTeamId's team (the nearest
    // *opposing* member). @0x822334E0. X360 returns it via the result-pointer ABI; modelled as
    // a plain f32 return. FLAG: const-ness inferred (a read-only query that lazily sorts).
    f32 SqDistanceOfNearestOpposingTeamMember(s32 liTeamId) const;
};

// Pin the element size/field offsets to the Append asm (12-byte / 3-word element copied as
// three 32-bit words; element pointer = base + count*3words).
static_assert(sizeof(AllVehicleData::NearestCarInfo) == 12, "NearestCarInfo is a 12-byte element");
static_assert(offsetof(AllVehicleData::NearestCarInfo, miVehicleIndex)    == 0x00, "NearestCarInfo.+0x00");
static_assert(offsetof(AllVehicleData::NearestCarInfo, mfDistanceSquared) == 0x04, "NearestCarInfo.+0x04");
static_assert(offsetof(AllVehicleData::NearestCarInfo, muTeamValue)       == 0x08, "NearestCarInfo.+0x08");

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_DIRECTOR_ALL_VEHICLE_DATA_H
