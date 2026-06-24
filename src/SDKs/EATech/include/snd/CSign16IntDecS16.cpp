// ============================================================================
// SDKs/EATech/include/snd/CSign16IntDecS16.cpp
//
// Out-of-line definitions for Snd::CSign16IntDecS16 (signed 16-bit source, byte-
// swapped into the s16 destination):
//
//   - Snd::CSign16IntDecS16::Decode                       @ 0x82B7A4E0
//   - Snd::CSign16IntDecS16::~CSign16IntDecS16  (vector deleting dtor) @ 0x82B7D0E8
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/SampleDecoder.h"

namespace Snd
{
    // 0x82B7A4E0. For each of aiCount samples and each channel, byte-swap the
    // next 16-bit source word into apDest[channel][sample]. The source cursor
    // advances by 2 bytes per channel sample.
    //
    // Store-for-store with the X360:
    //   lwz +0xC remaining; bail (return 0) if 0; clamp aiCount to remaining.
    //   lha +0x14 channel count (signed); inner do/while over channels.
    //   lwz +4 source; lhz +0 source word; (word<<8)|(word>>8) byte-swap; sth +0.
    //   stw +8 last-dest; source += 2; stw +0xC remaining -= aiCount; return aiCount.
    s32 CSign16IntDecS16::Decode(s16* const* apDest, s32 aiCount)
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
                    const u16* lpSource = reinterpret_cast<const u16*>(mpSource);
                    s16* lpDest = apDest[liChannel] + liSample;
                    ++liChannel;
                    mpLastDest = lpDest;

                    const u16 luWord = *lpSource;
                    *lpDest = static_cast<s16>((luWord << 8) | (luWord >> 8));

                    mpSource += 2;
                }
                while (liChannel < miNumChannels);
            }
        }

        miRemaining -= aiCount;
        return aiCount;
    }

    // 0x82B7D0E8. Vector-deleting destructor: vtable store + conditional delete.
    CSign16IntDecS16::~CSign16IntDecS16()
    {
    }
} // namespace Snd
