#pragma once

// FLAG PC-platform leaf: private diagnostic for a deferred audio-command overwrite.
// Fixed atomic storage only; callbacks never log, allocate or wait. The failure
// record is emitted by the observed SetAttribute producer before changing memory.
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <intrin.h>
#include <mutex>

#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace CgsSystem { namespace RwacCommandTracePC {

enum Event : unsigned
{
    DeviceCallbackBegin, DeviceCallbackEnd,
    EngineBegin, CommandLockAcquired, CommandPassBegin, CommandPassEnd,
    MixBegin, MixEnd, EngineEnd,
    ProducerFrame, DeviceOpened, DeviceClosed,
    SubmitCount, SubmitError, VoiceError, CriticalError, EventCount
};

struct Counter
{
    std::atomic<std::uint64_t> mCount{0};
    std::atomic<std::uint64_t> mTick{0};
};

struct State
{
    Counter maEvent[EventCount];
    std::atomic<std::uintptr_t> muEngineSystem{0};
    std::atomic<std::uintptr_t> muProducerSystem{0};
    std::atomic<std::uintptr_t> muEpochSystem{0};
    std::atomic<std::uintptr_t> muPassSystem{0};
    std::atomic<std::uint64_t> muSystemEpoch{0};
    std::atomic<std::uint64_t> muProducerSystemEpoch{0};
    std::atomic<std::uint64_t> muPassSystemEpoch{0};
    std::atomic<std::uint64_t> muEngineSystemEpoch{0};
    std::atomic<std::uint64_t> muProducerFramesAtEpochStart{0};
    std::atomic<std::uint64_t> muProducerFramesAtLastPass{0};
    std::atomic<std::uint64_t> muProducerFramesForTraceEpoch{0};
    std::atomic<std::uint64_t> muProducerFramesAtLastTracePass{0};
    std::atomic<unsigned> muPhase{0};
    std::atomic<unsigned> muDeviceOpen{0};
    std::atomic<unsigned> muLastSubmitError{0};
    std::atomic<unsigned> muLastVoiceError{0};
    std::atomic<unsigned> muLastCriticalError{0};
    std::atomic<unsigned> muEngineCallbackRegistration{0};
    std::atomic<bool> mbArmRecordWritten{false};
    std::atomic<bool> mbActiveRecordWritten{false};
};

// One shared COMDAT instance across the diagnostic executable's C++17 TUs.
inline State gState;

inline std::once_flag gConfigureOnce;
inline std::atomic<bool> gEnabled{false};
#if !defined(BRN_RWAC_COMMAND_TRACE_FIXTURE)
inline std::atomic_flag gFailureRecorded = ATOMIC_FLAG_INIT;
#endif

// Sample once before System/device startup. The private native witness is opt-in:
// only the exact value BRN_RWAC_COMMAND_TRACE=1 enables counters and the fatal guard.
inline void ConfigureFromEnvironment()
{
    std::call_once(gConfigureOnce, []()
    {
        const char* lpcValue = std::getenv("BRN_RWAC_COMMAND_TRACE");
        gEnabled.store(lpcValue && lpcValue[0] == '1' && lpcValue[1] == '\0',
                       std::memory_order_relaxed);
    });
}

inline bool Enabled() noexcept
{
#if defined(BRN_RWAC_COMMAND_TRACE_FIXTURE)
    return true;
#else
    return gEnabled.load(std::memory_order_relaxed);
#endif
}

inline void MarkEnabled(Event leEvent)
{
    gState.maEvent[leEvent].mTick.store(::GetTickCount64(), std::memory_order_relaxed);
    gState.maEvent[leEvent].mCount.fetch_add(1, std::memory_order_relaxed);
}

inline void Mark(Event leEvent)
{
    if (Enabled())
        MarkEnabled(leEvent);
}

inline std::uint64_t Count(Event leEvent);

inline void BeginSystemEpoch(const void* lpSystem, const void* lpRing, unsigned luCapacity)
{
    if (!Enabled() || !lpSystem)
        return;

    const std::uint64_t luEpoch =
        gState.muSystemEpoch.fetch_add(1, std::memory_order_relaxed) + 1;
    gState.muEpochSystem.store(reinterpret_cast<std::uintptr_t>(lpSystem),
                               std::memory_order_relaxed);
    gState.muProducerSystem.store(reinterpret_cast<std::uintptr_t>(lpSystem),
                                  std::memory_order_relaxed);
    gState.muProducerSystemEpoch.store(luEpoch, std::memory_order_relaxed);
    gState.muProducerFramesAtEpochStart.store(Count(ProducerFrame),
                                              std::memory_order_relaxed);
    gState.muProducerFramesAtLastPass.store(0, std::memory_order_relaxed);
    gState.muProducerFramesForTraceEpoch.store(0, std::memory_order_relaxed);
    gState.muProducerFramesAtLastTracePass.store(0, std::memory_order_relaxed);
    gState.muEngineSystem.store(0, std::memory_order_relaxed);
    gState.muEngineSystemEpoch.store(0, std::memory_order_relaxed);
    gState.muPassSystem.store(0, std::memory_order_relaxed);
    gState.muPassSystemEpoch.store(0, std::memory_order_relaxed);
    gState.muPhase.store(0, std::memory_order_relaxed);

    // One bounded first record proves the exact opt-in was read and the live
    // System allocated its command buffer. It does not claim consumer progress.
    if (!gState.mbArmRecordWritten.exchange(true, std::memory_order_relaxed))
    {
        char lacLine[256] = {};
        std::snprintf(lacLine, sizeof(lacLine),
            "[rwac-occupancy] ARMED enabled=1 system=%p ring=%p capacity=%u epoch=%llu\n",
            const_cast<void*>(lpSystem), const_cast<void*>(lpRing), luCapacity,
            static_cast<unsigned long long>(luEpoch));
        CgsDev::Log::WriteToLogEmergency(lacLine);
    }
}

inline void MarkProducerFrame(const void* lpSystem)
{
    if (!Enabled())
        return;

    const std::uintptr_t luSystem = reinterpret_cast<std::uintptr_t>(lpSystem);
    gState.muProducerSystem.store(luSystem, std::memory_order_relaxed);
    const std::uintptr_t luEpochSystem =
        gState.muEpochSystem.load(std::memory_order_relaxed);
    const std::uint64_t luEpoch = gState.muSystemEpoch.load(std::memory_order_relaxed);
    const bool lbCurrentSystemEpoch = luSystem != 0 && luSystem == luEpochSystem && luEpoch != 0;
    gState.muProducerSystemEpoch.store(lbCurrentSystemEpoch ? luEpoch : 0,
                                       std::memory_order_relaxed);
    if (lbCurrentSystemEpoch)
        gState.muProducerFramesForTraceEpoch.fetch_add(1, std::memory_order_relaxed);
    MarkEnabled(ProducerFrame);
}

inline void MarkEngineBegin(const void* lpSystem)
{
    if (!Enabled())
        return;

    const std::uintptr_t luSystem = reinterpret_cast<std::uintptr_t>(lpSystem);
    gState.muEngineSystem.store(luSystem, std::memory_order_relaxed);
    const std::uintptr_t luEpochSystem =
        gState.muEpochSystem.load(std::memory_order_relaxed);
    gState.muEngineSystemEpoch.store(
        luSystem != 0 && luSystem == luEpochSystem
            ? gState.muSystemEpoch.load(std::memory_order_relaxed)
            : 0,
        std::memory_order_relaxed);
    gState.muPhase.store(1, std::memory_order_relaxed);
    MarkEnabled(EngineBegin);
}

inline void MarkConsumerEvent(Event leEvent, unsigned luPhase)
{
    if (!Enabled())
        return;

    gState.muPhase.store(luPhase, std::memory_order_relaxed);
    if (leEvent == CommandPassEnd)
    {
        const std::uintptr_t luEngineSystem =
            gState.muEngineSystem.load(std::memory_order_relaxed);
        const std::uint64_t luEngineEpoch =
            gState.muEngineSystemEpoch.load(std::memory_order_relaxed);
        const std::uintptr_t luEpochSystem =
            gState.muEpochSystem.load(std::memory_order_relaxed);
        const std::uint64_t luSystemEpoch =
            gState.muSystemEpoch.load(std::memory_order_relaxed);
        gState.muPassSystem.store(luEngineSystem, std::memory_order_relaxed);
        gState.muPassSystemEpoch.store(luEngineEpoch, std::memory_order_relaxed);
        const std::uint64_t luFrames =
            Count(ProducerFrame) -
            gState.muProducerFramesAtEpochStart.load(std::memory_order_relaxed);
        gState.muProducerFramesAtLastPass.store(luFrames, std::memory_order_relaxed);
        const bool lbMatchingSystemEpoch =
            luEngineSystem != 0 && luEngineSystem == luEpochSystem &&
            luEngineEpoch != 0 && luEngineEpoch == luSystemEpoch;
        gState.muProducerFramesAtLastTracePass.store(
            lbMatchingSystemEpoch
                ? gState.muProducerFramesForTraceEpoch.load(std::memory_order_relaxed)
                : 0,
            std::memory_order_relaxed);
    }
    MarkEnabled(leEvent);
}

inline void MarkSubmit(HRESULT lResult)
{
    if (!Enabled())
        return;

    MarkEnabled(SubmitCount);
    if (FAILED(lResult))
    {
        gState.muLastSubmitError.store(static_cast<unsigned>(lResult), std::memory_order_relaxed);
        MarkEnabled(SubmitError);
    }
}

inline void MarkVoiceError(HRESULT lResult)
{
    if (!Enabled())
        return;

    gState.muLastVoiceError.store(static_cast<unsigned>(lResult), std::memory_order_relaxed);
    MarkEnabled(VoiceError);
}

inline void MarkCriticalError(HRESULT lResult)
{
    if (!Enabled())
        return;

    gState.muLastCriticalError.store(static_cast<unsigned>(lResult), std::memory_order_relaxed);
    MarkEnabled(CriticalError);
}

inline void MarkEngineCallbackRegistration(HRESULT lResult)
{
    if (Enabled())
        gState.muEngineCallbackRegistration.store(static_cast<unsigned>(lResult),
                                                  std::memory_order_relaxed);
}

inline void MarkDeviceOpened()
{
    if (!Enabled())
        return;
    gState.muDeviceOpen.store(1, std::memory_order_relaxed);
    MarkEnabled(DeviceOpened);
}

inline void MarkDeviceClosed()
{
    if (!Enabled())
        return;
    gState.muDeviceOpen.store(0, std::memory_order_relaxed);
    MarkEnabled(DeviceClosed);
}

inline std::uint64_t Count(Event leEvent)
{
    return gState.maEvent[leEvent].mCount.load(std::memory_order_relaxed);
}

inline std::uint64_t Age(Event leEvent, std::uint64_t luNow)
{
    const std::uint64_t luTick = gState.maEvent[leEvent].mTick.load(std::memory_order_relaxed);
    return luTick && luNow >= luTick ? luNow - luTick : 0;
}

// Emit at most one process-wide occupancy snapshot after a matching System
// producer frame and completed command pass in the current trace epoch. The
// callback/device event values below are process-global context only; they do
// not gate the witness or prove same-epoch callback/device health. Called after
// the existing System lock has been released.
inline void EmitFirstActiveOccupancy(const void* lpSystem, const void* lpRing,
    unsigned luCursor, unsigned luCapacity, unsigned luHighWater, unsigned luSystemFrame)
{
    if (!Enabled() || !lpSystem)
        return;

    const std::uintptr_t luSystem = reinterpret_cast<std::uintptr_t>(lpSystem);
    const std::uint64_t luEpoch = gState.muSystemEpoch.load(std::memory_order_relaxed);
    if (luEpoch == 0 || gState.muEpochSystem.load(std::memory_order_relaxed) != luSystem)
        return;

    const std::uint64_t luProducerFrames =
        gState.muProducerFramesForTraceEpoch.load(std::memory_order_relaxed);
    const std::uint64_t luProducerFramesAtPass =
        gState.muProducerFramesAtLastTracePass.load(std::memory_order_relaxed);
    const bool lbProducerValid =
        gState.muProducerSystem.load(std::memory_order_relaxed) == luSystem &&
        gState.muProducerSystemEpoch.load(std::memory_order_relaxed) == luEpoch &&
        luProducerFrames != 0;
    const bool lbPassValid =
        gState.muEngineSystem.load(std::memory_order_relaxed) == luSystem &&
        gState.muEngineSystemEpoch.load(std::memory_order_relaxed) == luEpoch &&
        gState.muPassSystem.load(std::memory_order_relaxed) == luSystem &&
        gState.muPassSystemEpoch.load(std::memory_order_relaxed) == luEpoch &&
        luProducerFramesAtPass != 0 &&
        luProducerFrames >= luProducerFramesAtPass;
    if (!lbProducerValid || !lbPassValid)
        return;

    bool lbExpected = false;
    if (!gState.mbActiveRecordWritten.compare_exchange_strong(
            lbExpected, true, std::memory_order_relaxed))
        return;

    const std::uint64_t luNow = ::GetTickCount64();
    const std::uint64_t luFramesSincePass = luProducerFrames - luProducerFramesAtPass;
    char lacLine[2048] = {};
    std::snprintf(lacLine, sizeof(lacLine),
        "[rwac-occupancy] FIRST_PASS_WITNESS enabled=1 consumer_pass_seen=1 system=%p ring=%p "
        "capacity=%u cursor=%u highwater=%u system_frame=%u epoch=%llu "
        "producer_frames_for_trace_epoch=%llu pass_producer_frames_for_trace_epoch=%llu frames_since_pass=%llu "
        "callback_ends_process_global=%llu callback_age_ms_process_global=%llu "
        "engine_ends_process_global=%llu engine_age_ms_process_global=%llu "
        "command_pass_ends_process_global=%llu pass_age_ms_process_global=%llu "
        "mix_begin_process_global=%llu mix_end_process_global=%llu "
        "device_open_process_global=%u device_opens_process_global=%llu device_closes_process_global=%llu "
        "submits_process_global=%llu submit_errors_process_global=%llu last_submit_hr=0x%08X "
        "voice_errors_process_global=%llu critical_errors_process_global=%llu "
        "callback_registration_process_global=0x%08X\n",
        const_cast<void*>(lpSystem), const_cast<void*>(lpRing),
        luCapacity, luCursor, luHighWater, luSystemFrame,
        static_cast<unsigned long long>(luEpoch),
        static_cast<unsigned long long>(luProducerFrames),
        static_cast<unsigned long long>(luProducerFramesAtPass),
        static_cast<unsigned long long>(luFramesSincePass),
        static_cast<unsigned long long>(Count(DeviceCallbackEnd)),
        static_cast<unsigned long long>(Age(DeviceCallbackEnd, luNow)),
        static_cast<unsigned long long>(Count(EngineEnd)),
        static_cast<unsigned long long>(Age(EngineEnd, luNow)),
        static_cast<unsigned long long>(Count(CommandPassEnd)),
        static_cast<unsigned long long>(Age(CommandPassEnd, luNow)),
        static_cast<unsigned long long>(Count(MixBegin)),
        static_cast<unsigned long long>(Count(MixEnd)),
        gState.muDeviceOpen.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(Count(DeviceOpened)),
        static_cast<unsigned long long>(Count(DeviceClosed)),
        static_cast<unsigned long long>(Count(SubmitCount)),
        static_cast<unsigned long long>(Count(SubmitError)),
        gState.muLastSubmitError.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(Count(VoiceError)),
        static_cast<unsigned long long>(Count(CriticalError)),
        gState.muEngineCallbackRegistration.load(std::memory_order_relaxed));
    CgsDev::Log::WriteToLogEmergency(lacLine);
}

#if defined(BRN_RWAC_COMMAND_TRACE_FIXTURE)
// The fixture throws here so it can inspect the untouched buffer/cursor. This
// branch is absent from a normal diagnostic build. The production stop is fatal.
[[noreturn]] void FixtureStopBeforeWrite();
#endif

[[noreturn]] inline void StopBeforeWrite(const void* lpSystem, const void* lpRing,
    unsigned luPending, unsigned luCapacity, unsigned luRecordSize,
    unsigned luHighWater, unsigned luSystemFrame)
{
#if !defined(BRN_RWAC_COMMAND_TRACE_FIXTURE)
    if (gFailureRecorded.test_and_set(std::memory_order_relaxed))
    {
        ::TerminateProcess(::GetCurrentProcess(), 0xE0420001u);
        __fastfail(7);
    }
#endif
    const std::uint64_t luNow = ::GetTickCount64();
    const std::uint64_t luFrameTotal = Count(ProducerFrame);
    const std::uint64_t luEpochFrameStart =
        gState.muProducerFramesAtEpochStart.load(std::memory_order_relaxed);
    const std::uint64_t luFrames =
        luFrameTotal >= luEpochFrameStart ? luFrameTotal - luEpochFrameStart : 0;
    const std::uint64_t luLastPassFrame =
        gState.muProducerFramesAtLastPass.load(std::memory_order_relaxed);
    const std::uintptr_t luSystem = reinterpret_cast<std::uintptr_t>(lpSystem);
    const std::uintptr_t luProducerSystem =
        gState.muProducerSystem.load(std::memory_order_relaxed);
    const std::uintptr_t luEpochSystem =
        gState.muEpochSystem.load(std::memory_order_relaxed);
    const std::uintptr_t luPassSystem =
        gState.muPassSystem.load(std::memory_order_relaxed);
    const std::uint64_t luSystemEpoch =
        gState.muSystemEpoch.load(std::memory_order_relaxed);
    const std::uint64_t luProducerEpoch =
        gState.muProducerSystemEpoch.load(std::memory_order_relaxed);
    const std::uint64_t luPassEpoch =
        gState.muPassSystemEpoch.load(std::memory_order_relaxed);
    const bool lbFramesSincePassValid =
        luSystem != 0 && luSystem == luProducerSystem &&
        luSystem == luEpochSystem && luSystem == luPassSystem &&
        luSystemEpoch != 0 && luProducerEpoch == luSystemEpoch &&
        luPassEpoch == luSystemEpoch && luFrames >= luLastPassFrame;
    const std::uint64_t luFramesSincePass =
        lbFramesSincePassValid ? luFrames - luLastPassFrame : 0;
    // Fixed-size, single record, with stable scalar values only. Never print
    // damaged block names or inspect memory beyond the already-owned System.
    char lacLine[2048] = {};
    std::snprintf(lacLine, sizeof(lacLine),
        "[rwac-command-overflow] STOP_BEFORE_WRITE producer=SetAttribute "
        "system=%p ring=%p pending=%u capacity=%u record=%u highwater=%u system_frame=%u "
        "producer_frames=%llu last_pass_producer_frame=%llu "
        "callback=%llu/%llu callback_age_ms=%llu engine=%llu/%llu engine_age_ms=%llu "
        "command_lock=%llu pass=%llu/%llu pass_age_ms=%llu mix=%llu/%llu phase=%u "
        "engine_system=%p producer_system=%p producer_epoch=%llu pass_system=%p "
        "pass_epoch=%llu epoch_system=%p system_epoch=%llu frames_since_pass_valid=%u "
        "frames_since_pass=%llu device_open=%u opens=%llu closes=%llu "
        "submits=%llu submit_errors=%llu last_submit_hr=0x%08X "
        "voice_errors=%llu last_voice_hr=0x%08X critical_errors=%llu last_critical_hr=0x%08X "
        "engine_callback_registration=0x%08X tick_ms=%llu exit=0xE0420001\n",
        const_cast<void*>(lpSystem), const_cast<void*>(lpRing),
        luPending, luCapacity, luRecordSize, luHighWater, luSystemFrame,
        static_cast<unsigned long long>(luFrames),
        static_cast<unsigned long long>(luLastPassFrame),
        static_cast<unsigned long long>(Count(DeviceCallbackBegin)),
        static_cast<unsigned long long>(Count(DeviceCallbackEnd)),
        static_cast<unsigned long long>(Age(DeviceCallbackBegin, luNow)),
        static_cast<unsigned long long>(Count(EngineBegin)),
        static_cast<unsigned long long>(Count(EngineEnd)),
        static_cast<unsigned long long>(Age(EngineBegin, luNow)),
        static_cast<unsigned long long>(Count(CommandLockAcquired)),
        static_cast<unsigned long long>(Count(CommandPassBegin)),
        static_cast<unsigned long long>(Count(CommandPassEnd)),
        static_cast<unsigned long long>(Age(CommandPassEnd, luNow)),
        static_cast<unsigned long long>(Count(MixBegin)),
        static_cast<unsigned long long>(Count(MixEnd)),
        gState.muPhase.load(std::memory_order_relaxed),
        reinterpret_cast<void*>(gState.muEngineSystem.load(std::memory_order_relaxed)),
        reinterpret_cast<void*>(luProducerSystem),
        static_cast<unsigned long long>(luProducerEpoch),
        reinterpret_cast<void*>(luPassSystem),
        static_cast<unsigned long long>(luPassEpoch),
        reinterpret_cast<void*>(luEpochSystem),
        static_cast<unsigned long long>(luSystemEpoch),
        lbFramesSincePassValid ? 1u : 0u,
        static_cast<unsigned long long>(luFramesSincePass),
        gState.muDeviceOpen.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(Count(DeviceOpened)),
        static_cast<unsigned long long>(Count(DeviceClosed)),
        static_cast<unsigned long long>(Count(SubmitCount)),
        static_cast<unsigned long long>(Count(SubmitError)),
        gState.muLastSubmitError.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(Count(VoiceError)),
        gState.muLastVoiceError.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(Count(CriticalError)),
        gState.muLastCriticalError.load(std::memory_order_relaxed),
        gState.muEngineCallbackRegistration.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(luNow));
    CgsDev::Log::WriteToLogEmergency(lacLine);
#if defined(BRN_RWAC_COMMAND_TRACE_FIXTURE)
    FixtureStopBeforeWrite();
#else
    // A continuing assertion must never resume into the overflowing stores.
    // This is an explicitly labelled private diagnostic stop, not recovery.
    ::TerminateProcess(::GetCurrentProcess(), 0xE0420001u);
    __fastfail(7);
#endif
}

} } // namespace CgsSystem::RwacCommandTracePC
