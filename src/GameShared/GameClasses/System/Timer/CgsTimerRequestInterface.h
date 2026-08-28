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

        // ⭐ Clear -- HEADER-INLINE ON THE CONSOLE, and BODIED HERE 2026-08-28 (crash-slomo
        // transport wave). It has NO X360 export at all (grep the identity table: every other
        // TimerRequests method does, this one does not), because every caller inlines it. The
        // shape is pinned store-for-store by BrnGameModule::BridgeTimers @0x823BD150, which
        // inlines it TWICE over the game module's own mTimerRequestInterface:
        //     0x823BD178  lfs  f0, flt_82001C98      ; 1.0f
        //     0x823BD16C  li   r10, 0
        //     0x823BD198  stfs f0,  4(r11)           ; mGameTimer.mfMultiplier = 1.0f
        //     0x823BD19C  stw  r10, 0(r11)           ; mGameTimer.muFlags      = 0
        //     0x823BD1A0  stfs f0, 0xC(r11)          ; mSimTimer.mfMultiplier  = 1.0f
        //     0x823BD1A4  stw  r10, 8(r11)           ; mSimTimer.muFlags       = 0
        // ⚠️ THE IDENTITY MULTIPLIER IS 1.0f, NOT 0.0f. A zeroed mfMultiplier here would be the
        // project's placeholder-identity trap in its purest form -- ApplyToTimers would drive
        // the sim timer's scale target to zero and freeze the world -- so the reset value is
        // taken from the console's own store, not from "clear means zero".
        void Clear()
        {
            mfMultiplier = 1.0f;   // store order as emitted: the float first, then the flags
            muFlags      = 0u;
        }

        // DWARF h:53-77 -- their own ledger functions (declaration-only here).
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
        // DWARF h:99. ⭐ BODIED 2026-08-28 as a header inline, same evidence as
        // TimerRequests::Clear above: no X360 export exists, and BridgeTimers @0x823BD150
        // inlines exactly this pair of member Clears over the whole 16-byte interface.
        // BrnGameModule::Construct @0x823C9EA8 is its other caller ("Clears the interface").
        void Clear()
        {
            mGameTimer.Clear();
            mSimTimer.Clear();
        }

        // DWARF h:100-111. [x] BODIED 2026-08-26 (stuntrace waveB CLOSURE round) in this TU's
        // .cpp -- all four, from the `addi r3, r3, 8` the X360 emits where it inlines the sim
        // accessor (ModeManager::FinishCurrentMode @0x8234BB80). GetSimTimerRequests was one of
        // the wave's unresolved externals (BrnModeManager_Finish.cpp:460); the game-timer twin is
        // bodied alongside it so the symmetric set cannot go half-linked.
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
