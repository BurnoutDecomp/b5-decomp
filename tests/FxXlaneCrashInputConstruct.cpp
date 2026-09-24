// Harness for run_fxxlane_crash_input_construct.py (crash parity FOLLOWUPS 19; FX-XLANE).
//
// BrnWorld::CrashIO::InputBuffer_PreScene::Construct -- the body the console inlines into
// CgsIOBufferStack::CreateIOBuffer<InputBuffer_PreScene> @0x827CEB30 -- extracted from the real
// source and run exactly as CreateIOBuffer<T> does (`new (lpMem) T; (*lpOut)->Construct();`) on
// 0xCD-poisoned storage of the REAL type, with the real member bodies linked in.
//   0x827CEBF4 stb 1,0(r31)                                 -> IOBuffer::Construct (constructed bit)
//   0x827CEBF8 CgsSystem::TimerStatusInterface::Clear       (+0x4)
//   0x827CEC00 BrnWorld::CrashIO::NetworkInputInterface::Construct (+0x40)
//   0x827CEC08 BrnPhysics::Vehicle::VehicleDriverInputInterface::Construct (+0x3CD0)
//   0x827CEC10 RCEntityActiveRaceCarOutputInterface::Clear  (+0x5180)
//   0x827CEC18 CgsModule::VariableEventQueue<13312,16>::Construct (+0x7A70)
//   no store to +0xAE80 (mbPlayerPressingBoost).
// The pre-fix tree had no InputBuffer_PreScene::Construct, so CreateIOBuffer<T>'s call bound to
// the base CgsModule::IOBuffer::Construct; the runner models that binding for --pre-fix.
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// LINK-ONLY: referenced by RCEntityActiveRaceCarOutputInterface / CgsResourcePtr helpers this test
// never calls (the buffer's default construction does not reference them). They abort if reached.
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() { std::abort(); }
BaseResourcePtr::~BaseResourcePtr() { std::abort(); }
ResourceHandle BaseResourcePtr::GetResourceHandle() const { std::abort(); }
void BaseResourcePtr::AddToNewList(BaseResourcePtr*) { std::abort(); }
}

static int giAsserts = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnWorld { namespace CrashIO {
#include "methods.inc"
} }

using namespace BrnWorld::CrashIO;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

alignas(16) static unsigned char gStorage[sizeof(InputBuffer_PreScene)];

int main()
{
    std::memset(gStorage, 0xCD, sizeof(gStorage));
    InputBuffer_PreScene* lpBuffer = new (gStorage) InputBuffer_PreScene;   // CreateIOBuffer: default-init
    lpBuffer->Construct();

    Check(lpBuffer->mxStatusFlags.IsBitSet(CgsModule::IOBuffer::eStatusConstructed)
          && !lpBuffer->IsBufferLocked(), "status: constructed and unlocked (stb 1 @0x827CEBF4; control)");

    const CgsSystem::TimerStatusInterface& lrTimers = lpBuffer->mTimerStatusInterface;
    Check(lrTimers.mGameTimerStatus.miFrameCount == 0 && lrTimers.mGameTimerStatus.mfBaseTimeStep == 0.0f
          && lrTimers.mGameTimerStatus.mfTimeStepMultiplier == 1.0f && lrTimers.mSimTimerStatus.miFrameCount == 0
          && lrTimers.mSimTimerStatus.mfBaseTimeStep == 0.0f && lrTimers.mSimTimerStatus.mfTimeStepMultiplier == 1.0f,
          "mTimerStatusInterface cleared (TimerStatusInterface::Clear @0x827CEBF8)");

    const NetworkInputInterface& lrNetwork = lpBuffer->mNetworkInputInterface;
    bool lbNetworkQueues = true, lbNoCarMarked = true;
    for (s32 li = 0; li < BrnWorld::KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        const NetworkInputInterface::CrashingTrafficUpdateQueue& lrQueue = lrNetwork.maCrashingTrafficUpdateQueues[li];
        lbNetworkQueues = lbNetworkQueues && lrQueue.miLength == 0
            && static_cast<const void*>(lrQueue.mpEvents) >= static_cast<const void*>(&lrQueue)
            && static_cast<const void*>(lrQueue.mpEvents) < static_cast<const void*>(&lrQueue + 1);
        lbNoCarMarked = lbNoCarMarked && !lrNetwork.IsRaceCarMarkedForUpdate(li);
    }
    Check(lbNetworkQueues, "network: every crashing-traffic queue constructed on its own storage, empty (@0x827CEC00)");
    Check(lbNoCarMarked, "network: no race car marked for update (bitset reset, @0x827CEC00)");

    const BrnPhysics::Vehicle::VehicleDriverInputInterface& lrDriver = lpBuffer->mVehicleDriverInterface;
    bool lbDriverSnapshot = lrDriver.miTargetAssistCount == 0;
    for (s32 li = 0; li < 8; ++li)
    {
        lbDriverSnapshot = lbDriverSnapshot && lrDriver.maBaseDeformationAmounts[li] == 0.0f
            && lrDriver.maBaseDeformationFrames[li] == -1;
    }
    unsigned char lu8DriverQueueConstructed;
    std::memcpy(&lu8DriverQueueConstructed, &lrDriver.mDriverUpdateQueue.mbIsConstructed, 1);
    Check(lu8DriverQueueConstructed == 1 && lrDriver.mDriverUpdateQueue.miLength == 0 && lbDriverSnapshot,
          "vehicle-driver interface constructed: empty queue, count 0, amounts 0.0, frames -1 (@0x827CEC08)");

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrActive = lpBuffer->mActiveRaceCarInterface;
    Check(lrActive.mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID
          && lrActive.maeActiveRaceCarIndex[0] == E_ACTIVE_RACE_CAR_INDEX_COUNT
          && lrActive.mbPlayerWrecked == false, "active race-car interface cleared (@0x827CEC10)");

    unsigned char lu8QueueConstructed;
    std::memcpy(&lu8QueueConstructed, &lpBuffer->mGameActionQueue.mbIsConstructed, 1);
    Check(lu8QueueConstructed == 1 && lpBuffer->mGameActionQueue.miLength == 0,
          "game-action queue constructed empty (VariableEventQueue<13312,16>::Construct @0x827CEC18)");

    unsigned char lu8Boost;
    std::memcpy(&lu8Boost, &lpBuffer->mbPlayerPressingBoost, 1);
    Check(lu8Boost == 0xCD, "mbPlayerPressingBoost (+0xAE80) not written (no store in 0x827CEBEC..C18; control)");
    Check(giAsserts == 0, "no tripwire (control)");

    std::printf("FxXlaneCrashInputConstruct: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
