#ifndef BRN_ACTIVE_RACE_CAR_DATA_H
#define BRN_ACTIVE_RACE_CAR_DATA_H

#include "types.hpp"

namespace CgsNumeric { class Random; }

// =============================================================================
// ActiveRaceCarData.h  (home for BrnEffects::BurstAccumulator)
//
// BurstAccumulator drives the spark/debris burst timing for an active race car.
// It accumulates contact "energy" over time and, once a randomised threshold is
// crossed, emits a whole-number burst count and carries the fractional remainder
// forward. The randomised next threshold is shaped (rand^2) so small bursts are
// more common than large ones.
//
// Layout proven by Construct @ 0x82278530 and Update @ 0x8227EC90 (six f32, no
// padding -> sizeof 0x18). Construct's asserts cite
//   "..\\..\\..\\GameSource\\Effects/ActiveRaceCarData.h" lines 51 ("lfMaxBurstSize
//   > lfMinBurstSize") and 52 ("lfBurstTimeout > 0.0f").
// Members accessed by name only.
// =============================================================================

namespace BrnEffects
{
    class BurstAccumulator
    {
    public:
        // Construct @ 0x82278530. Installs the configured min/max burst sizes and
        // the burst timeout, primes the next-threshold to lfMinBurstSize and the
        // accumulator to lfMinBurstSize, and zeroes the next-burst time.
        void Construct(f32 lfMinBurstSize, f32 lfMaxBurstSize, f32 lfBurstTimeout);

        // Update @ 0x8227EC90. Adds lfDelta to the accumulator; if lfTime has
        // reached the next-burst time the accumulator is reset to mfMaxBurstSize.
        // While the accumulator is below the current threshold returns 0; once the
        // threshold is crossed it draws a fresh randomised threshold, schedules the
        // next-burst time at (mfBurstTimeout + lfTime), and returns the whole-number
        // burst count (truncated accumulator), carrying the remainder forward.
        // The X360 leaves the integer arguments after lfTime unused.
        s32 Update(f32 lfDelta, f32 lfTime, s32 liArg3, s32 liArg4, CgsNumeric::Random* lpRandom);

    private:
        f32 mfMinBurstSize;       // +0x00
        f32 mfMaxBurstSize;       // +0x04
        f32 mfBurstTimeout;       // +0x08
        f32 mfNextThreshold;      // +0x0C  (randomised burst threshold)
        f32 mfNextBurstTime;      // +0x10  (next allowed burst time)
        f32 mfAccumulator;        // +0x14
    };

    static_assert(sizeof(BurstAccumulator) == 0x18, "BurstAccumulator layout drift");
}

#endif
