#pragma once

// FLAG PC-platform leaf: private diagnostic for a deferred audio-command overwrite.
// Fixed atomic storage only; callbacks never log, allocate or wait. The failure
// record is emitted by the observed SetAttribute producer before changing memory.
#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <intrin.h>

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
    std::atomic<std::uint64_t> muProducerFramesAtLastPass{0};
    std::atomic<unsigned> muPhase{0};
    std::atomic<unsigned> muDeviceOpen{0};
    std::atomic<unsigned> muLastSubmitError{0};
    std::atomic<unsigned> muLastVoiceError{0};
    std::atomic<unsigned> muLastCriticalError{0};
    std::atomic<unsigned> muEngineCallbackRegistration{0};
};

// One shared COMDAT instance across the diagnostic executable's C++17 TUs.
inline State gState;

inline void Mark(Event leEvent)
{
    gState.maEvent[leEvent].mTick.store(::GetTickCount64(), std::memory_order_relaxed);
    gState.maEvent[leEvent].mCount.fetch_add(1, std::memory_order_relaxed);
}

inline void MarkProducerFrame(const void* lpSystem)
{
    gState.muProducerSystem.store(reinterpret_cast<std::uintptr_t>(lpSystem),
                                  std::memory_order_relaxed);
    Mark(ProducerFrame);
}

inline void MarkEngineBegin(const void* lpSystem)
{
    gState.muEngineSystem.store(reinterpret_cast<std::uintptr_t>(lpSystem),
                                std::memory_order_relaxed);
    gState.muPhase.store(1, std::memory_order_relaxed);
    Mark(EngineBegin);
}

inline void MarkConsumerEvent(Event leEvent, unsigned luPhase)
{
    gState.muPhase.store(luPhase, std::memory_order_relaxed);
    if (leEvent == CommandPassEnd)
        gState.muProducerFramesAtLastPass.store(
            gState.maEvent[ProducerFrame].mCount.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
    Mark(leEvent);
}

inline void MarkSubmit(HRESULT lResult)
{
    Mark(SubmitCount);
    if (FAILED(lResult))
    {
        gState.muLastSubmitError.store(static_cast<unsigned>(lResult), std::memory_order_relaxed);
        Mark(SubmitError);
    }
}

inline void MarkVoiceError(HRESULT lResult)
{
    gState.muLastVoiceError.store(static_cast<unsigned>(lResult), std::memory_order_relaxed);
    Mark(VoiceError);
}

inline void MarkCriticalError(HRESULT lResult)
{
    gState.muLastCriticalError.store(static_cast<unsigned>(lResult), std::memory_order_relaxed);
    Mark(CriticalError);
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

#if defined(BRN_RWAC_COMMAND_TRACE_FIXTURE)
// The fixture throws here so it can inspect the untouched buffer/cursor. This
// branch is absent from a normal diagnostic build. The production stop is fatal.
[[noreturn]] void FixtureStopBeforeWrite();
#endif

[[noreturn]] inline void StopBeforeWrite(const void* lpSystem, const void* lpRing,
    unsigned luPending, unsigned luCapacity, unsigned luRecordSize,
    unsigned luHighWater, unsigned luSystemFrame)
{
    const std::uint64_t luNow = ::GetTickCount64();
    const std::uint64_t luFrames = Count(ProducerFrame);
    const std::uint64_t luLastPassFrame =
        gState.muProducerFramesAtLastPass.load(std::memory_order_relaxed);
    // Fixed-size, single record, with stable scalar values only. Never print
    // damaged block names or inspect memory beyond the already-owned System.
    char lacLine[2048] = {};
    std::snprintf(lacLine, sizeof(lacLine),
        "[rwac-command-overflow] STOP_BEFORE_WRITE producer=SetAttribute "
        "system=%p ring=%p pending=%u capacity=%u record=%u highwater=%u system_frame=%u "
        "producer_frames=%llu last_pass_producer_frame=%llu frames_since_pass=%llu "
        "callback=%llu/%llu callback_age_ms=%llu engine=%llu/%llu engine_age_ms=%llu "
        "command_lock=%llu pass=%llu/%llu pass_age_ms=%llu mix=%llu/%llu phase=%u "
        "engine_system=%p producer_system=%p device_open=%u opens=%llu closes=%llu "
        "submits=%llu submit_errors=%llu last_submit_hr=0x%08X "
        "voice_errors=%llu last_voice_hr=0x%08X critical_errors=%llu last_critical_hr=0x%08X "
        "engine_callback_registration=0x%08X tick_ms=%llu exit=0xE0420001\n",
        lpSystem, lpRing, luPending, luCapacity, luRecordSize, luHighWater, luSystemFrame,
        static_cast<unsigned long long>(luFrames),
        static_cast<unsigned long long>(luLastPassFrame),
        static_cast<unsigned long long>(luFrames >= luLastPassFrame ? luFrames - luLastPassFrame : 0),
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
        reinterpret_cast<const void*>(gState.muEngineSystem.load(std::memory_order_relaxed)),
        reinterpret_cast<const void*>(gState.muProducerSystem.load(std::memory_order_relaxed)),
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
