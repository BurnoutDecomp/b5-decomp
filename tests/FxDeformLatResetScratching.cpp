// Harness for run_fxdeformlat_reset_scratching.py (crash parity G24-D1, FX-DEFORM-LAT).
//
// The shipped DeformationManager::ProcessEvents (BrnDeformationManager.cpp) and
// DeformableObject::ResetScratching (BrnDeformableObject_Update.cpp, when present) are pasted in
// through methods.inc and run on zero-filled storage of the REAL DeformationManager /
// DeformationInputInterface / DeformableObject / StreamedDeformationSpec types. The four event
// drains, GetPlayerCarModel, UpdateIK and UpdateSkinningOffsets are fixtures; UpdateIK records the
// sensor scratch it would re-blend (it READS sensor+0x1A4 at 0x826088DC/0x826088E8).
//
// ARTIST ProcessEvents @0x82644E38, 0x82644ED0..0x82644F04 (the inlined ResetScratching, PS3
// 0x6B9D7C): n = spec+0x652 ; for i < n: stfs 0.0 (flt_82001CC0), 0x1AF4(model + 0x1B0*i) ==
// maDeformationSensors[i].mfScratchAmount ; then vspltisw v1,0 ; bl UpdateIK ; bl UpdateSkinningOffsets.
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"
#include <cstdio>
#include <cstring>

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;

alignas(16) static unsigned char gManagerStorage[sizeof(DeformationManager)];
alignas(16) static unsigned char gInputStorage[sizeof(DeformationInputInterface)];
alignas(16) static unsigned char gModelStorage[sizeof(DeformableObject)];
alignas(16) static unsigned char gSpecStorage[sizeof(StreamedDeformationSpec)];

static const u8 KU_SPEC_SENSORS = 6;
static int giUpdateIKCalls = 0, giSkinCalls = 0, giScratchSeenByIK = -1;
static float gfIKTime = -1.0f;

namespace BrnPhysics { namespace Deformation {
void DeformationManager::ProcessDeactivateDeformationModelEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer*, const DeformationInputInterface*, CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*) {}
void DeformationManager::ProcessRemoveDeformationModelEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer*, const DeformationInputInterface*, CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*) {}
void DeformationManager::ProcessAddDeformationModelEvents(CgsPhysics::PhysicsSimulationIO::InputBuffer*, const DeformationInputInterface*, CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*) {}
void DeformationManager::ProcessValidateDeformationModelEvents(const DeformationInputInterface*) {}
DeformableObject* DeformationManager::GetPlayerCarModel() { return reinterpret_cast<DeformableObject*>(gModelStorage); }
void DeformableObject::UpdateIK(VecFloat lvfTime)
{
    ++giUpdateIKCalls;
    gfIKTime = lvfTime.x;
    giScratchSeenByIK = 0;   // count of spec sensors whose scratch UpdateIK would re-blend as non-zero
    for (u8 i = 0; i < KU_SPEC_SENSORS; ++i)
        if (maDeformationSensors[i].GetScratchAmount() != 0.0f) ++giScratchSeenByIK;
}
void DeformableObject::UpdateSkinningOffsets() { ++giSkinCalls; }
} }

#include "methods.inc"

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liValue = -1)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (%d)\n", lpcName, liValue); }
}

int main()
{
    DeformationManager&        lrManager = *reinterpret_cast<DeformationManager*>(gManagerStorage);
    DeformationInputInterface& lrInput   = *reinterpret_cast<DeformationInputInterface*>(gInputStorage);
    DeformableObject&          lrModel   = *reinterpret_cast<DeformableObject*>(gModelStorage);
    StreamedDeformationSpec&   lrSpec    = *reinterpret_cast<StreamedDeformationSpec*>(gSpecStorage);

    lrSpec.mu8NumDeformationSensors = KU_SPEC_SENSORS;   // the spec's count: 6 of the 20 sensors
    lrModel.mpDeformationSpec = &lrSpec;
    for (int i = 0; i < 20; ++i) lrModel.maDeformationSensors[i].SetScratchAmount(0.5f + i);
    lrInput.mbResetPlayerScratches = true;               // game action 98 (paint-shop respray)

    lrManager.ProcessEvents(nullptr, &lrInput, nullptr);

    int liLeft = 0;
    for (int i = 0; i < KU_SPEC_SENSORS; ++i) if (lrModel.maDeformationSensors[i].GetScratchAmount() != 0.0f) ++liLeft;
    Check(giScratchSeenByIK == 0, "UpdateIK re-blends zeroed sensor scratch (reset BEFORE UpdateIK)", giScratchSeenByIK);
    Check(liLeft == 0, "sensors 0..n-1 scratch = 0 (stfs 0.0, 0x1AF4 @0x82644EFC)", liLeft);
    bool lbBeyond = true;
    for (int i = KU_SPEC_SENSORS; i < 20; ++i) lbBeyond = lbBeyond && lrModel.maDeformationSensors[i].GetScratchAmount() == 0.5f + i;
    Check(lbBeyond, "bound is the SPEC's sensor count (spec+0x652), not GetNumSensors() (control)");
    Check(giUpdateIKCalls == 1 && gfIKTime == 0.0f && giSkinCalls == 1, "UpdateIK(0) then UpdateSkinningOffsets, once (control)");
    Check(!lrInput.ShouldResetPlayerScratches(), "the reset flag is consumed with the queues (control)");
    Check(giAsserts == 0, "valid fixture fires no tripwire (control)");
    std::printf("FxDeformLatResetScratching: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
