#pragma once

#include "types.hpp"

// CgsSystem::TimerRequests / TimerRequestInterface - the per-frame start/stop/
// retime requests consumers post against the game and sim timers; ApplyToTimers
// folds them onto the two CgsSystem::Timer instances. Class shapes / member
// names / method sets verbatim from the DecFIGS DWARF
// (CgsTimerRequestInterface.h:42/:95); gated on the X360 ledger. This TU bodies
// ApplyToTimers; the rest of the surface is its own ledger functions
// (declaration-only), except the trivial flag tests the X360 header-inlines
// (the bit masks are pinned by the ApplyToTimers asm @0x828D7468: start=bit 0,
// stop=bit 1, multiplier=bit 2).
namespace CgsSystem
{
    class Timer;

    struct TimerRequests
    {
        // The muFlags request bits (ApplyToTimers @0x828D7468).
        static const u32 KU_FLAG_START      = 1u << 0;
        static const u32 KU_FLAG_STOP       = 1u << 1;
        static const u32 KU_FLAG_MULTIPLIER = 1u << 2;

        // DWARF h:53-77 -- their own ledger functions (declaration-only here).
        void Clear();
        void Start();
        void Stop();
        void SetTimestepMultiplier(f32 lfMultiplier);
        f32  GetMultiplier() const;
        void Append(const TimerRequests& lrOther);

        // Header-inline on the X360 (the ApplyToTimers bit tests).
        bool IsStartRequested() const      { return (muFlags & KU_FLAG_START) != 0; }
        bool IsStopRequested() const       { return (muFlags & KU_FLAG_STOP) != 0; }
        bool IsMultiplierRequested() const { return (muFlags & KU_FLAG_MULTIPLIER) != 0; }

    private:
        u32 muFlags;       // +0x00 (DWARF h:81)
        f32 mfMultiplier;  // +0x04 (DWARF h:82)
    };

    struct TimerRequestInterface
    {
        // DWARF h:99-111 -- their own ledger functions (declaration-only here).
        void Clear();
        const TimerRequests* GetGameTimerRequests() const;
        TimerRequests*       GetGameTimerRequests();
        const TimerRequests* GetSimTimerRequests() const;
        TimerRequests*       GetSimTimerRequests();

        // @0x828D7468 (this TU) -- fold the queued requests onto the two timers.
        void ApplyToTimers(Timer* lpGameTimer, Timer* lpSimTimer) const;   // const per DWARF

    private:
        TimerRequests mGameTimer;   // +0x00 (DWARF h:120)
        TimerRequests mSimTimer;    // +0x08 (DWARF h:121)
    };
}
