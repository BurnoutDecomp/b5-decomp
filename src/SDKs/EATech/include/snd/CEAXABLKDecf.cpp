// ============================================================================
// SDKs/EATech/include/snd/CEAXABLKDecf.cpp
//
// Out-of-line definition for Snd::process_raw_block (EA-XA raw-block sub-path).
// Reconstructed from the X360 assembly @ 0x82B80758. Every 16-bit sample is read
// big-endian (two bytes spliced hi<<8|lo, sign-extended) and converted to float
// via the X360's fcfid/frsp integer->float path (a plain (f32) cast here).
// ============================================================================

#include "SDKs/EATech/include/snd/CEAXABLKDecf.h"

namespace Snd
{
    // Read one big-endian s16 from apSource and return it as a float.
    static inline f32 ReadBigEndianSampleF32(const u8* apSource)
    {
        return static_cast<f32>(
            static_cast<s16>((apSource[0] << 8) | apSource[1]));
    }

    // 0x82B80758.
    XaBlockDecoder* process_raw_block(XaBlockDecoder* apState)
    {
        const u8* lpSource = apState->mpSource;
        if (*lpSource != 0xEE)
            return apState;

        ++lpSource;                       // consume the 0xEE raw-block marker
        apState->mpSource = lpSource;

        // Two big-endian s16 seed samples into the history slots.
        apState->mfHist0 = ReadBigEndianSampleF32(lpSource);
        lpSource += 2;
        apState->mpSource = lpSource;

        apState->mfHist1 = ReadBigEndianSampleF32(lpSource);
        lpSource += 2;
        apState->mpSource = lpSource;

        // 28 further big-endian s16 samples decoded verbatim into the output.
        for (s32 liSample = 0; liSample < 28; ++liSample)
        {
            const u8* lpSrc = apState->mpSource;
            f32* lpDst = apState->mpDest;
            *lpDst = ReadBigEndianSampleF32(lpSrc);
            apState->mpSource = lpSrc + 2;
            apState->mpDest = lpDst + 1;
        }
        return apState;
    }
} // namespace Snd
