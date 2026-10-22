// Compile the actual proposed producer and handler; no copied queue algorithm.
#define BRN_RWAC_COMMAND_TRACE_FIXTURE 1
#include "GameShared/GameClasses/System/PC/CgsRwacCommandTracePC.h"
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if defined(BRN_RWAC_BASELINE_PRODUCER)
#include "evidence/rwac_command_capacity/PlugIn_b823.cpp"
#else
#include "../vendor/renderware/src/rw/audio/core/PlugIn.cpp"
#endif

// Fixture-only link closure for the other methods in the complete included TU.
// Capacity cases call only the real SetAttribute producer and handler. The
// singleton slot is genuine test storage, not a constructed audio System or a
// replacement for an engine method. The unused CreateInstance body needs its
// definition at link time; no capacity case calls that creation path.
namespace rw { namespace audio { namespace core {
extern "C" { System* off_83271928 = nullptr; }

// The unused PlugIn::Event body references Send::EventEvent. This boundary has
// no success result and can never act as an engine implementation: an accidental
// entry leaves an explicit marker and fails the owned fixture child immediately.
Send* Send::EventEvent(Send*, int, void* const*)
{
    std::fputs("FIXTURE_UNEXPECTED_BOUNDARY rw::audio::core::Send::EventEvent\n", stderr);
    std::fflush(stderr);
    ::TerminateProcess(::GetCurrentProcess(), 0xE0420003u);
    __fastfail(7);
}
} } }

namespace
{
struct StoppedBeforeWrite {};
std::string gEmergency;
int gChecks = 0;
int gFailures = 0;
int gFirstPassChecks = 0;
int gFirstPassFailures = 0;
void Check(bool lbCondition, const char* lpcLabel)
{
    ++gChecks;
    if (!lbCondition)
    {
        ++gFailures;
        std::printf("FAIL %s\n", lpcLabel);
    }
}

void CheckFirstPass(bool lbCondition, const char* lpcLabel)
{
    ++gFirstPassChecks;
    if (!lbCondition)
    {
        ++gFirstPassFailures;
        std::printf("FAIL_FIRST_PASS %s\n", lpcLabel);
    }
}

void ResetTrace()
{
    using namespace CgsSystem::RwacCommandTracePC;
    for (unsigned lu = 0; lu < EventCount; ++lu)
    {
        gState.maEvent[lu].mCount.store(0);
        gState.maEvent[lu].mTick.store(0);
    }
    gState.muProducerFramesAtLastPass.store(0);
    gState.muEngineSystem.store(0);
    gState.muProducerSystem.store(0);
    gState.muPhase.store(0);
    gState.muDeviceOpen.store(0);
    gState.muLastSubmitError.store(0);
    gState.muLastVoiceError.store(0);
    gState.muLastCriticalError.store(0);
    gState.muEngineCallbackRegistration.store(0);
    gEmergency.clear();
}

struct World
{
    rw::audio::core::System mSystem{};
    rw::audio::core::PlugIn mPlugin;
    rw::audio::core::PlugIn::Attribute_t mAttribute[1]{};
    // The extra 64-byte tail makes the original producer a safe negative
    // control: its one out-of-capacity write is inside the fixture's backing.
    std::vector<unsigned char> mStorage;
    explicit World(unsigned luCapacity) : mStorage(static_cast<size_t>(luCapacity) + 80u, 0xA5)
    {
        mSystem.mpDeferredRingBase = reinterpret_cast<char*>(mStorage.data() + 16);
        mSystem.muCommandBufferSize = luCapacity;
        mSystem.muDeferredRingCursor = 0;
        mSystem.muDeferredRingHighWater = 7;
        mSystem.muFrameCounter = 41;
        mPlugin.mpSystemUseGetSystemAccessor = &mSystem;
        mPlugin.mpAttribute = mAttribute;
    }
};

bool Enqueue(World& lrWorld, float lfValue)
{
    try
    {
        auto* lpResult = rw::audio::core::PlugIn::SetAttribute(&lrWorld.mPlugin, 0, lfValue);
        Check(lpResult == &lrWorld.mPlugin, "valid producer return preserves target");
        return true;
    }
    catch (const StoppedBeforeWrite&)
    {
        return false;
    }
}

void ExactBoundary()
{
    ResetTrace();
    World lWorld(96);
    for (int li = 0; li < 4; ++li)
        Check(Enqueue(lWorld, static_cast<float>(li + 1)), "four exact-fit records accepted");
    Check(lWorld.mSystem.muDeferredRingCursor == 96, "exact fit uses all capacity");
    Check(lWorld.mAttribute[0].mfValue == 0, "valid stores stay deferred");
    const auto lBefore = lWorld.mStorage;
    Check(!Enqueue(lWorld, 99), "overflow stops before fifth record");
    Check(lWorld.mSystem.muDeferredRingCursor == 96, "overflow leaves cursor untouched");
    Check(lWorld.mStorage == lBefore, "overflow leaves full backing and canaries untouched");
    Check(gEmergency.find("STOP_BEFORE_WRITE") != std::string::npos, "overflow has explicit synchronous record");
    Check(gEmergency.find("pending=96 capacity=96 record=24 highwater=7 system_frame=41") != std::string::npos,
          "record contains actual pending/capacity/stride/highwater/frame");
    Check(gEmergency.size() < 2048 && !gEmergency.empty() && gEmergency.back() == '\n',
          "record is bounded and complete");
    for (unsigned lu = 0; lu < 4; ++lu)
    {
        const int liConsumed = rw::audio::core::PlugIn::SetAttributeHandler(
            lWorld.mSystem.mpDeferredRingBase + lu * 24u);
        Check(liConsumed == 24, "real handler preserves native stride");
        Check(lWorld.mAttribute[0].mfValue == static_cast<float>(lu + 1),
              "real handler replays valid records in order");
    }
}

void InvalidExtents()
{
    for (unsigned luCapacity : {0u, 23u})
    {
        ResetTrace();
        World lWorld(luCapacity);
        const auto lBefore = lWorld.mStorage;
        Check(!Enqueue(lWorld, 1), "undersized capacity stops");
        Check(lWorld.mSystem.muDeferredRingCursor == 0 && lWorld.mStorage == lBefore,
              "undersized capacity makes no store");
    }
    for (auto lPair : {std::array<unsigned, 2>{97u, 96u},
                       std::array<unsigned, 2>{(std::numeric_limits<unsigned>::max)() - 7u,
                                               (std::numeric_limits<unsigned>::max)()}})
    {
        ResetTrace();
        World lWorld(96);
        lWorld.mSystem.muDeferredRingCursor = lPair[0];
        lWorld.mSystem.muCommandBufferSize = lPair[1];
        const auto lBefore = lWorld.mStorage;
        Check(!Enqueue(lWorld, 1), "over-capacity or wrapping extent stops before pointer arithmetic");
        Check(lWorld.mSystem.muDeferredRingCursor == lPair[0] && lWorld.mStorage == lBefore,
              "invalid extent leaves bytes and cursor unchanged");
    }
    ResetTrace();
    World lWorld(96);
    lWorld.mSystem.mpDeferredRingBase = nullptr;
    Check(!Enqueue(lWorld, 1), "missing ring is recorded before pointer arithmetic");
}

void TraceClassification(bool lbBurst)
{
    using namespace CgsSystem::RwacCommandTracePC;
    ResetTrace();
    World lWorld(196608);
    BeginSystemEpoch(&lWorld.mSystem, lWorld.mSystem.mpDeferredRingBase,
                     lWorld.mSystem.muCommandBufferSize);
    MarkEngineBegin(&lWorld.mSystem);
    MarkConsumerEvent(CommandPassBegin, 3);
    MarkConsumerEvent(CommandPassEnd, 4);
    MarkConsumerEvent(EngineEnd, 7);
    if (lbBurst)
        MarkProducerFrame(&lWorld.mSystem);
    unsigned luAccepted = 0;
    for (unsigned lu = 0; lu <= 8192; ++lu)
    {
        if (!lbBurst && lu % 128u == 0)
            MarkProducerFrame(&lWorld.mSystem);
        if (Enqueue(lWorld, 1))
            ++luAccepted;
    }
    Check(luAccepted == 8192 && lWorld.mSystem.muDeferredRingCursor == 196608,
          "native 0x30000 capacity accepts exactly 8192 attribute records");
    Check(gEmergency.find(lbBurst ? "frames_since_pass=1 " : "frames_since_pass=65 ") != std::string::npos,
          "trace distinguishes one producer burst from 65 frames without consumer progress");
    Check(gEmergency.find("pass=1/1 ") != std::string::npos,
          "trace retains completed consumer progress");
    std::printf("TRACE_%s %s", lbBurst ? "BURST" : "STARVATION", gEmergency.c_str());
}

void ErrorState()
{
    using namespace CgsSystem::RwacCommandTracePC;
    ResetTrace();
    World lWorld(0);
    MarkSubmit(E_FAIL);
    MarkVoiceError(E_POINTER);
    MarkCriticalError(E_ABORT);
    Check(!Enqueue(lWorld, 1), "error-state case stops at capacity");
    Check(gEmergency.find("submit_errors=1 last_submit_hr=0x80004005") != std::string::npos,
          "submit failure survives to failure record");
    Check(gEmergency.find("voice_errors=1 last_voice_hr=0x80004003") != std::string::npos,
          "voice error survives to failure record");
    Check(gEmergency.find("critical_errors=1 last_critical_hr=0x80004004") != std::string::npos,
          "critical engine error survives to failure record");
}

void FirstPassWitness()
{
    using namespace CgsSystem::RwacCommandTracePC;
    ResetTrace();
    gState.muSystemEpoch.store(0, std::memory_order_relaxed);
    gState.mbActiveRecordWritten.store(false, std::memory_order_relaxed);

    World lWorld(196608);
    BeginSystemEpoch(&lWorld.mSystem, lWorld.mSystem.mpDeferredRingBase,
                     lWorld.mSystem.muCommandBufferSize);
    MarkProducerFrame(&lWorld.mSystem);
    MarkEngineBegin(&lWorld.mSystem);
    MarkConsumerEvent(CommandPassBegin, 3);
    MarkConsumerEvent(CommandPassEnd, 4);

    MarkDeviceOpened();
    MarkDeviceClosed();
    MarkDeviceOpened();
    MarkDeviceClosed();
    MarkDeviceOpened();
    MarkSubmit(S_OK);
    MarkSubmit(S_OK);
    MarkSubmit(S_OK);
    MarkSubmit(E_FAIL);
    for (int li = 0; li < 5; ++li)
        MarkVoiceError(E_POINTER);
    for (int li = 0; li < 6; ++li)
        MarkCriticalError(E_ABORT);
    MarkEngineCallbackRegistration(E_ACCESSDENIED);

    EmitFirstActiveOccupancy(&lWorld.mSystem, lWorld.mSystem.mpDeferredRingBase,
                             0, lWorld.mSystem.muCommandBufferSize,
                             lWorld.mSystem.muDeferredRingHighWater,
                             lWorld.mSystem.muFrameCounter);
    CheckFirstPass(gEmergency.find(
        "[rwac-occupancy] FIRST_PASS_WITNESS enabled=1 consumer_pass_seen=1") != std::string::npos,
        "actual matching producer/pass state emits FIRST_PASS_WITNESS");
    CheckFirstPass(gEmergency.find(
        "system_frame=41 epoch=1 producer_frames_for_trace_epoch=1 "
        "pass_producer_frames_for_trace_epoch=1 frames_since_pass=0") != std::string::npos,
        "witness carries deterministic matching epoch/frame inputs");
    CheckFirstPass(gEmergency.find(
        "device_open_process_global=1 device_opens_process_global=3 device_closes_process_global=2") != std::string::npos,
        "device-open state and process-global open/close counts keep their distinct known values");
    CheckFirstPass(gEmergency.find(
        "submits_process_global=4 submit_errors_process_global=1 last_submit_hr=0x80004005") != std::string::npos,
        "submit count, error count and HRESULT use their own known fields");
    CheckFirstPass(gEmergency.find(
        "voice_errors_process_global=5 critical_errors_process_global=6 "
        "callback_registration_process_global=0x80070005") != std::string::npos,
        "voice, critical and callback-registration values stay aligned");
    CheckFirstPass(gEmergency.size() < 2048 && !gEmergency.empty() && gEmergency.back() == '\n',
        "witness record remains bounded and complete");
}
}

namespace CgsDev { namespace Log {
void WriteToLogEmergency(const char* lpText) { gEmergency = lpText; }
} }
namespace CgsSystem { namespace RwacCommandTracePC {
[[noreturn]] void FixtureStopBeforeWrite() { throw StoppedBeforeWrite{}; }
} }

int main()
{
    static_assert(sizeof(rw::audio::core::PlugInSetAttributeCommand) == 24,
                  "This fixture targets the shipped x64 command layout");
    ExactBoundary();
#if !defined(BRN_RWAC_BASELINE_CONTROL)
    InvalidExtents();
    TraceClassification(true);
    TraceClassification(false);
    ErrorState();
    FirstPassWitness();
#endif
    std::printf("RWAC_CAPACITY checks=%d failures=%d\n", gChecks, gFailures);
#if !defined(BRN_RWAC_BASELINE_CONTROL)
    std::printf("RWAC_FIRST_PASS checks=%d failures=%d\n", gFirstPassChecks, gFirstPassFailures);
#endif
    return gFailures ? 1 : 0;
}
