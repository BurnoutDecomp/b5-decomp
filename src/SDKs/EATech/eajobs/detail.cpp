#include "SDKs/EATech/eajobs/detail.h"

#include "SDKs/EATech/eajobs/jobs.h"             // EA::Jobs::TicksToSeconds
#include "SDKs/EATech/eathread/BrnEAThreadX360.h" // EA::Thread::ThreadSleep

#include <windows.h> // QueryPerformanceCounter, LARGE_INTEGER

// ============================================================================
// SDKs/EATech/eajobs/detail.cpp
//
// EA::Jobs::Detail::WaitOnYieldHelper @ 0x82BC9B60, reconstructed store-for-store
// from the X360 .XEX.
//
// Control flow (asm-authoritative):
//   r30 = 2                                  ; default "callback verdict" sentinel
//   if (pCallback) {                         ; cmplwi r11,0 / beq
//       r30 = pCallback(iContext);           ; mtctr/bctrl ; mr. r30,r3
//       if (r30 == 0) return 0;              ; bne skips the early-out
//   }
//   if (lSleepMs >= 0)                        ; cmpwi r29,0 / blt
//       EA::Thread::ThreadSleep(&lSleepMs);  ; stores the ms count, passes its addr
//   // (r30==2 padding nop block is alignment only)
//   if (*pbDone) return 1;                    ; lbz r11,0(r27) / bne -> li r3,1
//   QueryPerformanceCounter(&counter);
//   seconds = EA::Jobs::TicksToSeconds(counter.QuadPart - iStartTicks);
//   if (seconds <= 8.0f) return 1;            ; flt_82004C88 == 8.0 / fcmpu / ble
//   if (!gpWaitWatchdog) return 1;            ; dword_8327F284 == 0 -> li r3,1
//   return (gpWaitWatchdog() & 0xFF) != 0 ? 1 : 0; ; clrlwi. r11,r3,24
//
// The X360 reads the 64-bit QPC value (`ld`) and subtracts iStartTicks before the
// (single-precision) tick->seconds conversion.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
namespace Detail
{
    // dword_8327F284 -- an optional process-wide watchdog predicate. When installed
    // it has the final say once the elapsed wait exceeds the 8-second budget:
    // returning nonzero keeps the caller spinning, returning 0 gives up. Null by
    // default (then a long wait simply keeps spinning).
    typedef bool (*WaitWatchdog)();
    static WaitWatchdog spWaitWatchdog = 0;

    // The X360 elapsed-wait budget before the watchdog is consulted (flt_82004C88).
    static const f32 KF_WAIT_BUDGET_SECONDS = 8.0f;

    // @ 0x82BC9B60
    int WaitOnYieldHelper(WaitOnYieldCallback pCallback,
                          int                 iContext,
                          s32                 lSleepMs,
                          int                 iStartTicks,
                          const u8*           pbDone)
    {
        if (pCallback)
        {
            // If the user predicate says stop (returns 0), bail immediately.
            if (pCallback(iContext) == 0)
                return 0;
        }

        if (lSleepMs >= 0)
        {
            // ThreadSleep takes a pointer to a millisecond count (X360 facade).
            u32 luSleepMs = static_cast<u32>(lSleepMs);
            EA::Thread::ThreadSleep(&luSleepMs);
        }

        // Done flag already set -> keep waiting (the caller's loop will observe it).
        if (*pbDone)
            return 1;

        LARGE_INTEGER lCounter;
        QueryPerformanceCounter(&lCounter);
        // counter - start (64-bit `subf` in the asm; iStartTicks is sign-extended
        // into the GPR), then ticks -> seconds (single precision, as on X360).
        u64 luElapsedTicks =
            static_cast<u64>(lCounter.QuadPart) - static_cast<u64>(static_cast<s64>(iStartTicks));
        f32 lfSeconds = EA::Jobs::TicksToSeconds(luElapsedTicks);

        // Within budget -> keep waiting.
        if (lfSeconds <= KF_WAIT_BUDGET_SECONDS)
            return 1;

        // Over budget: no watchdog -> keep waiting; else the watchdog decides.
        if (!spWaitWatchdog)
            return 1;

        return (spWaitWatchdog() != 0) ? 1 : 0;
    }
}
}
}
