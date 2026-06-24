#include "types.hpp"
#include "GameSource/Effects/ActiveRaceCarData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"     // CgsNumeric::Random

// =============================================================================
// BrnEffects::BurstAccumulator (X360 ARTIST)
//   Construct @ 0x82278530   Update @ 0x8227EC90
// =============================================================================

namespace BrnEffects
{

// ---- Construct @ 0x82278530 ----------------------------------------------
// Asserts (then stores) per the X360 body: lfMaxBurstSize > lfMinBurstSize and
// lfBurstTimeout > 0.0f. Both fields fall through to the stores regardless of the
// assert outcome (the assert is advisory). Stores: min, max, timeout, then the
// next-threshold and accumulator both primed to lfMinBurstSize and the
// next-burst time to 0.0f.
void BurstAccumulator::Construct(f32 lfMinBurstSize, f32 lfMaxBurstSize, f32 lfBurstTimeout)
{
    CGS_ASSERT(lfMaxBurstSize > lfMinBurstSize, "lfMaxBurstSize > lfMinBurstSize");
    CGS_ASSERT(lfBurstTimeout > 0.0f, "lfBurstTimeout > 0.0f");

    mfMinBurstSize  = lfMinBurstSize;
    mfMaxBurstSize  = lfMaxBurstSize;
    mfBurstTimeout  = lfBurstTimeout;
    mfNextThreshold = lfMinBurstSize;
    mfNextBurstTime = 0.0f;
    mfAccumulator   = lfMinBurstSize;
}

// ---- Update @ 0x8227EC90 -------------------------------------------------
// Accumulate lfDelta; reset the accumulator to mfMaxBurstSize once lfTime reaches
// the scheduled next-burst time. Below the current threshold -> emit nothing.
// Otherwise draw a fresh rand-in-[0,1) (the X360 inlines CgsNumeric::Random's
// per-call draw: read the oldest ring slot, refill it from the advanced LCG seed,
// and return slot-1.0f), shape the next threshold as
//   mfNextThreshold = (mfMaxBurstSize - mfMinBurstSize) * r * r + mfMinBurstSize,
// schedule the next-burst time at (mfBurstTimeout + lfTime), emit the truncated
// accumulator as the burst count and carry the fractional remainder forward.
// liArg3 / liArg4 are unreferenced by the X360 body.
s32 BurstAccumulator::Update(f32 lfDelta, f32 lfTime, s32 liArg3, s32 liArg4, CgsNumeric::Random* lpRandom)
{
    (void)liArg3;
    (void)liArg4;

    mfAccumulator = lfDelta + mfAccumulator;
    if (lfTime >= mfNextBurstTime)
    {
        mfAccumulator = mfMaxBurstSize;
    }

    if (mfAccumulator < mfNextThreshold)
    {
        return 0;
    }

    const f32 lfRand01 = lpRandom->RandomFloat();
    const f32 lfRange  = mfMaxBurstSize - mfMinBurstSize;

    mfNextBurstTime = mfBurstTimeout + lfTime;

    const s32 liBurstCount = static_cast<s32>(mfAccumulator);
    mfNextThreshold = lfRange * lfRand01 * lfRand01 + mfMinBurstSize;
    mfAccumulator   = mfAccumulator - static_cast<f32>(liBurstCount);

    return liBurstCount;
}

} // namespace BrnEffects
