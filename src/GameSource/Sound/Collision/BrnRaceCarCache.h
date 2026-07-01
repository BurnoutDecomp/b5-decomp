#ifndef BRN_SOUND_LOGIC_COLLISION_BRN_RACE_CAR_CACHE_H
#define BRN_SOUND_LOGIC_COLLISION_BRN_RACE_CAR_CACHE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3, Matrix44Affine
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"  // CgsSound::Utils::DataPoint<T>

// =============================================================================
// BrnSound::Logic::Collision::RaceCarCache
//   GameSource/Sound/Collision/BrnRaceCarCache.h (DWARF home BrnCollisionStateManager.h) +
//   GameSource/Sound/Collision/BrnRaceCarCache.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// RaceCarCache (DWARF BrnCollisionStateManager.h:230) is the fixed 8-entry per-race-car
// sound cache CollisionStateManager owns (mRaceCarCache @ BrnCollisionStateManager.h:630).
// It is a DISTINCT type from BrnTraffic::RaceCarStateData (BrnTrafficRaceCarCache.h);
// this one is BrnSound::Logic::Collision::RaceCarCache and lives one-struct-per-file
// beside the committed sibling BrnHingeStateCache (the committed BrnCollisionStateManager
// models the UNRELATED CollisionStateManager class as a deferred shell, not this cache).
//
// This TU's recon'd function set is ONE entry:
//   RaceCarCache::GetRaceCar  @ 0x82683068
// (RaceCarCache::Update is declared-only / DEFERRED -- it touches the un-homed
//  VehicleInterface / DeformationState surface; mirrors the BrnHingeStateCache.h
//  deferral pattern.)
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 RaceCarCacheNode stride is 192
// bytes; GetRaceCar subscripts BY NAME so the host sizeof drives the stride (no 192
// static_assert across the 32/64 ABI).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// BrnCollisionStateManager.h:129 (DWARF). Fixed 8-entry per-race-car sound cache.
struct RaceCarCache
{
    // BrnPhysics::Vehicle::ku8MaxNumRaceCars == 8. That symbol is NOT declared in-tree
    // (only referenced in assert message strings), so a LOCAL constant carries the value
    // in compiled conditions -- matching the committed sibling convention
    // (BrnBehaviourBystanderCam.h: `enum { KU_MAX_NUM_RACE_CARS = 8 };`, qualified name
    // kept in the message string only).
    static const u32 KU_MAX_NUM_RACE_CARS = 8;

    // BrnCollisionStateManager.h:132 (DWARF). One cached race-car state.
    // DWARF member ORDER (h:137,139,140,142,143):
    struct RaceCarCacheNode
    {
        Vector3                                    mComOffset;  // h:137  (16 B, quad)
        CgsSound::Utils::DataPoint<bool>           mbActive;    // h:139  (2 B)
        CgsSound::Utils::DataPoint<Matrix44Affine> mTransform;  // h:140  (128 B, 16-aligned)
        Vector3                                    mMin;        // h:142  (16 B)
        Vector3                                    mMax;        // h:143  (16 B)

        RaceCarCacheNode() {}   // BrnCollisionStateManager.h:133 (DWARF)
    };
    // X360 stride sizeof(RaceCarCacheNode) == 192 (attested by luIndex*192 in
    // GetRaceCar @ 0x82683068): 16 + 2 + pad(14) + 128 + 16 + 16 == 192. NOT
    // static_asserted (32-bit-vs-host-64-bit; GetRaceCar subscripts BY NAME).

    // BrnCollisionStateManager.h:154 (DWARF). @ 0x82683068 -- bodied in this TU.
    const RaceCarCacheNode* GetRaceCar( u32 luIndex ) const;

    // BrnCollisionStateManager.h:146 (DWARF).
    RaceCarCacheNode maRaceCars[KU_MAX_NUM_RACE_CARS];
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BRN_RACE_CAR_CACHE_H
