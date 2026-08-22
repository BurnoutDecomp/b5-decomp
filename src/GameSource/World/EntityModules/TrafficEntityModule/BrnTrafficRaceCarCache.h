#ifndef BRN_TRAFFIC_RACE_CAR_CACHE_H
#define BRN_TRAFFIC_RACE_CAR_CACHE_H

// ---------------------------------------------------------------------------
// BrnTraffic::RaceCarStateData -- DWARF BrnTrafficRaceCarCache.h:46. The per-frame snapshot of
// the active race cars the traffic sim reacts to. TrafficEntityModule::CacheRaceCarState
// @0x827185D0 fills it once per UpdateVehicles @0x82744F58, before the vehicle work is split
// across the TrafficJobStubs, and UpdateVehiclesJobParams carries a const pointer to it into
// the job (GameSource/Jobs/Traffic/TrafficCommon.h).
//
// The Feb-2007 build had no such struct: UpdateVehicles built the same four Append-grown lists
// on its own stack (BrnTrafficEntityModule.cpp:6976..:7025) and walked them inline. The ship
// hoisted them into this member and added the two parallel per-index arrays.
//
// HOST-NATIVE LAYOUT: member order is the DWARF's; nothing here is placed by a console offset.
// ---------------------------------------------------------------------------

#include "types.hpp"
#include "BrnCommonTypes.h"                             // Vector3, VecFloat
#include "GameShared/GameClasses/Containers/CgsArray.h" // ::Array<T,N>
#include "GameSource/BurnoutConstants.h"                // E_ACTIVE_RACE_CAR_INDEX_COUNT

namespace BrnTraffic
{
struct RaceCarStateData
{
    // The four Append-grown lists: one entry per race car that passed the active/not-rival
    // filter, so their live counts are <= E_ACTIVE_RACE_CAR_INDEX_COUNT and index-parallel
    // to each other, NOT to an EActiveRaceCarIndex.
    ::Array<Vector3,  E_ACTIVE_RACE_CAR_INDEX_COUNT> mRaceCarPositions;        // :56
    ::Array<Vector3,  E_ACTIVE_RACE_CAR_INDEX_COUNT> mRaceCarLinearVelocities; // :57
    ::Array<VecFloat, E_ACTIVE_RACE_CAR_INDEX_COUNT> mRaceCarSpeeds;           // :58
    ::Array<Vector3,  E_ACTIVE_RACE_CAR_INDEX_COUNT> mRaceCarXZVelocityDirs;   // :59

    // The two fixed tables, indexed BY EActiveRaceCarIndex.
    bool    mabRaceCarActive[E_ACTIVE_RACE_CAR_INDEX_COUNT];            // :62
    Vector3 maActiveRaceCarPositions[E_ACTIVE_RACE_CAR_INDEX_COUNT];    // :63
};
}

#endif
