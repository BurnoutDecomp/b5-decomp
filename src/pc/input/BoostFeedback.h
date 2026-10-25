#pragma once

#include "pc/input/EngineFeedback.h"

// FLAG PC-platform leaf: semantic boost-award observation. AddBoost records an
// award only when its allowed, clamped operation actually adds boost. The world
// module binds its local-player strategy; meter changes are never polled here.
namespace CgsInput::AdaptiveTriggers::BoostFeedback
{
    class Recorder
    {
    public:
        void Record(std::uintptr_t luCookie, std::uint64_t luNow)
        {
            Entry* lpEntry = nullptr;
            for (auto& lrEntry : maEntries) if (lrEntry.muCookie == luCookie) { lpEntry = &lrEntry; break; }
            if (!lpEntry) lpEntry = &maEntries[muNext++ % maEntries.size()];
            *lpEntry = {luCookie, ++muSerial, luNow};
        }
        void BindPlayer(std::uintptr_t luCookie)
        {
            if (luCookie != muPlayerCookie) { muPlayerCookie = luCookie; muBaseline = muSerial; }
        }
        std::uint64_t GetSerial(std::uint64_t luNow) const
        {
            for (const auto& lrEntry : maEntries)
                if (lrEntry.muCookie == muPlayerCookie && lrEntry.muSerial > muBaseline &&
                    luNow >= lrEntry.muUpdated && luNow - lrEntry.muUpdated <= 500) return lrEntry.muSerial;
            return 0;
        }
    private:
        struct Entry { std::uintptr_t muCookie = 0; std::uint64_t muSerial = 0, muUpdated = 0; };
        std::array<Entry, 16> maEntries{};
        std::uintptr_t muPlayerCookie = 0;
        std::size_t muNext = 0;
        std::uint64_t muSerial = 0, muBaseline = 0;
    };
    inline std::mutex gMutex;
    inline Recorder gRecorder;
    inline void Reset()
    {
        const std::lock_guard<std::mutex> lLock(gMutex);
        gRecorder = {};
    }
    inline void Record(std::uintptr_t luCookie)
    {
        if (!EngineFeedback::gbEnabled.load(std::memory_order_relaxed)) return;
        const std::lock_guard<std::mutex> lLock(gMutex);
        gRecorder.Record(luCookie, EngineFeedback::Now());
    }
    inline void BindPlayer(std::uintptr_t luCookie)
    {
        if (!EngineFeedback::gbEnabled.load(std::memory_order_relaxed)) return;
        const std::lock_guard<std::mutex> lLock(gMutex);
        gRecorder.BindPlayer(luCookie);
    }
    inline std::uint64_t GetSerial()
    {
        if (!EngineFeedback::gbEnabled.load(std::memory_order_relaxed)) return 0;
        const std::lock_guard<std::mutex> lLock(gMutex);
        return gRecorder.GetSerial(EngineFeedback::Now());
    }
}
