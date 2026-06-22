// Embed check for the Vehicle-physics VehiclePhysics.h TU: include the home + exercise the two
// header-homed bodied funcs (GetShowtimeDeformationScale, IsCounterSteeringAtLowSpeed).
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

using BrnPhysics::Vehicle::VehiclePhysics;

bool VehiclePhysics_embed_check(const VehiclePhysics& lrPhysics, f32 lfSteering, f32 lfSpeed)
{
    const f64 lfScale = lrPhysics.GetShowtimeDeformationScale();
    const bool lbCounter = lrPhysics.IsCounterSteeringAtLowSpeed(lfSteering, lfSpeed);
    return lbCounter && (lfScale > 0.0);
}
