// ============================================================================
// SDKs/EATech/include/snd/CSign24bIntDecS16.cpp
//
// Out-of-line definition for Snd::CSign24bIntDecS16 (signed 24-bit source: the
// leading two bytes are taken big-endian into the s16 destination):
//
//   - Snd::CSign24bIntDecS16::Decode  @ 0x82B7A598
//
// This TU has a single ledger function (Decode); no destructor is recorded for
// it, so none is defined here.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/SampleDecoder.h"

namespace Snd
{
    // 0x82B7A598. For each of aiCount samples and each channel, combine the first
    // two source bytes (src[0] high, src[1] low) into apDest[channel][sample]. The
    // source cursor advances by 3 bytes per channel sample (24-bit source frames).
    //
    // Store-for-store with the X360:
    //   lwz +0xC remaining; bail (return 0) if 0; clamp aiCount to remaining.
    //   lha +0x14 channel count (signed); inner do/while over channels.
    //   lwz +4 source; lbz +0 / lbz +1; (src[0]<<8)|src[1]; sth +0; stw +8 last-dest.
    //   source += 3; stw +0xC remaining -= aiCount; return aiCount.
    s32 CSign24bIntDecS16::Decode(s16* const* apDest, s32 aiCount)
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

                    *lpDest = static_cast<s16>((static_cast<u32>(lpSource[0]) << 8)
                                               | static_cast<u32>(lpSource[1]));

                    mpSource += 3;
                }
                while (liChannel < miNumChannels);
            }
        }

        miRemaining -= aiCount;
        return aiCount;
    }
} // namespace Snd
