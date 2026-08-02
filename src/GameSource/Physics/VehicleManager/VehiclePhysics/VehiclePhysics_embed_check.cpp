// Embed check for the Vehicle-physics VehiclePhysics.h TU: include the home + exercise the
// header-homed bodied funcs (GetShowtimeDeformationScale, IsCounterSteeringAtLowSpeed) and the
// out-of-line GetDownForce (aero/downforce group).
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"

#include <type_traits>   // the re-parenting guard below

using BrnPhysics::Vehicle::VehiclePhysics;

// The re-parenting guard. VehiclePhysics was a FLAT struct until physics wave 2b; the DWARF
// (references/DecFIGS/.../VehiclePhysics.h:810) declares it
//     struct BrnPhysics::Vehicle::VehiclePhysics : public SimpleVehiclePhysics
// and 25 of its 53 members were duplicates of a base/own member, 20 of them under a different
// name. If the base link is ever lost, those duplicates come straight back -- so assert the chain
// itself. (BY-NAME parity only: per project rule no absolute console offset is static_asserted.)
static_assert(std::is_base_of<BrnPhysics::Vehicle::SimpleVehiclePhysics, VehiclePhysics>::value,
              "VehiclePhysics must derive from SimpleVehiclePhysics (DWARF VehiclePhysics.h:810)");
static_assert(std::is_base_of<BrnPhysics::ExternalPhysicsBody, VehiclePhysics>::value,
              "VehiclePhysics must inherit the ExternalPhysicsBody force/impulse accumulators");
static_assert(std::is_base_of<BrnPhysics::ExternallySimulatedBody, VehiclePhysics>::value,
              "VehiclePhysics must inherit ExternallySimulatedBody's mTransform/velocities");

bool VehiclePhysics_embed_check(const VehiclePhysics& lrPhysics, f32 lfSteering, f32 lfSpeed)
{
    const f64 lfScale = lrPhysics.GetShowtimeDeformationScale();
    const bool lbCounter = lrPhysics.IsCounterSteeringAtLowSpeed(lfSteering, lfSpeed);
    const Vector3 lDownForce = lrPhysics.GetDownForce();   // aero quadratic (flagged-inert)
    // surface-response group (per-wheel grip/roughness + vehicle linear drag; flagged-inert)
    // EVehicleDrivenWheel is homed at NAMESPACE scope (BrnSimpleVehiclePhysics.h:52) per the DWARF;
    // the nested VehiclePhysics::EVehicleDrivenWheel fork retired with the re-parenting.
    const Vector3 lGrip  = lrPhysics.GetSurfaceGrip(BrnPhysics::Vehicle::eFrontLeftWheel);
    const Vector3 lRough = lrPhysics.GetSurfaceRoughness(BrnPhysics::Vehicle::eRearRightWheel);
    const Vector3 lDrag  = lrPhysics.GetSurfaceLinearDrag();
    return lbCounter && (lfScale > 0.0) && (lDownForce.x >= 0.0f)
        && (lGrip.x >= 0.0f) && (lRough.x >= 0.0f) && (lDrag.x >= 0.0f);
}
