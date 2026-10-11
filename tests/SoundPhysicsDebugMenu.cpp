// ARTIST825A2E88: asymmetric lane values catch the hold/spin VMX insert masks.
// Actual body and RenderWare getters are extracted by the runner; types are real.
#include <cmath>
#include <cstdio>
#include <cstring>
#include "GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.h"

#include "sound_physics_debug_menu.inc"

static int sChecks = 0, sFailures = 0;
static void Check(bool result, const char* name) {
    ++sChecks;
    if (!result) { ++sFailures; std::printf("FAIL: %s\n", name); }
}
static bool Equal(Vector3 a, Vector3 b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
static bool Equal(Matrix44Affine a, Matrix44Affine b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
static bool Near(f32 a, f32 b) { return std::fabs(a-b) < .00001f; }

int main() {
    using namespace BrnPhysics;
    rw::physics::RigidBody rigid{};
    rigid.mRi = {1,2,3,4}; rigid.mUp = {5,6,7,8}; rigid.mAt = {9,10,11,12};
    rigid.mCom = {13,14,15,16}; rigid.mVel = {17,18,19,20}; rigid.mOmega = {21,22,23,24};
    const Matrix44Affine current = rigid.GetTransform();
    const Vector3 linear = rigid.GetLinearVelocity(), angular = rigid.GetAngularVelocity();
    ExternallySimulatedBody body;
    body.ReadFromRenderware(&rigid);
    Check(Equal(body.GetTransform(), current), "normal path copies all transform lanes");
    Check(Equal(body.GetLinearVelocity(), linear) && Equal(body.GetAngularVelocity(), angular),
          "normal path copies both complete velocity registers");

    Matrix44Affine previous = current;
    previous.wAxis = {101,102,103,104};
    body.SetTransform(previous);
    KI_HOLD_CAR_IN_PLACE = 1;
    body.ReadFromRenderware(&rigid);
    Matrix44Affine held = current;
    held.wAxis = {101,14,103,104};
    Check(Equal(body.GetTransform(), held), "hold retains previous X/Z/W and current physics Y");
    Check(Equal(body.GetLinearVelocity(), linear) && Equal(body.GetAngularVelocity(), angular),
          "hold preserves normal velocity updates");

    KI_HOLD_CAR_SPINING_IN_AIR = 1;
    KF_SPINNING_SPEED_X = -.25f; KF_SPINNING_SPEED_Y = .5f; KF_SPINNING_SPEED_Z = 2.f;
    body.ReadFromRenderware(&rigid);
    Matrix44Affine spinning = current;
    spinning.wAxis = {101,10,103,104};
    Check(Equal(body.GetTransform(), spinning), "spin takes priority over hold and forces Y=10");
    Check(Equal(body.GetLinearVelocity(), Vector3{0,0,0,0}), "spin clears every linear velocity lane");
    const Vector3 spin = body.GetAngularVelocity();
    Check(Near(spin.x, -1.570796375f) && Near(spin.y, 3.14159275f) &&
          Near(spin.z, 12.566371f) && spin.w == 0, "three live spin speeds use original 2pi multiplier");

    body.SetFrozen(true);
    body.SetTransform(previous); body.SetLinearVelocity(linear); body.SetAngularVelocity(angular);
    body.ReadFromRenderware(nullptr);
    Check(Equal(body.GetTransform(), previous) && Equal(body.GetLinearVelocity(), linear) &&
          Equal(body.GetAngularVelocity(), angular), "frozen early return precedes debug flags and body dereference");
    body.SetFrozen(false);
    KI_HOLD_CAR_IN_PLACE = 0; KI_HOLD_CAR_SPINING_IN_AIR = 0;
    body.ReadFromRenderware(&rigid);
    Check(Equal(body.GetTransform(), current) && Equal(body.GetLinearVelocity(), linear) &&
          Equal(body.GetAngularVelocity(), angular), "cleared controls resume normal mirroring");
    std::printf("SoundPhysicsDebugMenu: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures ? 1 : 0;
}
