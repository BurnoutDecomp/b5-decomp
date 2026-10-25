#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

// FLAG PC-platform leaf: bounded sidecar for the real engine RPM-cap operation.
// Engine cookies are integer identities only; no physics pointer is retained or
// dereferenced. The local-player binding is published by WriteOutVehicleStats.
namespace CgsInput::AdaptiveTriggers::EngineFeedback
{
    class Recorder
    {
    public:
        void Record(std::uintptr_t luCookie, bool lbLimited, std::uint64_t luNow)
        {
            Entry* lpEntry = nullptr;
            for (auto& lrEntry : maEntries) if (lrEntry.muCookie == luCookie) { lpEntry = &lrEntry; break; }
            if (!lpEntry) lpEntry = &maEntries[muNext++ % maEntries.size()];
            *lpEntry = {luCookie, luNow, lbLimited, 0};
        }
        void RecordDrive(std::uintptr_t luCookie, unsigned luMask)
        {
            for (auto& lrEntry : maEntries)
                if (lrEntry.muCookie == luCookie) { lrEntry.muDrivenMask = luMask & 0xf; return; }
        }
        void BindPlayer(std::uintptr_t luCookie, std::uint32_t luEntity)
        {
            muPlayerCookie = luCookie;
            muPlayerEntity = luEntity;
        }
        bool IsLimited(std::uint32_t luEntity, std::uint64_t luNow) const
        {
            if (luEntity != muPlayerEntity || !muPlayerCookie) return false;
            for (const auto& lrEntry : maEntries)
                if (lrEntry.muCookie == muPlayerCookie)
                    return lrEntry.mbLimited && luNow >= lrEntry.muUpdated && luNow - lrEntry.muUpdated <= 500;
            return false;
        }
        unsigned GetDrivenMask(std::uint32_t luEntity, std::uint64_t luNow) const
        {
            if (luEntity != muPlayerEntity || !muPlayerCookie) return 0;
            for (const auto& lrEntry : maEntries)
                if (lrEntry.muCookie == muPlayerCookie && luNow >= lrEntry.muUpdated && luNow - lrEntry.muUpdated <= 500)
                    return lrEntry.muDrivenMask;
            return 0;
        }
    private:
        struct Entry { std::uintptr_t muCookie = 0; std::uint64_t muUpdated = 0; bool mbLimited = false; unsigned muDrivenMask = 0; };
        std::array<Entry, 64> maEntries{};
        std::size_t muNext = 0;
        std::uintptr_t muPlayerCookie = 0;
        std::uint32_t muPlayerEntity = ~0u;
    };

    inline std::atomic<bool> gbEnabled{false};
    inline std::mutex gMutex;
    inline Recorder gRecorder;

    inline std::uint64_t Now()
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    inline void SetEnabled(bool lbEnabled)
    {
        const std::lock_guard<std::mutex> lLock(gMutex);
        if (gbEnabled.load(std::memory_order_relaxed) != lbEnabled) gRecorder = {};
        gbEnabled.store(lbEnabled, std::memory_order_relaxed);
    }
    inline void Record(std::uintptr_t luCookie, bool lbLimited)
    {
        if (!gbEnabled.load(std::memory_order_relaxed)) return;
        const std::lock_guard<std::mutex> lLock(gMutex);
        gRecorder.Record(luCookie, lbLimited, Now());
    }
    inline void BindPlayer(std::uintptr_t luCookie, std::uint32_t luEntity)
    {
        if (!gbEnabled.load(std::memory_order_relaxed)) return;
        const std::lock_guard<std::mutex> lLock(gMutex);
        gRecorder.BindPlayer(luCookie, luEntity);
    }
    inline void RecordDrive(std::uintptr_t luCookie, float lfFront, float lfRear, bool lbRearOnly)
    {
        if (!gbEnabled.load(std::memory_order_relaxed)) return;
        const unsigned luMask = lbRearOnly ? 0xcu : ((lfFront != 0 ? 3u : 0u) | (lfRear != 0 ? 0xcu : 0u));
        const std::lock_guard<std::mutex> lLock(gMutex);
        gRecorder.RecordDrive(luCookie, luMask);
    }
    inline unsigned GetDrivenMask(std::uint32_t luEntity)
    {
        if (!gbEnabled.load(std::memory_order_relaxed)) return 0;
        const std::lock_guard<std::mutex> lLock(gMutex);
        return gRecorder.GetDrivenMask(luEntity, Now());
    }
    inline bool IsLimited(std::uint32_t luEntity)
    {
        if (!gbEnabled.load(std::memory_order_relaxed)) return false;
        const std::lock_guard<std::mutex> lLock(gMutex);
        return gRecorder.IsLimited(luEntity, Now());
    }
}
