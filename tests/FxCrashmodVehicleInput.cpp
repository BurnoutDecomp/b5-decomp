// FX-CRASHMOD (crash parity 2026-09-23), G63-D2: replay the production
// CrashIO::OutputBuffer_PreScene::Construct (ARTIST 0x827CE9E8) over a buffer poisoned with 0xCD.
// The console constructs, in order: the IOBuffer status byte (0x827CEA0C), the traffic output
// interface's two queues (0x827CEA10 / 0x827CEA18), the embedded
// BrnPhysics::Vehicle::VehicleInputInterface (0x827CEA20 `bl VehicleInputInterface::Construct`,
// this + 0x670) and the race-car crash-complete queue (0x827CEA30). The pre-fix body skipped the
// 0x827CEA20 leg (the member was a 1-byte placeholder), so every vehicle-input queue kept the
// poison. The second half appends the constructed crash-side interface into a physics-side one
// with the production VehicleInputInterface::Append (0x823C87C0) -- what
// WorldModule::BridgeCrashModuleToPhysicsModule @0x827AAC70 does every frame.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

static unsigned assertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::fprintf(stderr, "ASSERT: %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
} namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; } }

#include "fxcrashmod_vehicle_input_methods.inc"

using namespace BrnWorld;
using BrnPhysics::Vehicle::VehicleInputInterface;

namespace
{
    template <typename Q>
    bool IsConstructedEmpty(const Q& lrQueue)
    {
        return lrQueue.mpEvents == lrQueue.maEvents && lrQueue.miMaxLength == Q::KI_LENGTH && lrQueue.miLength == 0;
    }
}

int main()
{
    unsigned checks = 0, failures = 0;
    auto Check = [&](bool lbPass, const char* lpcName)
    {
        ++checks;
        if (!lbPass) { ++failures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
    };

    // Poison, then default-initialise in place (as CgsModule::IOBufferStack::CreateIOBuffer does
    // before it calls T::Construct), then run the production Construct.
    const size_t kuSize = sizeof(CrashIO::OutputBuffer_PreScene);
    void* lpRaw = ::operator new(kuSize, std::align_val_t(alignof(CrashIO::OutputBuffer_PreScene)));
    std::memset(lpRaw, 0xCD, kuSize);
    CrashIO::OutputBuffer_PreScene* lpBuffer = new (lpRaw) CrashIO::OutputBuffer_PreScene;
    lpBuffer->Construct();

    const VehicleInputInterface& v = lpBuffer->mVehicleInputInterface;
    Check(IsConstructedEmpty(v.mLineTestResultsQueue),                     "vehicle input: line-test result queue constructed");
    Check(IsConstructedEmpty(v.mCreateRaceCarEventQueue),                  "vehicle input: create race car queue constructed");
    Check(IsConstructedEmpty(v.mRemoveRaceCarEventQueue),                  "vehicle input: remove race car queue constructed");
    Check(IsConstructedEmpty(v.mResetRaceCarEventQueue),                   "vehicle input: reset race car queue constructed");
    Check(IsConstructedEmpty(v.mValidateRaceCarEventQueue),                "vehicle input: validate race car queue constructed");
    Check(IsConstructedEmpty(v.mSetRaceCarCollisionEventQueue),            "vehicle input: race car collision queue constructed");
    Check(IsConstructedEmpty(v.mSetRaceCarCullingGroupEventQueue),         "vehicle input: culling group queue constructed");
    Check(IsConstructedEmpty(v.mNetworkCarsAddedRemovedForCollisionQueue), "vehicle input: network add/remove queue constructed");
    Check(IsConstructedEmpty(v.mCreateTrafficEventQueue),                  "vehicle input: create traffic queue constructed");
    Check(IsConstructedEmpty(v.mCreateArticulatedTrafficEventQueue),       "vehicle input: articulated traffic queue constructed");
    Check(IsConstructedEmpty(v.mSetTrafficCrashingEventQueue),             "vehicle input: traffic crashing queue constructed");
    Check(IsConstructedEmpty(v.mRemoveCrashedTrafficEventQueue),           "vehicle input: remove crashed traffic queue constructed");
    Check(IsConstructedEmpty(v.mUpdateNetworkTrafficEventQueue),           "vehicle input: update network traffic queue constructed");
    Check(IsConstructedEmpty(v.mImpactEventQueue),                         "vehicle input: impact queue constructed");
    {
        VehicleInputInterface::RaceCarBitArray lEmpty;
        lEmpty.UnSetAll();
        Check(std::memcmp(&v.mRaceCarsAddedForCollision, &lEmpty, sizeof(lEmpty)) == 0,
              "vehicle input: added-for-collision bits cleared");
    }

    // The legs the pre-fix body already had stay constructed.
    Check(lpBuffer->mxStatusFlags.IsBitSet(CgsModule::IOBuffer::eStatusConstructed),         "status byte constructed");
    Check(IsConstructedEmpty(lpBuffer->mTrafficOutputInterface.mCleanupTrafficEventQueue), "traffic cleanup queue constructed");
    Check(IsConstructedEmpty(lpBuffer->mTrafficOutputInterface.mStartCrashingNetworkTrafficQueue),
          "network traffic crashing queue constructed");
    Check(IsConstructedEmpty(*lpBuffer->mRaceCarOutputInterface.GetRaceCarCrashCompleteEventQueue()),
          "race car crash complete queue constructed");

    // The bridge's merge: the constructed crash-side interface appends cleanly into the physics side,
    // and a network traffic update staged on the crash side arrives there.
    if (IsConstructedEmpty(v.mUpdateNetworkTrafficEventQueue))
    {
        auto lpPhysics = std::make_unique<VehicleInputInterface>();
        lpPhysics->Construct();
        lpPhysics->Append(v);
        Check(lpPhysics->mUpdateNetworkTrafficEventQueue.miLength == 0 && lpPhysics->mImpactEventQueue.miLength == 0,
              "an empty crash-side interface appends nothing");
        BrnPhysics::Vehicle::UpdateNetworkTrafficEvent lUpdate{};
        lpBuffer->mVehicleInputInterface.mUpdateNetworkTrafficEventQueue.AddEvent(lUpdate);
        lpPhysics->Append(v);
        Check(lpPhysics->mUpdateNetworkTrafficEventQueue.miLength == 1,
              "a crash-side network traffic update reaches the physics interface");
    }
    else
    {
        Check(false, "an empty crash-side interface appends nothing (queue unconstructed)");
        Check(false, "a crash-side network traffic update reaches the physics interface (queue unconstructed)");
    }

    ::operator delete(lpRaw, std::align_val_t(alignof(CrashIO::OutputBuffer_PreScene)));
    Check(assertions == 0, "no assertion fired");
    std::printf("FxCrashmodVehicleInput: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
