// Embed check for the Vehicle-physics VehiclePhysics.h TU: include the home + exercise the
// header-homed bodied funcs (GetShowtimeDeformationScale, IsCounterSteeringAtLowSpeed) and the
// out-of-line GetDownForce (aero/downforce group).
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

using BrnPhysics::Vehicle::VehiclePhysics;

bool VehiclePhysics_embed_check(const VehiclePhysics& lrPhysics, f32 lfSteering, f32 lfSpeed)
{
    const f64 lfScale = lrPhysics.GetShowtimeDeformationScale();
    const bool lbCounter = lrPhysics.IsCounterSteeringAtLowSpeed(lfSteering, lfSpeed);
    const Vector3 lDownForce = lrPhysics.GetDownForce();   // aero quadratic (flagged-inert)
    // surface-response group (per-wheel grip/roughness + vehicle linear drag; flagged-inert)
    const Vector3 lGrip  = lrPhysics.GetSurfaceGrip(VehiclePhysics::eFrontLeftWheel);
    const Vector3 lRough = lrPhysics.GetSurfaceRoughness(VehiclePhysics::eRearRightWheel);
    const Vector3 lDrag  = lrPhysics.GetSurfaceLinearDrag();
    return lbCounter && (lfScale > 0.0) && (lDownForce.x >= 0.0f)
        && (lGrip.x >= 0.0f) && (lRough.x >= 0.0f) && (lDrag.x >= 0.0f);
}
