// ============================================================================
// SDKs/EATech/include/snd/CEAXABLKDecf.cpp
//
// Out-of-line definitions for the EA-XA block decoder: Snd::process_raw_block
// (raw-block sub-path) and Snd::decodexac (the ADPCM block loop). Reconstructed
// from the console assembly. Every raw 16-bit sample is read big-endian (two
// bytes spliced hi<<8|lo, sign-extended) and converted to float via the
// console's fcfid/frsp integer->float path (a plain (f32) cast here). The two
// decodexac tables are dumped from the image.
// ============================================================================

#include "SDKs/EATech/include/snd/CEAXABLKDecf.h"

namespace Snd
{
    namespace
    {
        // EA-XA predictor pairs, indexed by the header's high nibble: entries 0..3
        // weight the previous output, entries 4..7 the one before it.
        const f32 kafXaPredictor[8] =
        {
            0.0f, 0.9375f, 1.796875f, 1.53125f, 0.0f, 0.0f, -0.8125f, -0.859375f
        };

        // EA-XA residual dequantiser: 16 shift rows of 16 signed 4-bit residuals
        // (already scaled), indexed by (header low nibble) * 16 + residual nibble.
        const f32 kafXaResidual[256] =
        {
            0.0f, 4096.0f, 8192.0f, 12288.0f, 16384.0f, 20480.0f, 24576.0f, 28672.0f,
            -32768.0f, -28672.0f, -24576.0f, -20480.0f, -16384.0f, -12288.0f, -8192.0f, -4096.0f,
            0.0f, 2048.0f, 4096.0f, 6144.0f, 8192.0f, 10240.0f, 12288.0f, 14336.0f,
            -16384.0f, -14336.0f, -12288.0f, -10240.0f, -8192.0f, -6144.0f, -4096.0f, -2048.0f,
            0.0f, 1024.0f, 2048.0f, 3072.0f, 4096.0f, 5120.0f, 6144.0f, 7168.0f,
            -8192.0f, -7168.0f, -6144.0f, -5120.0f, -4096.0f, -3072.0f, -2048.0f, -1024.0f,
            0.0f, 512.0f, 1024.0f, 1536.0f, 2048.0f, 2560.0f, 3072.0f, 3584.0f,
            -4096.0f, -3584.0f, -3072.0f, -2560.0f, -2048.0f, -1536.0f, -1024.0f, -512.0f,
            0.0f, 256.0f, 512.0f, 768.0f, 1024.0f, 1280.0f, 1536.0f, 1792.0f,
            -2048.0f, -1792.0f, -1536.0f, -1280.0f, -1024.0f, -768.0f, -512.0f, -256.0f,
            0.0f, 128.0f, 256.0f, 384.0f, 512.0f, 640.0f, 768.0f, 896.0f,
            -1024.0f, -896.0f, -768.0f, -640.0f, -512.0f, -384.0f, -256.0f, -128.0f,
            0.0f, 64.0f, 128.0f, 192.0f, 256.0f, 320.0f, 384.0f, 448.0f,
            -512.0f, -448.0f, -384.0f, -320.0f, -256.0f, -192.0f, -128.0f, -64.0f,
            0.0f, 32.0f, 64.0f, 96.0f, 128.0f, 160.0f, 192.0f, 224.0f,
            -256.0f, -224.0f, -192.0f, -160.0f, -128.0f, -96.0f, -64.0f, -32.0f,
            0.0f, 16.0f, 32.0f, 48.0f, 64.0f, 80.0f, 96.0f, 112.0f,
            -128.0f, -112.0f, -96.0f, -80.0f, -64.0f, -48.0f, -32.0f, -16.0f,
            0.0f, 8.0f, 16.0f, 24.0f, 32.0f, 40.0f, 48.0f, 56.0f,
            -64.0f, -56.0f, -48.0f, -40.0f, -32.0f, -24.0f, -16.0f, -8.0f,
            0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f, 28.0f,
            -32.0f, -28.0f, -24.0f, -20.0f, -16.0f, -12.0f, -8.0f, -4.0f,
            0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f,
            -16.0f, -14.0f, -12.0f, -10.0f, -8.0f, -6.0f, -4.0f, -2.0f,
            0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f,
            -8.0f, -7.0f, -6.0f, -5.0f, -4.0f, -3.0f, -2.0f, -1.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        };
    } // namespace

    // Read one big-endian s16 from apSource and return it as a float.
    static inline f32 ReadBigEndianSampleF32(const u8* apSource)
    {
        return static_cast<f32>(
            static_cast<s16>((apSource[0] << 8) | apSource[1]));
    }

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

    // Seed the predictor history into the two samples before the output cursor,
    // then decode blocks of 28 samples until miCount runs out. In an ADPCM block
    // each output is  prev * A + prev2 * B + residual,  the two residuals of a
    // byte taken high nibble first; the fused multiply-adds of the console are
    // written as separate products here.
    XaBlockDecoder* decodexac(XaBlockDecoder* apState)
    {
        apState->mpDest[-2] = apState->mfHist1;
        apState->mpDest[-1] = apState->mfHist0;

        while (apState->miCount > 0)
        {
            if (*apState->mpSource == 0xEE)
            {
                process_raw_block(apState);
                apState->miCount -= 28;
                continue;
            }

            apState->miCount -= 28;

            const u8 luHeader = *apState->mpSource;
            apState->mpSource += 1;

            const f32 lfPrev  = kafXaPredictor[luHeader >> 4];
            const f32 lfPrev2 = kafXaPredictor[4 + (luHeader >> 4)];
            const f32* lpResidual = &kafXaResidual[(luHeader & 0xF) * 16];

            for (s32 liByte = 0; liByte < 14; ++liByte)
            {
                const u8 luCodes = apState->mpSource[liByte];
                f32* lpDest = apState->mpDest;
                lpDest[0] = lpDest[-1] * lfPrev + (lpDest[-2] * lfPrev2 + lpResidual[luCodes >> 4]);
                lpDest[1] = lpDest[0] * lfPrev + (lpDest[-1] * lfPrev2 + lpResidual[luCodes & 0xF]);
                apState->mpDest = lpDest + 2;
            }

            apState->mpSource += 14;
            apState->mfHist1 = apState->mpDest[-2];
            apState->mfHist0 = apState->mpDest[-1];
        }
        return apState;
    }
} // namespace Snd
