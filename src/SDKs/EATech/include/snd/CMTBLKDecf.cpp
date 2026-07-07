// ============================================================================
// SDKs/EATech/include/snd/CMTBLKDecf.cpp
//
// Out-of-line definition for Snd::discardbits (MicroTalk bit reader helper).
// Reconstructed from the X360 assembly @ 0x82B79600.
// ============================================================================

#include "SDKs/EATech/include/snd/CMTBLKDecf.h"

namespace Snd
{
    // 0x82B79600. Store-for-store with the X360:
    //   accum = reader[1]; bitsLeft = reader[2] - aiBits; reader[2] = bitsLeft;
    //   shifted = accum >> aiBits; reader[1] = shifted;
    //   if (bitsLeft < 8) { byte = *reader[0]; reader[0]++; reader[2] = bitsLeft+8;
    //                       reader[1] = (byte << bitsLeft) | shifted; }
    MtBitReader* discardbits(MtBitReader* apBits, s32 aiBits)
    {
        const u32 luAccum    = apBits->muAccum;
        const s32 liBitsLeft = apBits->miBitsLeft - aiBits;
        apBits->miBitsLeft   = liBitsLeft;

        const u32 luShifted  = luAccum >> aiBits;
        apBits->muAccum      = luShifted;

        if (liBitsLeft < 8)
        {
            const u8 luByte = *apBits->mpSource;
            apBits->mpSource += 1;
            apBits->miBitsLeft = liBitsLeft + 8;
            apBits->muAccum = (static_cast<u32>(luByte) << liBitsLeft) | luShifted;
        }
        return apBits;
    }
} // namespace Snd
