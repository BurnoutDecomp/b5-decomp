// Replay the production PostPhysicsUpdate readback branch and deformation L3/L4/L6.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#undef private
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameSource/World/BrnEntityTypes.h"
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
    std::abort();
}
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; } }

namespace BrnWorld {
struct PhysicsReadbackInput
{
    BrnPhysics::Vehicle::VehicleOutputInterface vehicle = {};
    BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules deformation = {};
    const BrnPhysics::Vehicle::VehicleOutputInterface* GetVehicleOutputInterface() const
    { return &vehicle; }
};

struct PhysicsReadbackFixture
{
    ActiveRaceCar cars[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
    u32 readbacks = 0, standins = 0;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex)
    { return &cars[leIndex]; }
    void ReadUpdatedActiveRaceCarDataFromPhysics(PhysicsReadbackInput* lpInput);
    void PublishRenderPoseWithoutPhysicsBringUp(ActiveRaceCar* lpCar, s32)
    {
        ++standins;
        for (u32 luWheel = 0; luWheel < 4; ++luWheel)
            lpCar->GetRenderParams()->SetWheelExists(luWheel, true);
    }
    void TickReadback(PhysicsReadbackInput* lpInput, bool lbSimPaused);
    void Tick(PhysicsReadbackInput* lpInput, bool lbSimPaused)
    {
        cars[0].RestoreTickRenderPose();
        TickReadback(lpInput, lbSimPaused);
        cars[0].LatchTickRenderPose();
        cars[0].ApplyRenderPoseInterpolation(0.25f);
    }
};
#include "pause_readback_methods.inc"
}

static u32 suChecks = 0, suFailures = 0;
static std::set<std::string> sxReportedFailures;
static void Check(bool lbCondition, const char* lpcMessage)
{
    ++suChecks;
    if (!lbCondition)
    {
        ++suFailures;
        if (sxReportedFailures.insert(lpcMessage).second)
            std::fprintf(stderr, "FAIL: %s\n", lpcMessage);
    }
}

static Matrix44Affine Pose(f32 lfX)
{
    Matrix44Affine lPose = {};
    lPose.xAxis.x = lPose.yAxis.y = lPose.zAxis.z = 1.0f;
    lPose.wAxis.x = lfX;
    return lPose;
}

int main()
{
    using namespace BrnWorld;
    using namespace BrnPhysics::Deformation;
    static PhysicsReadbackFixture lFixture;
    static PhysicsReadbackInput lInput;
    static Vector3Plus laSkin[KU_MAX_RACE_CAR_VERLET_POINTS] = {};
    static RaceCar lGlobal;
    auto& lrCar = lFixture.cars[0];
    lrCar.mpRaceCar = &lGlobal;
    lrCar.BecomeActiveForReset();
    lrCar.ResetRenderPoseInterpolation();
    auto* lpParams = lrCar.GetRenderParams();
    lpParams->SetBodyTransform(Pose(5.0f));
    lpParams->GetDetachedPartQueue().Construct();
    for (u32 luWheel = 0; luWheel < 6; ++luWheel)
        lpParams->GetWheelTransform(luWheel) = Pose(0.0f);
    lInput.vehicle.mUsedRaceCars.SetBit(0);
    auto& lrOutput = lInput.deformation;
    lrOutput.muNumEntries = 1;
    lrOutput.maBaseIDs[0] = static_cast<u64>(E_ENTITYTYPE_RACECAR) << 56;
    WheelPhysicalStates lWheels = {};
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        lWheels.maStates[luWheel].mWorldSpaceTransform = Pose(10.0f + luWheel);
        lWheels.mabWheelExists[luWheel] = luWheel != 1; // wheel 1 has expired after detachment
    }
    std::memcpy(lrOutput.GetWheelStateSlot(0), &lWheels, sizeof(lWheels));
    laSkin[127] = {-0.3f, -0.7f, 0.5f, 0.2f};
    lrOutput.miNumSkinnedModels = 1;
    lrOutput.maSkinData[0] = {EntityId{static_cast<u32>(E_ENTITYTYPE_RACECAR) << 24}, laSkin};
    lrOutput.GetDetachedPartRenderQueue().Construct();
    BrnPhysics::Deformation::DetachedPartRenderEvent lPanel = {};
    lPanel.mVehicleEntityId.muValue = static_cast<u32>(E_ENTITYTYPE_RACECAR) << 24;
    lPanel.miPartIndex = 7;
    lPanel.mbIsAttached = false;
    lPanel.mTransform = Pose(20.0f);
    lrOutput.GetDetachedPartRenderQueue().AddEventSafe(lPanel);

    lFixture.Tick(&lInput, false);
    Check(lFixture.readbacks == 1, "running frame publishes once");
    Check(!lpParams->GetWheelExists(1), "L3 expired wheel stays absent after readback");
    Check(lpParams->GetDetachedPartQueue().GetLength() == 1, "detached panel published");
    Check(!lpParams->GetDetachedPartQueue().GetEvent(0).mbIsAttached, "detached panel stays detached");

    // The physics bridge supplies empty queues while paused. The original skips
    // readback entirely, preserving all render-owned damage over repeated frames.
    lrOutput.muNumEntries = 0;
    lrOutput.miNumSkinnedModels = 0;
    lrOutput.GetDetachedPartRenderQueue().Clear();
    lInput.vehicle.mUsedRaceCars.UnSetAll();
    for (u32 luFrame = 0; luFrame < 120; ++luFrame)
    {
        lFixture.Tick(&lInput, true);
        Check(lFixture.readbacks == 1 && lFixture.standins == 0, "paused frame has no physics writer");
        Check(lpParams->GetDetachedPartQueue().GetLength() == 1, "paused frame retains detached panel");
        Check(!lpParams->GetWheelExists(1), "paused frame retains expired wheel");
        Check(std::fabs(lpParams->GetVerletOffsets()[127].y + 0.7f) < 0.0001f,
              "paused frame retains final skin endpoint");
    }
    lFixture.Tick(nullptr, true);
    Check(lFixture.standins == 0, "paused missing-input frame has no rest-pose stand-in");

    // The first resumed tick replaces the queue once, without retaining stale
    // detached instances or forcing the expired wheel visible.
    lInput.vehicle.mUsedRaceCars.SetBit(0);
    lrOutput.muNumEntries = 1;
    lrOutput.miNumSkinnedModels = 1;
    lPanel.mTransform = Pose(24.0f);
    lrOutput.GetDetachedPartRenderQueue().AddEventSafe(lPanel);
    lFixture.Tick(&lInput, false);
    Check(lFixture.readbacks == 2, "first resumed tick publishes once");
    Check(lpParams->GetDetachedPartQueue().GetLength() == 1, "resumed queue replaces old panel");
    Check(std::fabs(lpParams->GetDetachedPartQueue().GetEvent(0).mTransform.wAxis.x - 21.0f) < 0.0001f,
          "resumed panel blends from last frozen world pose");
    Check(!lpParams->GetWheelExists(1), "resumed L3 expired wheel stays absent");
    std::printf("Pause deformation readback: %s (%u checks, %u failures, 120 paused frames, %u readbacks)\n",
                suFailures ? "FAIL" : "PASS", suChecks, suFailures, lFixture.readbacks);
    return suFailures ? 1 : 0;
}
