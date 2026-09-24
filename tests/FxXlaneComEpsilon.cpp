// Harness for run_fxxlane_com_epsilon.py (crash parity G39-D1; FX-XLANE).
//
// StreamedDeformationSpec::TransformToNewCOMSpace @0x825E3148 and the TU's anonymous-namespace
// constants, extracted from the real source, run on a zero-filled StreamedDeformationSpec with one
// deformation sensor and no tag / driven / IK points.
//   0x825E3164 lvlx v13 <- .rdata stru_8208F620 ; 0x825E3168 vspltw v12,v13,0
//     -> splat(x360rd 0x34000000 = 1.1920929e-07 = FLT_EPSILON)
//   0x825E3190 vmsum3fp128 |old-new|^2 ; 0x825E3194 vandc (sign mask) ;
//   0x825E3198 vcmpgtfp (strict >) ; 0x825E31AC beqlr
// => the re-frame runs iff |oldCOM - newCOM|^2 > FLT_EPSILON; equality and NaN skip.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int giAsserts = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnPhysics { namespace Deformation {
#include "anon.inc"
#include "methods.inc"
} }

using namespace BrnPhysics::Deformation;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

alignas(16) static unsigned char gStorage[sizeof(StreamedDeformationSpec)];

static StreamedDeformationSpec* Fresh()
{
    std::memset(gStorage, 0, sizeof(gStorage));
    StreamedDeformationSpec* lpSpec = reinterpret_cast<StreamedDeformationSpec*>(gStorage);
    lpSpec->mu8NumDeformationSensors = 1;
    return lpSpec;
}

static bool Untouched(const StreamedDeformationSpec* lpSpec)
{
    return lpSpec->mCurrentCOMOffset.x == 0.0f && lpSpec->mCurrentCOMOffset.y == 0.0f
        && lpSpec->mCurrentCOMOffset.z == 0.0f && lpSpec->mMeshOffset.x == 0.0f
        && lpSpec->mMeshOffset.y == 0.0f && lpSpec->mMeshOffset.z == 0.0f
        && lpSpec->maDeformationSensorSpecs[0].mInitialOffset.x == 0.0f
        && lpSpec->maDeformationSensorSpecs[0].mInitialOffset.y == 0.0f
        && lpSpec->maDeformationSensorSpecs[0].mInitialOffset.z == 0.0f;
}

int main()
{
    // (a) |d|^2 = 2.5e-7: FLT_EPSILON < 2.5e-7 <= 1e-6. The console re-frames; the 1e-6 placeholder
    //     skipped it.
    {
        StreamedDeformationSpec* lpSpec = Fresh();
        lpSpec->TransformToNewCOMSpace(Vector3{ 5.0e-4f, 0.0f, 0.0f, 0.0f });
        Check(lpSpec->mCurrentCOMOffset.x == 5.0e-4f, "(a) |d|^2 = 2.5e-7 > FLT_EPSILON: mCurrentCOMOffset = the new COM");
        Check(lpSpec->maDeformationSensorSpecs[0].mInitialOffset.x == 5.0e-4f, "(a) sensor 0 mInitialOffset += delta");
        Check(lpSpec->mMeshOffset.x == -5.0e-4f, "(a) mMeshOffset -= delta");
    }

    // (b) just above the boundary: d = (2^-12, 2^-12, 2^-20) -> |d|^2 = 2^-23 + 2^-40 (exact in f32),
    //     one ulp-scale step above FLT_EPSILON: applies.
    {
        StreamedDeformationSpec* lpSpec = Fresh();
        const f32 lfA = std::ldexp(1.0f, -12), lfB = std::ldexp(1.0f, -20);
        lpSpec->TransformToNewCOMSpace(Vector3{ lfA, lfA, lfB, 0.0f });
        Check(lpSpec->mCurrentCOMOffset.x == lfA && lpSpec->mCurrentCOMOffset.z == lfB,
              "(b) |d|^2 = 2^-23 + 2^-40 > FLT_EPSILON: the re-frame applies");
    }

    // (c) exactly at the boundary: d = (2^-12, 2^-12, 0) -> |d|^2 = 2^-23 == FLT_EPSILON exactly;
    //     vcmpgtfp is strict, so nothing moves (control: both bodies skip).
    {
        StreamedDeformationSpec* lpSpec = Fresh();
        const f32 lfA = std::ldexp(1.0f, -12);
        lpSpec->TransformToNewCOMSpace(Vector3{ lfA, lfA, 0.0f, 0.0f });
        Check(Untouched(lpSpec), "(c) |d|^2 == FLT_EPSILON exactly: strict '>' skips (control)");
    }

    // (d) |d|^2 = 9e-8 < FLT_EPSILON: skipped on both bodies (control).
    {
        StreamedDeformationSpec* lpSpec = Fresh();
        lpSpec->TransformToNewCOMSpace(Vector3{ 3.0e-4f, 0.0f, 0.0f, 0.0f });
        Check(Untouched(lpSpec), "(d) |d|^2 = 9e-8 < FLT_EPSILON: untouched (control)");
    }

    // (e) a real spawn-scale move (0.4 m): applies on both bodies (control).
    {
        StreamedDeformationSpec* lpSpec = Fresh();
        lpSpec->TransformToNewCOMSpace(Vector3{ 0.0f, 0.4f, 0.0f, 0.0f });
        Check(lpSpec->mCurrentCOMOffset.y == 0.4f && lpSpec->mMeshOffset.y == -0.4f
              && lpSpec->maDeformationSensorSpecs[0].mInitialOffset.y == 0.4f, "(e) 0.4 m move applies (control)");
    }

    // (f) NaN: NaN > eps is false -> skipped (control).
    {
        StreamedDeformationSpec* lpSpec = Fresh();
        lpSpec->TransformToNewCOMSpace(Vector3{ std::nanf(""), 0.0f, 0.0f, 0.0f });
        Check(Untouched(lpSpec), "(f) NaN delta skips (control)");
    }

    Check(giAsserts == 0, "no tripwire (control)");
    std::printf("FxXlaneComEpsilon: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
