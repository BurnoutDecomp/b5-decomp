#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (= rw::math::vpu::Vector3)

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h
//
// BrnWorld Power-Parking candidacy helper. DWARF home BrnPowerParkingManager.h:19
// (free function, extern, returns bool). The X360 signature is FOUR Vector3 by value
// (lPlayerPos, lPlayerDir, lVehiclePos, lVehicleDir) + FOUR f32& out/inout refs.
//
// DECLARATION-ONLY (the body @ 0x822B1FA0 is BLOCKED in this wave -- see the .cpp note):
// its angle-fold math depends on the RwMathFPU TWO_PI/PI/HALF_PI constants and on
// BrnMath::GetPointToInfiniteLineDistance, neither of which is homed in this batch.
// Emitting the body would require fabricating a RwMathFPU constants home (anti-fabrication
// rule forbids it). The signature is recorded here for the callers; the body lands once
// those dependencies are homed.
// ============================================================================
namespace BrnWorld
{
    // DWARF BrnPowerParkingManager.h:19 -- free Power-Parking candidacy test for one nearby
    // parked/target vehicle. FOUR Vector3 by value + FOUR const f32& out/inout refs.
    bool CheckVehicleForPowerPark( Vector3 lPlayerPos,
                                   Vector3 lPlayerDir,
                                   Vector3 lVehiclePos,
                                   Vector3 lVehicleDir,
                                   f32& lfClosestDistanceSq,
                                   f32& lfSecondClosestDistanceSq,
                                   f32& lfClosestAngleDiff,
                                   f32& lfClosestPerpendicularDist );
}
