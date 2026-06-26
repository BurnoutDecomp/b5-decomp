#include "GameShared/GameClasses/Network/Packeting/BitStream/CgsIntQuantiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_*.XEX
//   CgsNetwork::IntQuantiser::UnPack @ 0x82871710 -- add the packed unsigned
//      offset back onto the range minimum and clamp the result into
//      [liMinValue, liMaxValue]. The X360 build computes (min + packed), asserts
//      the sum is inside the range (CgsIntQuantiser.cpp:89), then folds it back
//      through a Min/Max clamp so an out-of-range value is pinned to the nearest
//      bound. Store-for-store faithful to the asm.
//   CgsNetwork::IntQuantiser::GetNumBits -- bit width of the range [min,max]:
//      the count of right-shifts it takes to drive (max - min) to zero, i.e.
//      bit_width(max - min); 0 when max == min. The X360 inlines this
//      into the quantised-int unpack helper @0x828800F0
//        v8 = max - min; v7 = 0; if (max != min) { do { v8 >>= 1; ++v7; } while (v8); }
//      so the loop here matches that inlined arithmetic exactly.
//   CgsNetwork::IntQuantiser::Pack -- the asm-confirmed inverse of UnPack: clamp
//      value into [min,max], emit the unsigned offset (clamp(value) - min) and the
//      bit width GetNumBits(min,max) reports. The clamp is the Min/Max pair the
//      DWARF hints (CgsIntQuantiser.cpp:53) and the @0x82880F50 pack path show.

namespace CgsNetwork
{
    // ---- GetNumBits ------------------------------------------------------------
    // Number of bits needed to hold the range span: shift (max - min) right until
    // it reaches zero and count the shifts. Inlined @0x828800F0 (the quantised-int
    // unpack helper) as the bit count fed to BitStream::GetBits.
    s32 IntQuantiser::GetNumBits(s32 liMinValue, s32 liMaxValue)
    {
        u32 luSpan   = static_cast<u32>(liMaxValue) - static_cast<u32>(liMinValue);
        s32 liNumBits = 0;
        if (liMaxValue != liMinValue)
        {
            do
            {
                luSpan >>= 1;
                ++liNumBits;
            }
            while (luSpan);
        }
        return liNumBits;
    }

    // ---- Pack @ 0x82871568 -----------------------------------------------------
    // Inverse of UnPack: clamp the value into [min,max], store the unsigned offset
    // (clamped - min) as the packed value, and report GetNumBits(min,max) bits.
    void IntQuantiser::Pack(s32 liValue, s32 liMinValue, s32 liMaxValue,
                            u32* lpuPackedValue, s32* lpiNumBitsUsed)
    {
        // Clamp into [min,max] -- the Min/Max pair (so an out-of-range value is
        // pinned to the nearest bound, mirroring UnPack's clamp).
        s32 liClamped = liValue;
        if (liClamped > liMaxValue)
        {
            liClamped = liMaxValue;
        }
        if (liClamped < liMinValue)
        {
            liClamped = liMinValue;
        }

        *lpuPackedValue = static_cast<u32>(liClamped - liMinValue);
        *lpiNumBitsUsed = GetNumBits(liMinValue, liMaxValue);
    }

    // ---- UnPack @ 0x82871710 ---------------------------------------------------
    void IntQuantiser::UnPack(s32* lpiValue, s32 liMinValue, s32 liMaxValue,
                              u32 luPackedValue)
    {
        // value = min + packed (the additive inverse of Pack's value - min).
        s32 liValue = liMinValue + static_cast<s32>(luPackedValue);
        *lpiValue   = liValue;

        // The asm asserts the reconstructed value sits inside the range before it
        // clamps; the original streamed "Value <v> is outside the range <min>:<max>".
        CGS_ASSERT(liValue >= liMinValue && liValue <= liMaxValue,
                   "liValue is outside the range [liMinValue, liMaxValue]");

        // Clamp to [min,max] -- the inlined Min/Max the asm folds in after the
        // assert (so an out-of-range value still resolves to the nearest bound).
        if (liValue > liMaxValue)
        {
            liValue = liMaxValue;
        }
        if (liValue < liMinValue)
        {
            *lpiValue = liMinValue;
        }
        else
        {
            *lpiValue = liValue;
        }
    }
}
