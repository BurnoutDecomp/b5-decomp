// Harness for run_fxdeformlat_joint_proportion.py (crash parity G28-D3, FX-DEFORM-LAT).
//
// The shipped PhysicalBodyPart::GetJointRotationProportion (BrnPhysicalBodyPart.cpp) and its Splat
// helper are pasted in through methods.inc and run on a REAL PhysicalBodyPart.
//
// ARTIST @0x825C1B38 is straight line after the IsJoinedToVehicle tripwire: est = vrefp(m); e1 =
// 1 - est*m ; y1 = est*e1 + est ; e2 = 1 - y1*m ; y2 = y1*e2 + y1 ; result = y2 * r
// (m = +0x180.w max joint angle, r = +0x160.w rotation). No compare or branch on m: at m = +-0 the
// result is NaN (inf*0), whatever r -- the tree returned 0, and a plain r/m would give +-inf.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

#include "methods.inc"

using namespace BrnPhysics::Deformation;
alignas(16) static unsigned char gPartStorage[sizeof(PhysicalBodyPart)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, float lfGot)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (got %g)\n", lpcName, lfGot); }
}
static float Proportion(float lfMaxAngle, float lfRotation)
{
    PhysicalBodyPart& lrPart = *reinterpret_cast<PhysicalBodyPart*>(gPartStorage);
    lrPart.mbJoinedToVehicle = true;
    lrPart.mLocalInitialComPositionPlusMaxJointAngle.SetPlus(lfMaxAngle);
    lrPart.mLocalJointPositionPlusRotation.SetPlus(lfRotation);
    const VecFloat lv = lrPart.GetJointRotationProportion();
    return (lv.x == lv.y && lv.y == lv.z && lv.z == lv.w) || std::isnan(lv.x) ? lv.x : -12345.0f;   // a splat
}
static bool WithinUlps(float a, float b, int n)
{
    int ia, ib; std::memcpy(&ia, &a, 4); std::memcpy(&ib, &b, 4);
    return (ia < 0) == (ib < 0) && std::abs(ia - ib) <= n;
}

int main()
{
    // The retail zero-angle joints carry -0.0f (SetJoinedToVehicle stores the spec's -0).
    float p = Proportion(-0.0f, 0.0f);   Check(std::isnan(p), "m = -0, r = 0 -> NaN (vrefp -inf, inf*0)", p);
    p = Proportion(-0.0f, 0.25f);        Check(std::isnan(p), "m = -0, r = 0.25 -> NaN (NOT -inf: a plain divide fails here)", p);
    p = Proportion(0.0f, -0.5f);         Check(std::isnan(p), "m = +0, r = -0.5 -> NaN", p);
    // m != 0: the refined reciprocal converges on the quotient (within 2 ulp of r/m).
    volatile float lfM = -1.047198f, lfR = -0.5f;
    p = Proportion(lfM, lfR);            Check(WithinUlps(p, lfR / lfM, 2), "m = -1.047198, r = -0.5 -> r/m within 2 ulp (control)", p);
    lfM = 0.5f; lfR = 0.3f;
    p = Proportion(lfM, lfR);            Check(WithinUlps(p, lfR / lfM, 2), "m = 0.5, r = 0.3 -> r/m within 2 ulp (control)", p);
    Check(giAsserts == 0, "joined part fires no tripwire (control)", 0.0f);
    std::printf("FxDeformLatJointProportion: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
