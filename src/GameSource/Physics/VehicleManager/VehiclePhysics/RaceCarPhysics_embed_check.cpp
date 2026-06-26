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

// ----- C10 group coverage: exercise the showtime/aftertouch/target-assist read-side methods + the
//       module-static singleton so the gate type-checks the new declarations. -----
bool RaceCarPhysics_c10_embed_check(RaceCarPhysics& lrCar)
{
    using namespace BrnPhysics::Vehicle;
    const bool lbInShowtime = lrCar.IsPlayerVehicleInShowtime();
    const bool lbUncapped   = lrCar.IsPlayerVehicleWithUncappedShowtimeSpeed();
    const bool lbBoosting   = lrCar.IsBounceBoosting();
    const bool lbUsingAT    = lrCar.IsUsingAftertouch();
    const bool lbShouldBoost = lrCar.ShouldBounceBoostNextImpact();
    const f32  lfStrength   = lrCar.GetShowtimePlayerCarStrength();

    // singleton reset + a couple of fields (the showtime home).
    msPlayerParams.Reset();
    const bool lbDisabled = msPlayerParams.mbDisableShowtime;

    return lbInShowtime && lbUncapped && lbBoosting && lbUsingAT && lbShouldBoost
        && (lfStrength >= 0.0f) && !lbDisabled;
}
