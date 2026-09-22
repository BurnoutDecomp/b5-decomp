// FX-AIDRV regression G01-D1 (crash parity 2026-09-22): the production AICar::GetUsefulDirection,
// extracted verbatim from src/GameSource/World/AI/BrnAICar_Update.cpp by
// run_aidrv_useful_direction.py, checked against the ARTIST assembly @0x82770028.
//
// Console (vmx128.py raw fields): BOTH gates measure a Y-ZEROED copy and BOTH returns re-read the
// full 3D accessor:
//   0x82770050 bl GetVelocity -> sp+0x60 ; 0x827700A4 stfs f31 (flt_82001CC0 = 0.0) -> sp+0x64
//   (the copy's Y lane) ; 0x827700BC vmsum3fp128 v0,v0,v0 ; 0x827700FC vcmpgefp. |xz| >= 8.9408
//   (flt_8300D964, dyn-init 0x82C685B8 = 20 * 0.44704) -> 0x82770118 Normalize(GetVelocity()).
//   0x82770164 bl GetDirection -> sp+0x70 ; 0x827701A0 stfs f31 -> sp+0x74 (Y lane) ;
//   0x827701F4 vcmpgtfp. |xz| > 0.01 (flt_82002138) -> 0x82770210 GetDirection() into the sret.
//   0x8277027C Cross(GetDirection(), GetRight()) unflattened, returned unnormalised when > 0.01;
//   else (1,0,0,0) (flt_82001C98 = 1.0 into lane x at 0x827702EC).
// The fixtures are the three accessors (each returns the plain member it reads on the console:
// GetVelocity @0x8276B570 -> +0x1470, GetDirection @0x8276B488 -> +0x1440, GetRight -> +0x1450).
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnAI {
Vector3 AICar::GetVelocity() const  { return mVelocity; }
Vector3 AICar::GetDirection() const { return mDirection; }
Vector3 AICar::GetRight() const     { return mRight; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    Vector3 V(f32 lfX, f32 lfY, f32 lfZ) { return Vector3{ lfX, lfY, lfZ, 0.0f }; }
    bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-5f * (1.0f + std::fabs(lfB)); }
    bool NearV(const Vector3& lrA, const Vector3& lrB) { return Near(lrA.x, lrB.x) && Near(lrA.y, lrB.y) && Near(lrA.z, lrB.z); }
    Vector3 Unit(const Vector3& lrV)
    {
        const f32 lfLength = std::sqrt(lrV.x * lrV.x + lrV.y * lrV.y + lrV.z * lrV.z);
        return V(lrV.x / lfLength, lrV.y / lfLength, lrV.z / lfLength);
    }
    Vector3 Useful(const Vector3& lrVelocity, const Vector3& lrDirection, const Vector3& lrRight)
    {
        static AICar sCar{};
        sCar.mVelocity = lrVelocity;
        sCar.mDirection = lrDirection;
        sCar.mRight = lrRight;
        return sCar.GetUsefulDirection();
    }
}

int main()
{
    const Vector3 lFacingZ = V(0.0f, 0.0f, 1.0f);
    const Vector3 lRightX  = V(1.0f, 0.0f, 0.0f);

    // ---- gate 1 measures |(vx, 0, vz)| -------------------------------------------------------
    // A rival knocked into the air: 1 m/s across, 12 m/s down. |xz| = 1 < 8.9408, so the console
    // falls through to the facing; the full 3D magnitude (12.04) would have passed gate 1.
    Check(NearV(Useful(V(1.0f, -12.0f, 0.0f), lFacingZ, lRightX), lFacingZ),
          "vel (1,-12,0): |xz| 1 < 8.9408 -> the facing (0,0,1), not Normalize(1,-12,0)");
    // Landing: mostly vertical speed, 3 m/s x / 4 m/s z (|xz| = 5).
    const Vector3 lFacingDiag = V(0.6f, 0.0f, 0.8f);
    Check(NearV(Useful(V(3.0f, 50.0f, 4.0f), lFacingDiag, V(0.8f, 0.0f, -0.6f)), lFacingDiag),
          "vel (3,50,4): |xz| 5 < 8.9408 -> the facing (0.6,0,0.8)");
    // Just under the speed gate horizontally, with a small vertical component that lifts the 3D
    // magnitude over it: 8.9 across, 1.5 up (|3D| = 9.03 >= 8.9408, |xz| = 8.9 < 8.9408).
    Check(NearV(Useful(V(8.9f, 1.5f, 0.0f), lFacingZ, lRightX), lFacingZ),
          "vel (8.9,1.5,0): |xz| 8.9 < 8.9408 <= |3D| -> the facing");

    // ---- gate 1 passes: the RETURN is the unflattened velocity, normalised -------------------
    Check(NearV(Useful(V(9.0f, 0.0f, 0.0f), lFacingZ, lRightX), V(1.0f, 0.0f, 0.0f)),
          "vel (9,0,0): |xz| 9 >= 8.9408 -> Normalize(vel) = (1,0,0)");
    Check(NearV(Useful(V(6.0f, 20.0f, 7.0f), lFacingZ, lRightX), Unit(V(6.0f, 20.0f, 7.0f))),
          "vel (6,20,7): |xz| 9.22 >= 8.9408 -> Normalize of the FULL 3D velocity (0x82770118)");

    // ---- gate 2 measures |(dx, 0, dz)|, returns the full facing ------------------------------
    const Vector3 lFacingPitched = Unit(V(0.0f, 0.5f, 1.0f));
    Check(NearV(Useful(V(0.0f, 0.0f, 0.0f), lFacingPitched, lRightX), lFacingPitched),
          "stopped, facing pitched 26.6 deg: |xz| 0.894 > 0.01 -> the UNflattened facing (0x82770210)");
    // A facing within ~0.4 deg of vertical: |xz| = 0.00707 <= 0.01, so the console takes the
    // cross arm: Cross(dir, right) = (dy*rz - dz*ry, dz*rx - dx*rz, dx*ry - dy*rx) = (0, dz, -dy).
    const Vector3 lFacingUp = Unit(V(0.005f, 1.0f, 0.005f));
    const Vector3 lExpectedCross = V(lFacingUp.y * lRightX.z - lFacingUp.z * lRightX.y,
                                     lFacingUp.z * lRightX.x - lFacingUp.x * lRightX.z,
                                     lFacingUp.x * lRightX.y - lFacingUp.y * lRightX.x);
    Check(NearV(Useful(V(0.0f, 0.0f, 0.0f), lFacingUp, lRightX), lExpectedCross),
          "stopped, facing ~vertical: |xz| 0.00707 <= 0.01 -> Cross(dir, right) (0x8277027C), not the facing");
    // Same near-vertical facing while falling fast: gate 1 is also flattened, so still the cross.
    Check(NearV(Useful(V(0.5f, -30.0f, 0.5f), lFacingUp, lRightX), lExpectedCross),
          "falling at 30 m/s with |xz| 0.71: both gates flattened -> Cross(dir, right)");

    // ---- final fallback ------------------------------------------------------------------------
    Check(NearV(Useful(V(0.0f, 0.0f, 0.0f), V(0.0f, 0.0f, 0.0f), V(0.0f, 0.0f, 0.0f)), V(1.0f, 0.0f, 0.0f)),
          "no speed, no facing, no right -> (1,0,0) (flt_82001C98 into lane x)");

    Check(gAssertions == 0, "no assertion fired");
    std::printf("AIDrvUsefulDirection: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
