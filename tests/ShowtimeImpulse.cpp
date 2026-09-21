// ARTIST contact-impulse regression: observe the production routine's two impulse outputs.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include <cstdio>
#include <cstdlib>
#include <cmath>

static Vector3 gLinearImpulse, gAngularImpulse;
namespace BrnPhysics {
void ExternalPhysicsBody::AddWorldSpaceImpulse(Vector3 lImpulse) { gLinearImpulse = lImpulse; }
void ExternalPhysicsBody::AddWorldSpaceAngularImpulse(Vector3 lImpulse) { gAngularImpulse = lImpulse; }
namespace Vehicle {
// Unrelated vtable entries must link but must never run in this fixture.
VecFloat SimpleVehiclePhysics::GetSteeringAngle() const { std::abort(); }
void SimpleVehiclePhysics::ClearCrashing() { std::abort(); }
void SimpleVehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::Update(VecFloat, VecFloat, const Matrix44Affine*, const BrnPlayerDriverControls*,
    bool, bool, bool, CgsNumeric::Random&) { std::abort(); }
void VehiclePhysics::UpdateSuspension(VecFloat) { std::abort(); }
void VehiclePhysics::SetCrashing() { std::abort(); }
void VehiclePhysics::ClearCrashing() { std::abort(); }
VecFloat VehiclePhysics::GetSteeringAngle() const { std::abort(); }
} }

static void Check(bool lbCondition, const char* lpcMessage)
{ if (!lbCondition) { std::fprintf(stderr, "FAIL: %s\n", lpcMessage); std::exit(1); } }

static void CheckVector(Vector3 lActual, Vector3 lExpected, const char* lpcMessage)
{
    if (!std::isfinite(lActual.x) || !std::isfinite(lActual.y) || !std::isfinite(lActual.z) ||
        std::fabs(lActual.x - lExpected.x) > 0.0001f ||
        std::fabs(lActual.y - lExpected.y) > 0.0001f ||
        std::fabs(lActual.z - lExpected.z) > 0.0001f)
    {
        std::fprintf(stderr, "FAIL: %s: got (%g,%g,%g), expected (%g,%g,%g)\n",
            lpcMessage, lActual.x, lActual.y, lActual.z, lExpected.x, lExpected.y, lExpected.z);
        std::exit(1);
    }
}

int main()
{
    using BrnPhysics::Vehicle::VehiclePhysics;
    using rw::physics::WORLD_SPACE;
    using rw::physics::BODY_SPACE;
    VehiclePhysics lVehicle;
    Matrix44Affine lTransform;
    lTransform.SetIdentity();
    lTransform.wAxis = {30, 40, 50, 0};
    lVehicle.SetTransform(lTransform);
    lVehicle.SetLinearVelocity({0, 0, 10, 0});
    lVehicle.miNumCollisions = 10;
    lVehicle.mi8NumWorldCollisions = 3;
    lVehicle.mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact = {1, 2, 3, 4};

    // A centre contact has no angular impulse. The console banks the full linear impulse.
    lVehicle.ApplyShowtimeContactImpulse({12, -5, 7, 0}, WORLD_SPACE, {30, 40, 50, 0}, WORLD_SPACE, false);
    CheckVector(gLinearImpulse, {12, -5, 7, 0}, "linear impulse is unchanged");
    CheckVector(gAngularImpulse, {0, 0, 0, 0}, "centre contact has no torque");
    Check(lVehicle.miNumCollisions == 11 && lVehicle.mi8NumWorldCollisions == 3, "car contact counters");
    Check(lVehicle.mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact.w == 4,
        "car contact leaves world-contact timer unchanged");

    // arm +Y crossed with impulse +Z produces +X. With velocity +Z, the filtered
    // axis is -X: the negative projection keeps 0.7 * 0.7 of this angular impulse.
    lVehicle.ApplyShowtimeContactImpulse({0, 0, 10, 0}, WORLD_SPACE, {30, 41, 50, 0}, WORLD_SPACE, true);
    CheckVector(gLinearImpulse, {0, 0, 10, 0}, "world contact retains full linear impulse");
    CheckVector(gAngularImpulse, {4.9f, 0, 0, 0}, "negative angular projection");
    Check(lVehicle.miNumCollisions == 12 && lVehicle.mi8NumWorldCollisions == 4, "world contact counters");
    const VecFloat& lrTimer = lVehicle.mvTimeSinceHardLanding_SteeringOverride_CarCarResponse_SecondsSinceLastWallContact;
    Check(lrTimer.x == 1 && lrTimer.y == 2 && lrTimer.z == 3 && lrTimer.w == 0, "world contact resets only timer W");

    lVehicle.ApplyShowtimeContactImpulse({0, 0, -10, 0}, WORLD_SPACE, {30, 41, 50, 0}, WORLD_SPACE, false);
    CheckVector(gAngularImpulse, {-0.147f, 0, 0, 0}, "positive angular projection loses 97 percent");
    lVehicle.SetLinearVelocity({0, 0, -10, 0});
    lVehicle.ApplyShowtimeContactImpulse({0, 0, 10, 0}, WORLD_SPACE, {30, 41, 50, 0}, WORLD_SPACE, false);
    CheckVector(gAngularImpulse, {0.147f, 0, 0, 0}, "reversing travel reverses filtered rotation");

    // Vertical/stationary travel has no horizontal axis; normalization must yield zero.
    lVehicle.SetLinearVelocity({0, 5, 0, 0});
    lVehicle.ApplyShowtimeContactImpulse({0, 0, -10, 0}, WORLD_SPACE, {30, 41, 50, 0}, WORLD_SPACE, false);
    CheckVector(gAngularImpulse, {-4.9f, 0, 0, 0}, "vertical travel keeps damped angular impulse");

    // Diagonal travel checks normalized-axis construction and the component that survives it.
    lVehicle.SetLinearVelocity({3, 4, 4, 0});
    lVehicle.ApplyShowtimeContactImpulse({0, 10, 0, 0}, WORLD_SPACE, {31, 40, 52, 0}, WORLD_SPACE, false);
    CheckVector(gAngularImpulse, {-1.43472f, 0, -1.37396f, 0}, "diagonal velocity angular projection");

    // Rotate the vehicle 90 degrees about Z and supply both arguments in body space.
    lTransform.xAxis = {0, 1, 0, 0};
    lTransform.yAxis = {-1, 0, 0, 0};
    lVehicle.SetTransform(lTransform);
    lVehicle.SetLinearVelocity({0, 0, 0, 0});
    lVehicle.ApplyShowtimeContactImpulse({2, 0, 0, 0}, BODY_SPACE, {0, 0, 1, 0}, BODY_SPACE, false);
    CheckVector(gLinearImpulse, {0, 2, 0, 0}, "body impulse rotates to world");
    CheckVector(gAngularImpulse, {-0.98f, 0, 0, 0}, "body position remains relative to COM");
    std::puts("PASS: Showtime linear/angular impulses, travel-direction filter, zero axis, spaces and counters");
}
