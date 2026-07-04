#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsFloatQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (Hex-Rays "local variable allocation
// failed" here, so this is decoded from the ASSEMBLY, not the pseudocode).
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
}
