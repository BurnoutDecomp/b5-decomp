// Production module split/drain, wrappers, kernel and native EAJobs. Only world
// creation/cache input and the physics-event sink are isolated boundaries.
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <vector>
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/job_thread_parameters.h"
#include "SDKs/EATech/eajobs/jobs.h"
#include "rw/math/vpu/vector4_operation.h"

static unsigned checks, failures;
static void Check(bool ok, const char* name)
{ ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", name); } }
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int) { std::printf("ASSERT %s\n", text); std::abort(); }
void* EndAssert() { return nullptr; }
void ServiceWorkerAssertsWhileWaitingPC() {}
}
namespace Log {
DebugPrint* gpDebugPrint = nullptr;
void WriteToLog(const char*) {}
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}
namespace renderengine { u32 guPresentCount = 0; }
struct Allocator : EA::Allocator::ICoreAllocator {
    void* Alloc(size_t n, const char*, unsigned, unsigned alignment, unsigned offset) override
    { if (offset) std::abort(); return _aligned_malloc(n, (std::max)(alignment, 16u)); }
    void* Alloc(size_t n, const char* name, unsigned flags) override { return Alloc(n, name, flags, 16, 0); }
    void Free(void* p, size_t) override { _aligned_free(p); }
};
static EA::Jobs::JobScheduler scheduler;
namespace CgsSystem { EA::Jobs::JobScheduler* JobManager() { return &scheduler; } }

using Request = std::tuple<u16, s8, u32>;
namespace BrnTraffic {
struct TestPostInput { const void* GetActiveRaceCarOutputInterface() const { return nullptr; } };
struct TestPostOutput {};
struct TestPreOutput {};
inline void LogMissingLeg_T2(bool&, const char*) {}
struct TrafficFixture {
    using M = TrafficEntityModule;
    using TotalTrafficBitArray = M::TotalTrafficBitArray;
    decltype(M::maJobs) maJobs;
    decltype(M::maParams) maParams{};
    decltype(M::maParamTransforms) maParamTransforms{};
    decltype(M::maVehicles) maVehicles{};
    decltype(M::maVehicleTransforms) maVehicleTransforms{};
    decltype(M::maVehicleAxles) maVehicleAxles{};
    decltype(M::maVehicleTypeRuntime) maVehicleTypeRuntime{};
    decltype(M::mRaceCarState) mRaceCarState{};
    Random mEffectRand{};
    Hull hull{};
    LaneRung rungs[2]{};
    Section section{};
    float rungLengths[2] = {0, 1000};
    Hull* hulls[1] = {&hull};
    struct Data { Hull** mpapHulls; u16 muNumHulls; } data{hulls, 1};
    Data* mpData = &data;
    struct Camera { Vector3 position{}; Vector3 GetPosition() const { return position; } } mCameraLastFrame;
    u32 muNumUpdateVehiclesJobs = 4;
    f32 mfSimTimeStep = 1.0f / 60, mfSimTimeSinceLastDecision = 0.1f, mfCrashSliderFinalValue = 1;
    EActiveRaceCarIndex meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    bool mbHardcoreSwerveForMode = false, mbGameModeAllowsSwerving = false, mbDEBUGStopTrafficMoving = false;
    bool decision = false;
    unsigned creates = 0, cacheCalls = 0;
    std::vector<Request> requests;
    bool IsDecisionFrame() const { return decision; }
    void UpdateVehicles_CreateNewVehicles(const TestPostInput*) { ++creates; }
    void CacheRaceCarState(const void*) { ++cacheCalls; }
    void UpdateVehicles(const TestPostInput*, TestPostOutput*);
    void SendPhysicalRequests(TestPreOutput*, TotalTrafficBitArray*);
    void SafeRequestMakeVehiclePhysical(u16 v, PhysicalReason reason, EntityId target,
        BrnPhysics::Vehicle::ETrafficType, BrnPhysics::Vehicle::eCrashTrafficType, TestPreOutput*, TotalTrafficBitArray*)
    { requests.emplace_back(v, static_cast<s8>(reason), target.muValue); }

    TrafficFixture()
    {
        for (auto& job : maJobs) job.Construct();
        mEffectRand.Construct();
        hull.mpaRungs = rungs; hull.muNumRungs = 2;
        hull.mpaSections = &section; hull.muNumSections = 1;
        hull.mpafCumulativeRungLengths = rungLengths;
        section.muNumRungs = 2; section.mfLength = 1000;
        rungs[0].maPoints[0] = {0, 0.25f, 0, 0}; rungs[0].maPoints[1] = {4000, 0.25f, 0, 0};
        rungs[1].maPoints[0] = {0, 0.25f, 1000, 0}; rungs[1].maPoints[1] = {4000, 0.25f, 1000, 0};
        for (auto& type : maVehicleTypeRuntime) {
            type.mBBoxHalfSize = {1, 1, 2, 0};
            type.mCabPivot_TrailerPivot_BackAxle_FwdAxle = {0, 0, -2, 2};
        }
        for (u32 i = 0; i < KU_MAX_TOTAL_TRAFFIC; ++i) {
            auto& v = maVehicles[i];
            v.mxFlags = i % 7 ? Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_HASENTITY : 0;
            if (i % 11 == 0) v.mxFlags &= ~Vehicle::E_FLAG_HASENTITY;
            if (i % 5 == 0) v.mxFlags |= Vehicle::E_FLAG_PHYSICAL;
            v.miPhysicalReason = E_PHYSICALREASON_NORMAL;
            v.mfRandomVal = float(i % 17) / 17;
            v.mSpeed_DistAcrossLane_SwerveAmount_W = {12, 0.3f, 0, 0};
            maVehicleTransforms[i].SetIdentity();
            maVehicleTransforms[i].Pos() = {1000 + float(i) * 3, 0, 10, 0};
            auto& axles = maVehicleAxles[i];
            axles.mFrontAxle.Initialise(); axles.mBackAxle.Initialise();
            axles.mFrontAxle.mPosAndWheelRadius = {1000 + float(i) * 3, 0, 12, 0.35f};
            axles.mBackAxle.mPosAndWheelRadius = {1000 + float(i) * 3, 0, 8, 0.35f};
            if (i >= KU_MAX_PARAMS) continue;
            auto& p = maParams[i]; p.mxFlags = Param::E_FLAG_ALIVE;
            p.miBehaviour = i % 3 ? 0 : 6; p.mfSpeed = 10 + float(i % 13); p.mfFrontDist = 4;
            p.mxEffectAndHistoryState = u8(i & 1); p.mSympCrashTarget.muValue = 0x1000000u + i;
            p.mauNeighbourData[0] = p.mauNeighbourData[1] = 0xffff;
            maParamTransforms[i].mLerpedPosAndSpeed = {1000 + float(i) * 3, 0, 15, p.mfSpeed};
            maParamTransforms[i].mDirAndAccel = {0, 0, 1, 0};
            maParamTransforms[i].mRight = {1, 0, 0, 0};
        }
    }
    void RunReference()
    {
        for (u32 j = 0; j < muNumUpdateVehiclesJobs; ++j) {
            auto& p = maJobs[j].mJobData.mParams.mUpdateVehicles;
            p.Construct(j * (400 / muNumUpdateVehiclesJobs), j + 1 == muNumUpdateVehiclesJobs ? 400 : (j + 1) * (400 / muNumUpdateVehiclesJobs),
                hulls, 1, maParams, maParamTransforms, maVehicles, maVehicleTransforms, maVehicleAxles,
                maVehicleTypeRuntime, &mRaceCarState, mfSimTimeStep, mfSimTimeSinceLastDecision, &mEffectRand,
                meLocalPlayerIndex, mbHardcoreSwerveForMode, mbGameModeAllowsSwerving, mbDEBUGStopTrafficMoving,
                mCameraLastFrame.GetPosition(), mfCrashSliderFinalValue, nullptr);
            p.SetOutputs(maJobs[j].GetNewPhysicalRequests());
            UpdateVehiclesJob kernel; kernel.Execute(&p);
            (void)mEffectRand.RandomBool();
        }
    }
};
}
static BrnTraffic::TrafficFixture* activeFixture;
static HWND window;
static DWORD owner;
static std::atomic<unsigned> active{0}, peak{0}, badSnapshots{0}, badContexts{0}, badMessages{0};
static std::mutex observationLock;
static std::map<void*, DWORD> contexts;
static std::set<DWORD> threads;
bool TrafficTestBeforeExecute(void* worker, BrnTraffic::JobParams* data)
{
    bool valid = false;
    for (auto& job : activeFixture->maJobs) valid |= data == &job.mJobData.mParams;
    if (!valid) { ++badSnapshots; return false; }
    {
        std::lock_guard<std::mutex> lock(observationLock);
        const auto id = GetCurrentThreadId(); threads.insert(id);
        auto found = contexts.find(worker);
        if (found != contexts.end() && found->second != id) { ++badContexts; return false; }
        contexts[worker] = id;
    }
    const unsigned n = ++active; unsigned old = peak;
    while (old < n && !peak.compare_exchange_weak(old, n)) {}
    DWORD_PTR reply = 0;
    if (!SendMessageTimeoutW(window, WM_APP + 1, 25, 0, SMTO_BLOCK | SMTO_ABORTIFHUNG, 2000, &reply) || reply != 42)
        ++badMessages;
    if (GetCurrentThreadId() != owner) Sleep(2);
    --active;
    return true;
}
static LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l)
{ return m == WM_APP + 1 ? w + 17 : DefWindowProcW(h, m, w, l); }
#include "pc_traffic_jobs.inc"

int main()
{
    bool parallel = true;
    _putenv_s("BRN_TRAFFIC_JOBS", "1"); _putenv_s("BRN_TRAFFIC_DIAG", ""); _putenv_s("BRN_WORLD_CAMTRAFFIC", "");
#ifdef TRAFFIC_TEST_SERIAL
    parallel = false; _putenv_s("BRN_TRAFFIC_JOBS", "0");
#endif
#ifdef TRAFFIC_TEST_DIAGNOSTIC
    parallel = false; _putenv_s("BRN_TRAFFIC_DIAG", "1");
#endif
    owner = GetCurrentThreadId();
    WNDCLASSW wc{}; wc.lpfnWndProc = WindowProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TrafficJobsFixture"; RegisterClassW(&wc);
    window = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    Allocator allocator; EA::Jobs::SetAllocator(&allocator); scheduler.Initialize(64, 64);
    for (unsigned i = 0; i < 3; ++i) { EA::Jobs::JobThreadParameters p; scheduler.AddThread(p); }
    for (u32 numJobs : {4u, 3u, 1u}) {
        auto actual = std::make_unique<BrnTraffic::TrafficFixture>();
        auto reference = std::make_unique<BrnTraffic::TrafficFixture>();
        actual->muNumUpdateVehiclesJobs = reference->muNumUpdateVehiclesJobs = numJobs;
        activeFixture = actual.get();
        for (unsigned step = 0; step < 4; ++step) {
            actual->decision = (step % 2 == 0);
            actual->mCameraLastFrame.position = reference->mCameraLastFrame.position =
                step == 0 ? Vector3{0, 0, 0, 0} : Vector3{1000 + float(step - 1) * 600, 0, 10, 0};
            actual->mbDEBUGStopTrafficMoving = reference->mbDEBUGStopTrafficMoving = step == 2;
            if (step == 3) {
                for (auto* f : {actual.get(), reference.get()}) {
                    f->mbGameModeAllowsSwerving = true;
                    f->mRaceCarState.mRaceCarPositions.Append({2100, 0, 25, 0});
                    f->mRaceCarState.mRaceCarLinearVelocities.Append({0, 0, 30, 0});
                    f->mRaceCarState.mRaceCarSpeeds.Append(rw::math::vpu::Splat(30));
                    f->mRaceCarState.mRaceCarXZVelocityDirs.Append({0, 0, 1, 0});
                }
            }
            BrnTraffic::TestPostInput in; BrnTraffic::TestPostOutput out; BrnTraffic::TestPreOutput physical;
            actual->UpdateVehicles(&in, &out);
            bool joined = true;
            for (auto& job : actual->maJobs) joined &= !job.mbRunningJob;
            Check(joined, "module joins every submitted slice before returning");
            // Safely drain an intentionally broken join before observing output.
            for (auto& job : actual->maJobs) if (job.mbRunningJob) job.WaitOn();
            reference->RunReference();
            Check(std::memcmp(actual->maVehicles, reference->maVehicles, sizeof(actual->maVehicles)) == 0,
                  "complete vehicle state matches serial kernel bit for bit");
            Check(std::memcmp(actual->maVehicleTransforms, reference->maVehicleTransforms, sizeof(actual->maVehicleTransforms)) == 0
                  && std::memcmp(actual->maVehicleAxles, reference->maVehicleAxles, sizeof(actual->maVehicleAxles)) == 0,
                  "transforms and axles match serial kernel including untouched static/trailer slots");
            Check(std::memcmp(&actual->mEffectRand, &reference->mEffectRand, sizeof(actual->mEffectRand)) == 0,
                  "master random state retains one step per original slice");
            bool snapshots = true;
            for (u32 j = 0; j < numJobs; ++j) {
                const auto& a = actual->maJobs[j].mJobData.mParams.mUpdateVehicles;
                const auto& b = reference->maJobs[j].mJobData.mParams.mUpdateVehicles;
                snapshots &= a.muBeginVehicle == b.muBeginVehicle && a.muEndVehicle == b.muEndVehicle
                    && std::memcmp(&a.mEffectRand, &b.mEffectRand, sizeof(a.mEffectRand)) == 0;
            }
            Check(snapshots, "owned descriptors preserve bounds and random snapshots including remainder slice");
            Check(std::memcmp(actual->maParams, reference->maParams, sizeof(actual->maParams)) == 0
                  && std::memcmp(actual->maParamTransforms, reference->maParamTransforms, sizeof(actual->maParamTransforms)) == 0
                  && std::memcmp(&actual->mRaceCarState, &reference->mRaceCarState, sizeof(actual->mRaceCarState)) == 0,
                  "worker inputs and cached race-car data remain immutable");
            actual->requests.clear(); reference->requests.clear();
            actual->SendPhysicalRequests(&physical, nullptr);
            // Independent expected order: ascending job, then list insertion.
            for (u32 j = 0; j < numJobs; ++j) {
                auto& list = *reference->maJobs[j].GetNewPhysicalRequests();
                for (u32 i = 0; i < list.GetLength(); ++i)
                    reference->requests.emplace_back(list[i].muVehicle, list[i].miReason, list[i].mTargetEntityId.muValue);
                list.Clear();
            }
            Check(actual->requests == reference->requests && !actual->requests.empty(),
                  "physical requests retain original order and reasons");
            bool empty = true; for (auto& job : actual->maJobs) empty &= job.GetNewPhysicalRequests()->GetLength() == 0;
            Check(empty, "owner drains every physical request list after joining");
        }
        Check(actual->creates == 2 && actual->cacheCalls == 4, "creation and race-car snapshot stay on owner before splitting");
        Check(actual->maVehicleAxles[1].mFrontAxle.mPosAndWheelRadius.y > 0.2f,
              "near-camera case executes actual axle-to-road collision");
    }
    Check(badSnapshots == 0 && badContexts == 0, "jobs use owned snapshots and isolated native worker state");
    Check(badMessages == 0, "owner wait pumps messages required by workers");
    Check(parallel ? peak >= 2 && !threads.count(owner) : peak == 1 && threads.size() == 1 && threads.count(owner),
          "native jobs overlap, or selected serial diagnostic stays ordered on owner");
    scheduler.Destroy(); DestroyWindow(window); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    std::printf("PCTrafficJobs: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
