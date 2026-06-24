// ============================================================================
// SDKs/EATech/include/snd/CUnSign8IntDecS16.cpp
//
// Out-of-line definitions for Snd::CUnSign8IntDecS16 (unsigned 8-bit source,
// biased by -128 and shifted into the high byte of the s16 destination):
//
//   - Snd::CUnSign8IntDecS16::Decode                        @ 0x82B7A7D8
//   - Snd::CUnSign8IntDecS16::~CUnSign8IntDecS16  (scalar deleting dtor) @ 0x82B7D048
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/SampleDecoder.h"

namespace Snd
{
    // 0x82B7A7D8. For each of aiCount samples and each channel, convert the next
    // unsigned 8-bit source byte to a signed s16 by biasing -128 and shifting up
    // 8 bits into apDest[channel][sample]. The source cursor advances by 1 byte
    // per channel sample.
    //
    // Store-for-store with the X360:
    //   lwz +0xC remaining; bail (return 0) if 0; clamp aiCount to remaining.
    //   lha +0x14 channel count (signed); inner do/while over channels.
    //   lwz +4 source; lbz +0 source byte; (byte-0x80)<<8; sth +0; stw +8 last-dest.
    //   source += 1; stw +0xC remaining -= aiCount; return aiCount.
    s32 CUnSign8IntDecS16::Decode(s16* const* apDest, s32 aiCount)
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

                    *lpDest = static_cast<s16>((static_cast<s32>(*lpSource) - 128) << 8);

                    mpSource += 1;
                }
                while (liChannel < miNumChannels);
            }
        }

        miRemaining -= aiCount;
        return aiCount;
    }

    // 0x82B7D048. Scalar-deleting destructor: vtable store + conditional delete.
    CUnSign8IntDecS16::~CUnSign8IntDecS16()
    {
    }
} // namespace Snd
