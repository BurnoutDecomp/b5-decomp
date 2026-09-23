// FX-VEHPHYS (crash parity 2026-09-23, G51-D2): BrnPhysics::Vehicle::VehiclePhysics::
// UpdateLinearVelocityMagnitude @0x825C0000 -- the cached direction + speed at +0x1340.
//
// run_fxvehphys_linear_velocity_magnitude.py EXTRACTS the production body from VehiclePhysics.cpp
// and compiles it into SpeedFixture (mLinearVelocity + mNormLinearVelocityMag, real types).
//
// Console facts checked (ARTIST asm):
//   0x825C003C stvx128 v12(=0) -> +0x1340 zeroed
//   0x825C0028 vmsum3fp128 |v|^2 ; 0x825C0058..0088 rsqrt + 2 Newton ; |v| = |v|^2 * rsqrt
//   0x825C008C vsel -> 0 where vcmpeqfp(0, |v|^2)
//   0x825C0090 vrlimi128 v6,v0,1 ; 0x825C0098 stvx128 -> +0x1340 = (0, 0, 0, |v|)
//   0x825C009C lvlx stru_8208F620 (lane 0 = 0x34000000 = FLT_EPSILON) ; 0x825C00A8 vandc (abs)
//   0x825C00AC vcmpgtfp |v| > eps ; 0x825C00C0 beqlr -> RETURN with xyz = 0 when not
//   0x825C00DC vrefp + 2 Newton ; 0x825C00F0 v * (1/|v|) ; 0x825C00F4 vrlimi128 w = |v| ; stvx128
// A NaN |v|^2 is not == 0, so the vsel keeps |v| = NaN, and abs(NaN) > eps is false: (0,0,0,NaN).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    struct SpeedFixture
    {
        decltype(VehiclePhysics::mLinearVelocity)        mLinearVelocity{};
        decltype(VehiclePhysics::mNormLinearVelocityMag) mNormLinearVelocityMag{ 999.0f, 999.0f, 999.0f, 999.0f };

        void UpdateLinearVelocityMagnitude();
    };

#include "linear_velocity_magnitude.inc"
}
}

using BrnPhysics::Vehicle::SpeedFixture;

static int giChecks = 0, giFailures = 0;

static bool Near(float lfGot, float lfWant)
{
    if (std::isnan(lfWant))
        return std::isnan(lfGot);
    return std::fabs(lfGot - lfWant) <= 1.0e-6f * (1.0f + std::fabs(lfWant)) &&
           (lfWant != 0.0f || lfGot == 0.0f);
}

static void Case(const char* lpcName, Vector3 lvVelocity, const float (&lafWant)[4])
{
    SpeedFixture lCar;
    lCar.mLinearVelocity = lvVelocity;
    lCar.UpdateLinearVelocityMagnitude();
    const float lafGot[4] = { lCar.mNormLinearVelocityMag.x, lCar.mNormLinearVelocityMag.y,
                              lCar.mNormLinearVelocityMag.z, lCar.mNormLinearVelocityMag.w };
    bool lbPass = true;
    for (int i = 0; i < 4; ++i)
        lbPass = lbPass && Near(lafGot[i], lafWant[i]);
    ++giChecks;
    std::printf("%s: got (%g, %g, %g, %g) want (%g, %g, %g, %g)\n", lpcName,
                lafGot[0], lafGot[1], lafGot[2], lafGot[3], lafWant[0], lafWant[1], lafWant[2], lafWant[3]);
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

int main()
{
    const float kfNaN = std::numeric_limits<float>::quiet_NaN();

    // Below the FLT_EPSILON gate: the direction lanes stay 0 (0x825C00C0 beqlr).
    Case("(1) |v| = 1e-8: xyz stay 0", Vector3{ 1.0e-8f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0e-8f });
    Case("(2) |v| = 1e-7 (6e-8, 8e-8): xyz stay 0", Vector3{ 6.0e-8f, 8.0e-8f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0e-7f });
    // Controls: zero speed, a normal speed, and a speed just past the gate.
    Case("(3) |v| = 0: all zero", Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f });
    Case("(4) (3,0,4): direction (0.6,0,0.8), |v| 5", Vector3{ 3.0f, 0.0f, 4.0f, 7.0f }, { 0.6f, 0.0f, 0.8f, 5.0f });
    Case("(5) |v| = 3e-7 > eps: direction (1,0,0)", Vector3{ 3.0e-7f, 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 3.0e-7f });
    // A NaN speed: the vsel keeps NaN in w, and the gate (abs(NaN) > eps is false) keeps xyz 0.
    Case("(6) NaN velocity: (0,0,0,NaN)", Vector3{ kfNaN, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, kfNaN });

    std::printf("FxVehphysLinearVelocityMagnitude: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
