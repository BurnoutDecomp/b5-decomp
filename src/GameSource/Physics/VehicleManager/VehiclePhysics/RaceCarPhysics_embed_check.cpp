// Embed check for the Vehicle-physics RaceCarPhysics.h TU: include the home + exercise the three
// bodied funcs (IsPlayerVehicleActuallyInShowtime inline, IsCrashingNormally + GetHeightAboveRoad
// out-of-line in RaceCarPhysics.cpp).
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"

namespace BrnPhysics
{
namespace Vehicle
{
    // Tentative definition of the un-homed bounce-boost flag so the gate links cleanly. This is
    // ONLY a placeholder for the compile check; the real home is a future showtime/bounce TU.
    bool gbVehicleBounceBoosting = false;   // FLAG: un-homed module static (placeholder)
}
}

using BrnPhysics::Vehicle::RaceCarPhysics;

bool RaceCarPhysics_embed_check(const RaceCarPhysics& lrCar)
{
    const bool lbShowtime = lrCar.IsPlayerVehicleActuallyInShowtime();
    const bool lbNormal   = lrCar.IsCrashingNormally();
    const Vector3 lHeight = lrCar.GetHeightAboveRoad();
    return lbShowtime && lbNormal && (lHeight.x < 1.0e30f);
}
