// ============================================================================
// SDKs/EATech/include/snd/CSign8IntDecS16.cpp
//
// Out-of-line definitions for Snd::CSign8IntDecS16 (signed 8-bit source shifted
// into the high byte of the s16 destination -- the signed counterpart to the
// -128-biased CUnSign8IntDecS16):
//
//   - Snd::CSign8IntDecS16::Decode                        @ 0x82B7A6F8
//   - Snd::CSign8IntDecS16::SetState                      @ 0x82B7A888
//   - Snd::CSign8IntDecS16::~CSign8IntDecS16  (scalar deleting dtor) @ 0x82B7D098
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/SampleDecoder.h"

namespace Snd
{
    // 0x82B7A6F8. For each of aiCount samples and each channel, widen the next
    // signed 8-bit source byte into apDest[channel][sample] by shifting it up 8
    // bits into the high byte of the s16 (no bias -- this is the signed variant of
    // CUnSign8IntDecS16). The source cursor advances by 1 byte per channel sample.
    //
    // Store-for-store with the X360:
    //   lwz +0xC remaining; bail (return 0) if 0; clamp aiCount to remaining.
    //   lha +0x14 channel count (signed); inner do/while over channels.
    //   lwz +4 source; lbz +0 source byte; rotlwi 8 (== <<8); sth +0; stw +8 last-dest.
    //   source += 1; stw +0xC remaining -= aiCount; return aiCount.
    s32 CSign8IntDecS16::Decode(s16* const* apDest, s32 aiCount)
    {
        const s32 liRemaining = miRemaining;
        if (liRemaining == 0)
            return 0;
        if (aiCount >= liRemaining)
            aiCount = miRemaining;

        for (s32 liSample = 0; liSample < aiCount; ++liSample)
        {
            if (miNumChannels > 0)
            {
                s16 liChannel = 0;
                do
                {
                    const u8* lpSource = mpSource;
                    s16* lpDest = apDest[liChannel] + liSample;
                    ++liChannel;
                    mpLastDest = lpDest;

                    *lpDest = static_cast<s16>(static_cast<u32>(*lpSource) << 8);

                    mpSource += 1;
                }
                while (liChannel < miNumChannels);
            }
        }

        miRemaining -= aiCount;
        return aiCount;
    }

    // 0x82B7A888. Set the output channel count from *apNumChannels.
    //   lhz +0 source word -> sth +0x14 (miNumChannels). Returns this on the X360;
    //   modelled as void since the result is never consumed by callers in the family.
    void CSign8IntDecS16::SetState(const s16* apNumChannels)
    {
        miNumChannels = *apNumChannels;
    }

    // 0x82B7D098. Scalar-deleting destructor: vtable store + conditional delete.
    CSign8IntDecS16::~CSign8IntDecS16()
    {
    }
} // namespace Snd
