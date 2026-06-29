// ============================================================================
// SDKs/EATech/include/snd/CSign24IntDecS16.cpp
//
// Out-of-line definitions for Snd::CSign24IntDecS16 (signed 24-bit source: the
// upper two bytes of each little-endian 24-bit frame are taken big-endian into
// the s16 destination -- byte +2 high, byte +1 low; the low byte +0 is dropped):
//
//   - Snd::CSign24IntDecS16::Decode                          @ 0x82B7A648
//   - Snd::CSign24IntDecS16::Feed                            @ 0x82B7A7A0
//   - Snd::CSign24IntDecS16::~CSign24IntDecS16  (scalar deleting dtor) @ 0x82B7D188
//
// Sibling-mirrored store-for-store on the already-committed CSign24bIntDecS16 /
// CUnSign8IntDecS16 bodies; the only per-format difference here is the source
// byte selection (+2,+1 vs the 'b' variant's +0,+1).
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/SampleDecoder.h"

namespace Snd
{
    // 0x82B7A648. For each of aiCount samples and each channel, combine the upper
    // two bytes of the next little-endian 24-bit source frame (src[2] high,
    // src[1] low) into apDest[channel][sample]. The source cursor advances by 3
    // bytes per channel sample (24-bit source frames); the lowest byte src[0] is
    // discarded.
    //
    // Store-for-store with the X360:
    //   lwz +0xC remaining; bail (return 0) if 0; clamp aiCount to remaining.
    //   lha +0x14 channel count (signed); inner do/while over channels.
    //   lwz +4 source; lbz +2 / lbz +1; (src[2]<<8)|src[1]; sth +0; stw +8 last-dest.
    //   source += 3; stw +0xC remaining -= aiCount; return aiCount.
    s32 CSign24IntDecS16::Decode(s16* const* apDest, s32 aiCount)
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

                    *lpDest = static_cast<s16>((static_cast<u32>(lpSource[2]) << 8)
                                               | static_cast<u32>(lpSource[1]));

                    mpSource += 3;
                }
                while (liChannel < miNumChannels);
            }
        }

        miRemaining -= aiCount;
        return aiCount;
    }

    // 0x82B7A7A0. Hand a source span to the decoder. Fails (-1) when no source is
    // supplied or a previous span is still pending (miRemaining != 0). On success
    // it records the source cursor, the capacity and the remaining-sample count,
    // then returns 0.
    //
    // Store-for-store with the X360:
    //   cmplwi r4 -> return -1 if apSource is null.
    //   lwz +0xC -> return -1 if miRemaining already non-zero.
    //   stw r6 -> +0xC miRemaining; stw r5 -> +0x10 miCapacity; stw r4 -> +4 mpSource.
    s32 CSign24IntDecS16::Feed(u8* apSource, s32 aiCapacity, s32 aiRemaining)
    {
        if (apSource == 0 || miRemaining != 0)
            return -1;

        miRemaining = aiRemaining;
        miCapacity = aiCapacity;
        mpSource = apSource;
        return 0;
    }

    // 0x82B7D188. Scalar-deleting destructor: vtable store + conditional delete.
    CSign24IntDecS16::~CSign24IntDecS16()
    {
    }
} // namespace Snd
