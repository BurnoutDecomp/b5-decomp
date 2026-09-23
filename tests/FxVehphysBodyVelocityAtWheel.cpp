// FX-VEHPHYS (crash parity 2026-09-23, G54-D1): BrnPhysics::Vehicle::VehiclePhysics::
// CalculateBodyVelocityAtWheelContact @0x825FB200 -- the lever arm of an AIRBORNE wheel.
//
// run_fxvehphys_body_velocity_at_wheel.py EXTRACTS the production body from VehiclePhysics.cpp and
// compiles it into WheelVelocityFixture (maWheels / mTransform / mLinearVelocity / mAngularVelocity
// with their real types).
//
// Console facts checked (ARTIST asm):
//   0x825FB218 lbz wheel+0x28 (mbIsOnGround) -> ground arm / 0x825FB2B0 airborne arm
//   ground:   r = [wheel+0x00] RoadContact.mPosition - [this+0x40] mTransform.Pos()
//   airborne: r28 = wheel+0x80 (mPosition) ; 0x825FB3EC splat [wheel+0x40].w (radius) ;
//             0x825FB3FC vsubfp + 0x825FB400 vrlimi128 mask 4 -> local = (x, y - radius, z)
//             0x825FB418 Up*y ; 0x825FB424 + Right*x ; 0x825FB42C + At*z  (no this+0x40 load)
//   both:     vpermwi 0x63 / vmulfp128 / vnmsubfp -> omega(+0x60) x r ; + v(+0x50) ; stvx128 wheel+0xA0
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    struct WheelVelocityFixture
    {
        decltype(VehiclePhysics::maWheels)         maWheels{};
        decltype(VehiclePhysics::mTransform)       mTransform{};
        decltype(VehiclePhysics::mLinearVelocity)  mLinearVelocity{};
        decltype(VehiclePhysics::mAngularVelocity) mAngularVelocity{};

        void CalculateBodyVelocityAtWheelContact(EVehicleDrivenWheel leWheel, Vector3 lvRollDirection, VecFloat lvfTimeStep);
    };

#include "body_velocity_at_wheel.inc"
}
}

using namespace BrnPhysics::Vehicle;

static int giChecks = 0, giFailures = 0;
static void Lanes(const Vector3& lrGot, const double (&laWant)[3], const char* lpcName)
{
    const double laGot[3] = { lrGot.x, lrGot.y, lrGot.z };
    std::printf("%s: got (%.4f, %.4f, %.4f) want (%.4f, %.4f, %.4f)\n", lpcName,
                laGot[0], laGot[1], laGot[2], laWant[0], laWant[1], laWant[2]);
    for (int i = 0; i < 3; ++i)
    {
        ++giChecks;
        if (!(std::fabs(laGot[i] - laWant[i]) <= 1.0e-3))
        {
            ++giFailures;
            std::printf("FAIL: %s lane %c\n", lpcName, "xyz"[i]);
        }
    }
}

int main()
{
    // A car yawed 90 degrees, far from the origin: Right (0,0,-1), Up (0,1,0), At (1,0,0).
    WheelVelocityFixture lCar;
    lCar.mTransform.xAxis = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
    lCar.mTransform.yAxis = Vector3{ 0.0f, 1.0f,  0.0f, 0.0f };
    lCar.mTransform.zAxis = Vector3{ 1.0f, 0.0f,  0.0f, 0.0f };
    lCar.mTransform.wAxis = Vector3{ 1000.0f, 50.0f, -2000.0f, 0.0f };
    lCar.mLinearVelocity  = Vector3{ 5.0f, -2.0f, 30.0f, 0.0f };
    lCar.mAngularVelocity = Vector3{ 0.5f, 1.0f, -0.25f, 0.0f };

    // (a) wheel 2 airborne: local (-0.8, -0.3, -1.4), radius 0.35 -> (-0.8, -0.65, -1.4)
    //     r = Up*-0.65 + Right*-0.8 + At*-1.4 = (-1.4, -0.65, 0.8)
    //     omega x r = (0.6375, -0.05, 1.075) -> v = (5.6375, -2.05, 31.075)
    //     (the streamed-minus-Pos() arm gave (1991.0375, -751.1, 1005.675))
    Wheel& lrAir = lCar.maWheels[eRearLeftWheel];
    lrAir.mRoadContact.mbIsOnGround = false;
    lrAir.mPosition = Vector3{ -0.8f, -0.3f, -1.4f, 0.0f };
    lrAir.mStreamedPositionPlusTwistAmount = Vector3Plus{ -0.8f, -0.25f, -1.4f, 0.3f };
    lrAir.mSlipVariables.w = 0.35f;
    lCar.CalculateBodyVelocityAtWheelContact(eRearLeftWheel, Vector3{ 1.0f, 0.0f, 0.0f, 0.0f }, VecFloat{});
    Lanes(lCar.maWheels[eRearLeftWheel].mBodyPointVelocity, { 5.6375, -2.05, 31.075 },
          "(a) airborne wheel: r = R*(mPosition - (0,radius,0)), no translation");

    // (b) wheel 0 grounded: contact (1001, 49.6, -2001) -> r = (1, -0.4, -1)
    //     omega x r = (-1.1, 0.25, -1.2) -> v = (3.9, -1.75, 28.8)   (both bodies)
    Wheel& lrGround = lCar.maWheels[eFrontLeftWheel];
    lrGround.mRoadContact.mbIsOnGround = true;
    lrGround.mRoadContact.mPosition = Vector3{ 1001.0f, 49.6f, -2001.0f, 0.0f };
    lrGround.mPosition = Vector3{ 0.8f, -0.3f, 1.4f, 0.0f };
    lrGround.mSlipVariables.w = 0.35f;
    lCar.CalculateBodyVelocityAtWheelContact(eFrontLeftWheel, Vector3{ 1.0f, 0.0f, 0.0f, 0.0f }, VecFloat{});
    Lanes(lCar.maWheels[eFrontLeftWheel].mBodyPointVelocity, { 3.9, -1.75, 28.8 },
          "(b) grounded wheel: r = contact - Pos()");

    std::printf("FxVehphysBodyVelocityAtWheel: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
