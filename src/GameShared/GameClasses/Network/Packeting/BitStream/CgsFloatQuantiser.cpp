#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (Hex-Rays "local variable allocation
// failed" here, so this is decoded from the ASSEMBLY, not the pseudocode).
//   CgsNetwork::FloatQuantiser::Pack -- the forward direction; see its own banner below.
//   CgsNetwork::FloatQuantiser::UnPack @ 0x82882B40 -- reconstruct a bounded float
//     from its packed increment across [min,max] with (1<<numBits)-1 divisions:
//       value = min + packed * (max - min) / ((1 << numBits) - 1)
//     The asm asserts packed < (1<<numBits) (CgsFloatQuantiser.cpp:116), then that the
//     reconstructed value sits within ONE increment of [min,max] (streams the original
//     "Value <v> is outside the range <min>:<max>", :125), then clamps with four fsels
//     -- into [min-inc, max+inc] and then into [min, max] (net [min,max]).
//   CgsNetwork::FloatQuantiser::GetIncrement -- (max-min)/((1<<numBits)-1); the per-step
//     size. The X360 INLINES it into UnPack (fdivs f26, (max-min), (float)((1<<numBits)-1)
//     @0x82882BE4); extracted here per its private declaration.

namespace CgsNetwork
{
    // ---- GetIncrement ----------------------------------------------------------
    // Float step per packed unit across [min,max]. Divisor is the UNSIGNED
    // (1<<numBits)-1 (the asm's f12 path: clrldi + fcfid).
    float FloatQuantiser::GetIncrement(float lfMin, float lfMax, s32 liNumBits)
    {
        const u32 luDivisor = (1u << liNumBits) - 1u;
        return (lfMax - lfMin) / static_cast<float>(luDivisor);
    }

    // ---- Pack ------------------------------------------------------------------
    // The inverse of UnPack: map a value in [lfMin, lfMax] onto an integer increment
    // across ((1 << liNumBits) - 1) divisions. Decoded from the assembly; the ORDER
    // below is the assembly's order, which is not the order UnPack would suggest:
    //   1. assert the value is already inside [min, max] (streams
    //      "Value <v> is outside the range <min>:<max>");
    //   2. clamp anyway, with two fsels, LOWER bound first, then upper -- so a value
    //      that failed the assert is still packed, not left to overflow;
    //   3. normalise (clamped - min) / (max - min);
    //   4. scale by the SIGNED (1 << numBits) - 1 (the asm's extsw + fcfid + frsp
    //      path, i.e. the divisor is converted through a 64-bit signed integer and
    //      rounded to single) -- the same signed divisor UnPack uses for the value,
    //      NOT GetIncrement's unsigned one;
    //   5. round by ADDING 0.5 and truncating TOWARD ZERO (fmadds then fctidz), which
    //      is round-half-up for the non-negative operand this always is, not the
    //      round-half-to-even a plain cast-with-rint would give;
    //   6. store the packed value UNCONDITIONALLY, and only then assert it fits in
    //      liNumBits.
    // Step 5's multiply-add is fused on the console and is not here; that is a
    // sub-ULP difference in the last quantisation step, and the assert in step 6
    // bounds it.
    void FloatQuantiser::Pack(float lfValue, float lfMin, float lfMax, s32 liNumBits,
                              u32* lpuPackedValue)
    {
        CGS_ASSERT(lfValue >= lfMin && lfValue <= lfMax,
                   "Value  is outside the range :");

        // Two fsels, lower bound first.
        float lfClamped = lfValue;
        if (lfClamped < lfMin) lfClamped = lfMin;
        if (lfClamped > lfMax) lfClamped = lfMax;

        const s32 liRange   = 1 << liNumBits;   // 2^numBits (asm r11 = slw 1, numBits)
        const s32 liDivisor = liRange - 1;      // signed, via extsw -> fcfid -> frsp

        const float fNormalised = (lfClamped - lfMin) / (lfMax - lfMin);
        const float fScaled     = fNormalised * static_cast<float>(liDivisor) + 0.5f;
        const u32   luPacked    = static_cast<u32>(SaturateToS64(fScaled));

        *lpuPackedValue = luPacked;

        CGS_ASSERT(luPacked < static_cast<u32>(liRange),
                   "*lpuPackedValue < static_cast<uint32_t>(1 << liNumBitsUsed)");
    }

    // ---- UnPack @ 0x82882B40 ---------------------------------------------------
    void FloatQuantiser::UnPack(float* lpfValue, float lfMin, float lfMax, s32 liNumBits,
                                u32 luPackedValue)
    {
        const s32 liRange = 1 << liNumBits;   // 2^numBits (asm r31 = slw 1, numBits)

        // The packed increment must fit in numBits.
        CGS_ASSERT(static_cast<s32>(luPackedValue) < liRange,
                   "(int32_t)luPackedValue < (1 << liNumBitsUsed)");

        // increment = (max-min)/((1<<numBits)-1); value = min + packed*(max-min)/divisor.
        // The value divisor is the SIGNED (1<<numBits)-1 (asm f11: extsw + fcfid); for
        // numBits < 31 it equals GetIncrement's unsigned divisor.
        const float fIncrement = GetIncrement(lfMin, lfMax, liNumBits);
        const float fValue = static_cast<float>(luPackedValue) * (lfMax - lfMin)
                                 / static_cast<float>(liRange - 1) + lfMin;

        // The reconstructed value must sit within one increment of the range (the X360
        // streams "Value <v> is outside the range <min>:<max>" and fires the assert).
        CGS_ASSERT(fValue >= (lfMin - fIncrement) && fValue <= (lfMax + fIncrement),
                   "fValue is outside the range [lfMin - increment, lfMax + increment]");

        // Four-fsel clamp: into [min-inc, max+inc], then into [min, max].
        float fClamped = fValue;
        if (fClamped < (lfMin - fIncrement)) fClamped = (lfMin - fIncrement);
        if (fClamped > (lfMax + fIncrement)) fClamped = (lfMax + fIncrement);
        if (fClamped < lfMin) fClamped = lfMin;
        if (fClamped > lfMax) fClamped = lfMax;
        *lpfValue = fClamped;
    }

    // ---- Pack (resolution form) --------------------------------------------------
    // Assert the value is inside [min, max], clamp it anyway (two fsels, lower bound
    // first), then count whole steps of 2 * resolution from min, rounding half up. The bit
    // count is the smallest n with (1 << n) > the rounded step count of the whole range;
    // the range quotient is rounded in double precision, the value's in single. The
    // packed value is stored before the fits-in-n-bits assert.
    void FloatQuantiser::Pack(float lfValue, float lfMin, float lfMax, float lfResolution,
                              u32* lpuPackedValue, s32* lpiNumBitsUsed)
    {
        CGS_ASSERT(!(lfValue < lfMin) && lfValue <= lfMax,
                   "Value  is outside the range :");

        const float lfIncrement = lfResolution * 2.0f;

        float lfClamped = ((lfMin - lfValue) >= 0.0f) ? lfMin : lfValue;
        lfClamped       = ((lfMax - lfClamped) >= 0.0f) ? lfClamped : lfMax;

        CGS_ASSERT(lfIncrement > 0.0f, "lfIncrement > 0.0f");

        const float  lfSteps    = (lfClamped - lfMin) / lfIncrement + 0.5f;
        const double ldMaxSteps = static_cast<double>((lfMax - lfMin) / lfIncrement) + 0.5;

        // fctidz + stfiwx: the low word of the saturated 64-bit step count.
        *lpuPackedValue = static_cast<u32>(SaturateToS64(lfSteps));

        // fctiwz saturates, so an unbounded range (max FLT_MAX) counts 0x7FFFFFFF steps and
        // the bit loop below ends at 31 bits.
        const u32 luNumValues = static_cast<u32>(SaturateToS32(ldMaxSteps)) + 1u;
        s32 liNumBits = 0;
        if (luNumValues != 0)
        {
            do
            {
                ++liNumBits;
            }
            while ((1u << liNumBits) < luNumValues);
        }
        *lpiNumBitsUsed = liNumBits;

        CGS_ASSERT(*lpuPackedValue < (1u << liNumBits),
                   "*lpuPackedValue < static_cast<uint32_t>( 1 << *lpiNumBitsUsed )");
    }

    // ---- UnPack (resolution form) ------------------------------------------------
    // value = min + packed * (2 * resolution) (one fused multiply-add on the console),
    // asserted inside [min, max], then clamped: two fsels, then the explicit bound checks.
    void FloatQuantiser::UnPack(float* lpfValue, float lfMin, float lfMax, float lfResolution,
                                u32 luPackedValue)
    {
        const float lfValue = (lfResolution * 2.0f) * static_cast<float>(luPackedValue) + lfMin;

        CGS_ASSERT(!(lfValue < lfMin) && lfValue <= lfMax,
                   "Value  is outside the range :");

        float lfClamped = ((lfMin - lfValue) >= 0.0f) ? lfMin : lfValue;
        lfClamped       = ((lfMax - lfClamped) >= 0.0f) ? lfClamped : lfMax;

        if (lfClamped < lfMin)
        {
            *lpfValue = lfMin;
        }
        else if (lfClamped > lfMax)
        {
            *lpfValue = lfMax;
        }
        else
        {
            *lpfValue = lfClamped;
        }
    }
}
