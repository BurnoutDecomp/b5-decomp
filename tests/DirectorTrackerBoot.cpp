#include <cmath>
#include <cstdio>
#include <cstring>
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"

static int giAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int) { ++giAsserts; std::printf("ASSERT %s\n", text); return 0; }
void* EndAssert() { return nullptr; }
} }
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(this, 0, sizeof(*this)); }

static BrnDirector::Camera::PlayerCrashInfo gCrash;
static BrnDirector::Camera::VehicleInfo gaCars[8];
static CgsContainers::BitArray<8u> gUsed;
static CgsSystem::TimerStatusInterface gTimers;
namespace BrnDirector { namespace DirectorIO {
const Camera::PlayerCrashInfo* InputBuffer::GetPlayerCrashInfo() const { return &gCrash; }
const Camera::VehicleInfo* InputBuffer::GetRaceCarInfo() const { return gaCars; }
const CgsContainers::BitArray<8u>* InputBuffer::GetUsedRaceCars() const { return &gUsed; }
const CgsSystem::TimerStatusInterface* InputBuffer::GetTimerStatusInterface() const { return &gTimers; }
} }
// Fixture clock: only its published quarter-second step matters to the real tracker.
void CgsSystem::TimerStatusInterface::StoreTimers(Timer*, Timer*)
{
    mSimTimerStatus.Clear();
    mSimTimerStatus.mfBaseTimeStep = 0.25f;
    mSimTimerStatus.mbRunning = true;
}

struct UnrelatedInit { void Construct() {} };
struct DirectorInitFixture
{
    UnrelatedInit mAllVehicleData;
    BrnDirector::VehicleTracker mVehicleTracker;
    UnrelatedInit mBehaviourManager;
    void Construct();
};
// Matches main -> DebugMemoryInit -> module ctor, and the PC's static gGameModule.
static DirectorInitFixture gDirector;
#include "director_tracker_boot.inc"

static int giChecks = 0, giFailed = 0;
static void Check(bool pass, const char* name)
{
    ++giChecks;
    if (!pass) { ++giFailed; std::printf("FAIL %s\n", name); }
}
static bool Is(const rw::math::vpu::Vector3& v, float x, float y, float z)
{
    return v.x == x && v.y == y && v.z == z;
}
int main()
{
    using namespace BrnDirector;
    gDirector.Construct();
    auto& tracker = gDirector.mVehicleTracker;
    Check(tracker.GetPositionJournal().GetSize() == 0, "boot leaves the tracker history empty");
    static GameState gameState;
    gameState.mEventState.SetAll(GameState::E_EVENT_STATE_PRE_INTRO);
    static DirectorIO::InputBuffer input;
    gUsed.UnSetAll();
    gUsed.SetBit(0);
    gTimers.StoreTimers(nullptr, nullptr);
    gaCars[0].mRaceCarState.mTransform.wAxis = {10.0f, 20.0f, 30.0f, 0.0f};
    tracker.SetVehicleIndex(0);
    tracker.Update(&gameState, &input, static_cast<EActiveRaceCarIndex>(0), false);
    Check(tracker.GetPositionJournal().GetSize() == 1, "first unpaused frame appends exactly one sample");
    Check(Is(tracker.GetImplicitVelocity(), 0, 0, 0), "one position has a finite zero implicit velocity");
    Check(giAsserts == 0, "first unpaused frame does not assert about a zero timestep");
    gaCars[0].mRaceCarState.mTransform.wAxis = {12.0f, 20.5f, 29.0f, 0.0f};
    tracker.Update(&gameState, &input, static_cast<EActiveRaceCarIndex>(0), false);
    Check(tracker.GetPositionJournal().GetSize() == 2, "second frame appends a second sample");
    Check(Is(tracker.GetImplicitVelocity(), 8, 2, -4), "second frame divides the displacement by the actual step");
    gDirector.Construct();
    Check(tracker.GetPositionJournal().GetSize() == 2, "main Construct does not reset existing tracker history");
    static VehicleTracker replayTracker;
    replayTracker.Construct();
    replayTracker.SetVehicleIndex(0);
    replayTracker.Update(&gameState, &input, static_cast<EActiveRaceCarIndex>(0), false);
    Check(replayTracker.GetPositionJournal().GetSize() == 8, "the replay reset retains its separate SetAll semantics");
    std::printf("DirectorTrackerBoot: %d checks, %d failures\n", giChecks, giFailed);
    return giFailed ? 1 : 0;
}
