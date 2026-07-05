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
// The body @ 0x822B1FA0 is HOMED in BrnPowerParkingManager.cpp. The RwMathFPU
// TWO_PI/PI/HALF_PI constants are reproduced as the X360 rodata literals inline (no
// fabricated constants home), and BrnMath::GetPointToInfiniteLineDistance is an
// additive-grow declare-only in BrnMathUtils.h (its own body TU, link-deferred).
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
