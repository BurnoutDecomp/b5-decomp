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

// ----------------------------------------------------------------------------------------------
// THE RESET-CHAIN LINK WITNESS (2026-08-03).
//
// A green compile gate is NOT a green link: nothing in the tree calls Construct, Reset or
// SetWheelVelocities yet, so /OPT:REF discards their COMDATs and the exe's .map comes back with
// zero mentions of any of them. This function exists purely to force the linker to RESOLVE the
// chains
//     VehiclePhysics::Reset -> SetWheelVelocities -> Engine::Reset
//     VehiclePhysics::Reset -> SimpleVehiclePhysics::Reset / Wheel::Reset / SuspensionSpring::Reset
//     VehiclePhysics::Construct -> Wheel::Clear / SuspensionSpring::Prepare / Engine::Construct
//                               -> VehicleAttribs::Construct (x2) -> ... -> Reset
// so that an unresolved callee shows up as LNK2019 rather than as a surprise several waves later.
// (/OPT:REF does not suppress LNK2019 from a stripped COMDAT, so this measures closure even though
// the code itself is discarded.) It is never called at runtime.
//
// MEASURED 2026-08-03: the whole Reset chain resolved with zero LNK2019, and the file has been a
// STANDING mount in tools/build/build_game_exe.bat since (parent-repo commit be5f2fd, "build: mount
// VehiclePhysics_embed_check.cpp as a link-closure guard"). ⚠️ The sentence that stood here --
// "The mount line was then REVERTED" -- was wrong; the mount was kept, and it costs zero exe bytes.
// ⭐ TAMPER-TESTED 2026-08-03 (Construct wave), because "a stripped COMDAT still LNK2019s" had been
// an INHERITED claim, never checked. A call to a declared-but-undefined
// `BpTamperProbe_NoSuchSymbol_2026_08_03()` was added here and the exe rebuilt:
//     VehiclePhysics_embed_check.obj : error LNK2019: unresolved external symbol ...
//         referenced in function "void __cdecl VehiclePhysics_reset_link_check(...)"
//     Burnout_PC.exe : fatal error LNK1120: 1 unresolved externals
// So the witness really does measure closure even though /OPT:REF discards its code (the exe stays
// byte-size identical and the .map contains none of these symbols). The probe was then removed.
void VehiclePhysics_reset_link_check(VehiclePhysics& lrPhysics, Vector3 lvVelocity)
{
    lrPhysics.Construct();
    lrPhysics.SetWheelVelocities(lvVelocity);
    lrPhysics.Reset(lvVelocity);
}
