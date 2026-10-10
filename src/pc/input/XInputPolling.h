#pragma once

#include <atomic>
#include <cstdint>

// FLAG PC-platform leaf: XInput can enumerate Windows devices when a user slot
// is empty. Microsoft recommends spacing out disconnected-slot probes instead
// of doing that on every frame. Connected pads retain the original poll rate.
// https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput
namespace CgsInput
{
    class PCDisconnectedPadPoll
    {
    public:
        bool ShouldPoll(std::uint64_t luNowMs)
        {
            // The window thread only publishes this request. The input thread
            // owns the connection state and consumes notifications here.
            if (mbProbeRequested.exchange(false, std::memory_order_relaxed))
                mbDisconnected = false;
            return !mbDisconnected || luNowMs - muLastDisconnectMs >= 1000u;
        }

        void RecordResult(unsigned long luResult, std::uint64_t luNowMs)
        {
            // Only ERROR_DEVICE_NOT_CONNECTED is cached. Other errors retain
            // the original retry behaviour, and success is never rate limited.
            mbDisconnected = luResult == 1167u;
            if (mbDisconnected)
                muLastDisconnectMs = luNowMs;
        }

        void RequestProbe()
        {
            mbProbeRequested.store(true, std::memory_order_relaxed);
        }

    private:
        std::atomic<bool> mbProbeRequested{false};
        bool mbDisconnected = false;
        std::uint64_t muLastDisconnectMs = 0;
    };

    // Both PC input consumers read XInput user zero. Share its absence cache;
    // they still take fresh samples independently whenever a pad is connected.
    inline PCDisconnectedPadPoll gPCDisconnectedPadPoll;
}
