// Harness for run_fxdeformlat_position_queue_safe.py (crash parity G30-D1, FX-DEFORM-LAT).
//
// The shipped PhysicalBodyPartPool::OutputEvents (BrnPhysicalBodyPartPool.cpp) and
// PhysicalBodyPart::GetGlobalEntityId are pasted in through methods.inc and run on zero-filled
// storage of the REAL pool / output-interface / IK types (the header's inline
// DeformationOutputInterface::AddDetachedPartPosition comes with it); CgsStrStream is linked for the
// opt-in [detach-pose] probe, which stays off.
//
// ARTIST OutputEvents @0x8260DBE8: the render add is 0x8260DD68 bl 0x825E5C78 == AddEvent (:312/:313
// tripwires, unconditional write); the position add is 0x8260DDAC bl 0x825E5B00 == AddEventSafe
// (:331 null tripwire only; 0x825E5B48..54 `cmpw len,max ; bge -> li r3,0` drops the event when the
// 50-slot queue is full). PS3 0x6FE148 is the same pair.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int giAsserts = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}
namespace renderengine { u32 guPresentCount = 0; }

#include "methods.inc"

using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;
alignas(16) static unsigned char gPool[sizeof(PhysicalBodyPartPool)];
alignas(16) static unsigned char gOut[sizeof(DeformationOutputInterface)];
alignas(16) static unsigned char gOutEM[sizeof(DeformationOutputInterfaceForEntityModules)];
alignas(16) static unsigned char gIK[sizeof(IKBodyPart)];
alignas(16) static unsigned char gSpec[sizeof(IKBodyPartSpec)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

static void Setup()
{
    std::memset(gPool, 0, sizeof(gPool)); std::memset(gOut, 0, sizeof(gOut)); std::memset(gOutEM, 0, sizeof(gOutEM));
    std::memset(gIK, 0, sizeof(gIK)); std::memset(gSpec, 0, sizeof(gSpec));
    PhysicalBodyPartPool& lrPool = *reinterpret_cast<PhysicalBodyPartPool*>(gPool);
    IKBodyPart& lrIK = *reinterpret_cast<IKBodyPart*>(gIK);
    IKBodyPartSpec& lrSpec = *reinterpret_cast<IKBodyPartSpec*>(gSpec);
    lrSpec.miPartGraphics = 11;
    lrSpec.mePartType = static_cast<EBodyParts>(5);
    lrIK.mpSpec = &lrSpec;
    lrPool.mUsedParts.SetBit(7);
    PhysicalBodyPart& lrPart = lrPool.maParts[7];
    lrPart.mpIKPart = &lrIK;
    lrPart.mGlobalVehicleId.muValue = 0x01000400u;
    lrPart.mbJoinedToVehicle = false;
    Matrix44Affine lT; lT.SetIdentity(); lT.wAxis = Vector3{ 1.0f, 2.0f, 3.0f, 0.0f };
    lrPart.mRwBody.mTransform = lT;
    DeformationOutputInterface& lrOut = *reinterpret_cast<DeformationOutputInterface*>(gOut);
    lrOut.mDetachedPartCurrentPositionQueue.Construct();
    lrOut.mGlassSmashOrCrackQueue.Construct();
    reinterpret_cast<DeformationOutputInterfaceForEntityModules*>(gOutEM)->mDetachedPartRenderQueue.Construct();
}

int main()
{
    PhysicalBodyPartPool& lrPool = *reinterpret_cast<PhysicalBodyPartPool*>(gPool);
    DeformationOutputInterface& lrOut = *reinterpret_cast<DeformationOutputInterface*>(gOut);
    DeformationOutputInterfaceForEntityModules& lrOutEM = *reinterpret_cast<DeformationOutputInterfaceForEntityModules*>(gOutEM);

    // Case A -- room in the queue: one used slot publishes one render + one position event.
    Setup();
    lrPool.OutputEvents(&lrOutEM, &lrOut);
    Check(lrOut.mDetachedPartCurrentPositionQueue.GetLength() == 1, "A: one position event (control)");
    if (lrOut.mDetachedPartCurrentPositionQueue.GetLength() == 1)
    {
        const DetachedPartCurrentPositionEvent& lrE = lrOut.mDetachedPartCurrentPositionQueue.GetEvent(0);
        Check(lrE.mTransform.wAxis.x == 1.0f && lrE.mTransform.wAxis.z == 3.0f && lrE.mVehicleEntityId.muValue == 0x01000400u
              && static_cast<s32>(lrE.meType) == 5, "A: event carries transform, entity id, part type (control)");
    }
    Check(lrOutEM.mDetachedPartRenderQueue.GetLength() == 1, "A: one render event (control)");
    Check(giAsserts == 0, "A: no tripwire (control)");

    // Case B -- the position queue is already full (50): the console drops silently.
    Setup();
    DetachedPartCurrentPositionEvent lSentinel{};
    lSentinel.mVehicleEntityId.muValue = 0xABCDEF01u;
    for (int i = 0; i < 50; ++i) lrOut.mDetachedPartCurrentPositionQueue.AddEventSafe(lSentinel);
    unsigned char lau8GlassBefore[sizeof(lrOut.mGlassSmashOrCrackQueue)];
    std::memcpy(lau8GlassBefore, &lrOut.mGlassSmashOrCrackQueue, sizeof(lau8GlassBefore));
    giAsserts = 0;
    lrPool.OutputEvents(&lrOutEM, &lrOut);
    Check(giAsserts == 0, "B: a full queue fires no Reached-Max-length tripwire (AddEventSafe has only :331)");
    Check(lrOut.mDetachedPartCurrentPositionQueue.miLength == 50, "B: the length stays 50 (bge -> li r3,0: dropped)");
    Check(lrOut.mDetachedPartCurrentPositionQueue.maEvents[49].mVehicleEntityId.muValue == 0xABCDEF01u, "B: slot 49 keeps the sentinel");
    Check(std::memcmp(lau8GlassBefore, &lrOut.mGlassSmashOrCrackQueue, sizeof(lau8GlassBefore)) == 0,
          "B: nothing is written past maEvents[50] (the next member is untouched)");
    Check(lrOutEM.mDetachedPartRenderQueue.GetLength() == 1, "B: the render event still goes out (control)");

    std::printf("FxDeformLatPositionQueueSafe: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
