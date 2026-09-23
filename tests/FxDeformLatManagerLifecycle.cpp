// Harness for run_fxdeformlat_manager_lifecycle.py (crash parity G23-D2, G23-D3, G23-D4, FX-DEFORM-LAT).
//
// The shipped DeformationManager::Construct (BrnDeformationManager_Construct.cpp) and ::Prepare
// (BrnDeformationManager.cpp), DeformableObject::ClearVariables & co. (+ ::Construct when present)
// from BrnDeformableObject_Lifecycle.cpp, the DetachedWheelManager Construct/Prepare bodies (when
// present) and ImpulsePasser::Construct/ClearVariables are pasted in through methods.inc and run
// on storage of the REAL DeformationManager / DeformableObject types that is POISONED with 0xCD
// first -- the host's picture of "memory nobody wrote" (PC IO/resource memory is not zero-filled).
// Fixtures stand in for the debug component, the perf-monitor registry, the part pool construct,
// the rw pool carve and the DeformableObject placement ctor (which, like the console's, writes none
// of the fields under test -- here it simply leaves the 0xCD poison in place).
//
// ARTIST:
//   Construct @0x82621510: 0x826215A8 std 0 -> this+0x12870 (mDetachedWheelManager.mUsedWheels);
//     0x8262177C stwx 0 -> this+0x12900 (mpaModels); 0x8262178C..98 28 x stw -1 from this+0x12888
//     (maGlobalEntityIDs).
//   Prepare @0x82630230: per model 0x826303B4..CC std 0 +0x6710, stb 0 +0x6722/+0x6728/+0x6729/
//     +0x6770/+0x6730, bl ClearVariables (== DeformableObject::Construct, PS3 0x6BEFC4); tail
//     0x82630404 std 0 -> this+0x12870 (wheel used-set), 0x8262040C std 0 -> this+0xBBE0
//     (mStateOutput.mxLiveSlots), plus mModelsAdded / miLastBodyToHaveIKUpdate.
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/rwcore_structs.h"
#include <cstdio>
#include <cstring>
#include <new>

static int giAsserts = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
}
namespace PerfMonCpu {
s32 AddMonitor(const char*, s32, s32, double, s32, s32) { return 7; }
}
}

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

static const u32 KU_POOL_MODELS = 28u;
alignas(16) static unsigned char gManagerStorage[sizeof(DeformationManager)];
alignas(16) static unsigned char gPoolStorage[KU_POOL_MODELS * sizeof(DeformableObject)];

namespace BrnPhysics { namespace Deformation {
// Fixtures for the bodies outside the functions under test.
void DeformationDebugComponent_Construct(DeformationDebugComponent*, DeformationManager*) {}
void DeformationDebugComponent_Register(DeformationDebugComponent*) {}
void DeformableObject_ConstructUpdatePerformanceMonitors() {}
void DeformableObject_ConstructUpdateIKAndLocatorsPerformanceMonitors() {}
void DeformableObject_ConstructPostPhysicsPerformanceMonitors() {}
void DetachedPartManager::Construct() {}
DeformableObject* AllocateDeformableModelPool(rw::IResourceAllocator*, u32) { return reinterpret_cast<DeformableObject*>(gPoolStorage); }
// The placement ctor stand-in: the console ctor writes none of the six Construct fields.
static void HarnessPlaceModel(DeformableObject*) {}
// The pre-fix Prepare's trampoline (declared at namespace scope in the old BrnDeformationManager.cpp).
void DeformableObject_ClearVariables(DeformableObject* lpModel);
} }

#include "methods.inc"

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liIndex = -1)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (%d)\n", lpcName, liIndex); }
}

int main()
{
    DeformationManager& lrManager = *reinterpret_cast<DeformationManager*>(gManagerStorage);

    // ---- Construct on poisoned storage ---------------------------------------------------------
    std::memset(gManagerStorage, 0xCD, sizeof(gManagerStorage));
    lrManager.Construct();
    bool lbWheelsClear = true;
    for (u32 lu = 0; lu < 20u; ++lu) lbWheelsClear = lbWheelsClear && !lrManager.mDetachedWheelManager.mUsedWheels.IsBitSet(lu);
    Check(lbWheelsClear, "Construct: detached-wheel used-set cleared (0x826215A8)");
    Check(lrManager.mpaModels == nullptr, "Construct: mpaModels = NULL (0x8262177C)");
    bool lbIdsUnmapped = true;
    for (u32 lu = 0; lu < 28u; ++lu) lbIdsUnmapped = lbIdsUnmapped && lrManager.maGlobalEntityIDs[lu].muValue == 0xFFFFFFFFu;
    Check(lbIdsUnmapped, "Construct: maGlobalEntityIDs[0..27] = -1 (0x8262178C..98)");
    Check(lrManager.miPlayerModelIndex == -1 && lrManager.miNumUsedModels == 0, "Construct: player -1, no models (control)");
    Check(lrManager.ma8RaceCarToModelIndex[0] == -1 && lrManager.ma8TrafficToModelIndex[19] == -1, "Construct: index tables -1 (control)");
    Check(!lrManager.mModelsAdded.IsBitSet(0) && !lrManager.mModelsAdded.IsBitSet(27), "Construct: mModelsAdded clear (control)");

    // ---- Prepare on poisoned storage (models, manager, used wheels, live slots) ----------------
    std::memset(gManagerStorage, 0xCD, sizeof(gManagerStorage));
    std::memset(gPoolStorage, 0xCD, sizeof(gPoolStorage));
    const bool lbPrepared = lrManager.Prepare(reinterpret_cast<rw::IResourceAllocator*>(gPoolStorage));
    Check(lbPrepared, "Prepare returns true");
    Check(lrManager.mpaModels == reinterpret_cast<DeformableObject*>(gPoolStorage), "Prepare binds the pool (control)");
    int liBadModels = 0, liUnclearedModels = 0;
    for (u32 lu = 0; lu < KU_POOL_MODELS; ++lu)
    {
        const DeformableObject& lrModel = reinterpret_cast<DeformableObject*>(gPoolStorage)[lu];
        const bool lbConstructed = static_cast<u64>(lrModel.mHandlingBodyID) == 0ull && !lrModel.mbActive
            && !lrModel.mbHasDeformedThisFrame && !lrModel.mbIKUpdateRequired && lrModel.miNumBrokenWheels == 0
            && !lrModel.mbResetDeformationNextUpdate;
        if (!lbConstructed) ++liBadModels;
        if (!(lrModel.miNumTagPoints == 0 && lrModel.mfNoDamageTimer == 100.0f)) ++liUnclearedModels;
    }
    Check(liBadModels == 0, "Prepare: every model's six Construct fields zeroed (0x826303B4..0x826303C8)", liBadModels);
    Check(liUnclearedModels == 0, "Prepare: ClearVariables still runs on every model (control)", liUnclearedModels);
    lbWheelsClear = true;
    for (u32 lu = 0; lu < 20u; ++lu) lbWheelsClear = lbWheelsClear && !lrManager.mDetachedWheelManager.mUsedWheels.IsBitSet(lu);
    Check(lbWheelsClear, "Prepare: detached-wheel used-set cleared (0x82630404)");
    bool lbLiveClear = true;
    for (u32 lu = 0; lu < 28u; ++lu) lbLiveClear = lbLiveClear && !lrManager.mStateOutput.mxLiveSlots.IsBitSet(lu);
    Check(lbLiveClear, "Prepare: mStateOutput.mxLiveSlots cleared (0x8263040C)");
    Check(!lrManager.mModelsAdded.IsBitSet(0) && lrManager.miLastBodyToHaveIKUpdate == 0, "Prepare: models/IK cursor reset (control)");

    std::printf("FxDeformLatManagerLifecycle: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
