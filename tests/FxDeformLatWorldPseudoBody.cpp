// Harness for run_fxdeformlat_world_pseudo_body.py (crash parity G23-D1, FX-DEFORM-LAT).
//
// The shipped statements of DeformationManager::SolvePenetration (BrnDeformationManager_Contacts.cpp)
// between the phase-1 StartMonitor and the per-model add loop are pasted into SolveHead::Run through
// head.inc, with the real PenetrationSolver::AddObject, and run on a PenetrationSolver whose storage
// is poisoned with 0xCD (CreateIOBuffer<PenetrationSolver> writes only the two counts).
//
// ARTIST SolvePenetration @0x82621B08, 0x82621B44..0x82621C1C: rows (1,0,0,0) (0,1,0,0) (0,0,1,0)
// (0,0,0,0) from flt_82001C98 (1.0) / flt_82001CC0 (0.0) stored to solver+0x710..+0x740 ==
// maObjectTransforms[28]; vspltisw v10,0 stored to solver+0x910 == mavfBodyWeighting[28].
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPenetrationSolver.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include <cstdio>
#include <cstring>

static int giAsserts = 0, giMonitorStarts = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
}
namespace PerfMonCpu { void StartMonitor(s32) { ++giMonitorStarts; } }
}

#include "methods.inc"

namespace BrnPhysics { namespace Deformation {
struct SolveHead
{
    s32 miPostPhysicsUpdateAddContactsToPenSolverPerfMon = 3;
    void Run(PenetrationSolver* lpSolver)
    {
#include "head.inc"
    }
};
} }

using namespace BrnPhysics::Deformation;
alignas(16) static unsigned char gSolverStorage[sizeof(PenetrationSolver)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}
static bool Row(const Vector3& r, float x, float y, float z, float w)
{
    const float lafWant[4] = { x, y, z, w };
    return std::memcmp(&r, lafWant, 16) == 0;   // bitwise: +0.0f, never -0.0f or poison
}

int main()
{
    std::memset(gSolverStorage, 0xCD, sizeof(gSolverStorage));
    PenetrationSolver& lrSolver = *reinterpret_cast<PenetrationSolver*>(gSolverStorage);
    SolveHead lHead;
    lHead.Run(&lrSolver);

    const Matrix44Affine& lrWorld = lrSolver.maObjectTransforms[KI_MAX_DEFORMATION_MODELS];
    Check(Row(lrWorld.xAxis, 1.0f, 0.0f, 0.0f, 0.0f) && Row(lrWorld.yAxis, 0.0f, 1.0f, 0.0f, 0.0f)
          && Row(lrWorld.zAxis, 0.0f, 0.0f, 1.0f, 0.0f), "maObjectTransforms[28] rows = identity, w lanes 0 (+0x710..+0x730)");
    Check(Row(lrWorld.wAxis, 0.0f, 0.0f, 0.0f, 0.0f), "maObjectTransforms[28] translation row all zero (+0x740)");
    const VecFloat& lrWeight = lrSolver.mavfBodyWeighting[KI_MAX_DEFORMATION_MODELS];
    Check(std::memcmp(&lrWeight, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0, "mavfBodyWeighting[28] = 0 (+0x910)");
    unsigned char lau8Poison[sizeof(Matrix44Affine)];
    std::memset(lau8Poison, 0xCD, sizeof(lau8Poison));
    Check(std::memcmp(&lrSolver.maObjectTransforms[KI_MAX_DEFORMATION_MODELS - 1], lau8Poison, sizeof(lau8Poison)) == 0,
          "model slot 27 untouched (control)");
    Check(giMonitorStarts == 1 && giAsserts == 0, "StartMonitor once, no tripwire (control)");
    std::printf("FxDeformLatWorldPseudoBody: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
