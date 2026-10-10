// =====================================================================================
// rw::audio::core::Xas1Enc bodies -- the 'Xas1' (XAS1 block-ADPCM) stream encoder.
//
// EARenderWare "rwaudio". Reconstructed from the console image; its PowerPC asm is
// authoritative for every store, branch and rounding point. See Xas1Enc.h / Encoder.h.
//
// Floating-point notes, kept exact because the residuals are quantised from them:
//   * single precision throughout; the console's fused multiply-adds are written as
//     std::fma (one rounding) and every other product/sum as a plain f32 operation (the
//     closed-loop prediction here is two products and a sum, NOT fused);
//   * the float-to-int conversions truncate toward zero;
//   * the comparisons keep the console's NaN behaviour (a NaN error magnitude is negated
//     and then replaces the running maximum).
// =====================================================================================

#include "rw/audio/core/Xas1Enc.h"
#include "rw/audio/core/PlugIn.h" // System::Alloc

#include <cmath>   // std::fma
#include <cstring> // std::memcpy (the console's XMemCpy / memcpy)
#include <new>     // placement new (the v-table install over the System allocation)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
const f32 KF_ZERO = 0.0f;                 // the +0x08 seed; the error zero
const s32 KI_FIELD0C_SEED = 4000;         // the +0x0C seed
const f32 KF_BYTES_PER_SAMPLE = 0.59375f; // 76 bytes per 128 samples
const f32 KF_FULL_SCALE = 32767.0f;
const f32 KF_HEADER_ROUND = 8.0f;         // rounds a header sample to its 12-bit grid
const f32 KF_BEST_ERROR_INIT = 1.0e9f;
const f32 KF_SILENT_ERROR = 7.0f;         // predictor 0 is taken outright at or below this

// The four second-order predictors, {c0 (previous sample), c1 (sample before)} pairs, as
// stored in the image's rodata (this TU's own copy).
const f32 KAF_PREDICTOR[4][2] = {
    { 0.0f, 0.0f },
    { -0.9375f, 0.0f },
    { -1.796875f, 0.8125f },
    { -1.53125f, 0.859375f },
};

enum
{
    KI_SUBBLOCK_SAMPLES = 32,
    KI_RESIDUALS = 30, // samples per sub-block after the two header samples
    KI_PREDICTORS = 4,
    KI_MAX_SHIFT = 12,
    KI_HEADER_BYTES = 4,
    KI_RESIDUAL_BYTES = 15
};

// A header sample: scaled, rounded by +8, truncated, its low nibble cleared, and clamped
// from above only.
s32 HeaderSample(f32 afSample)
{
    s32 liValue = static_cast<s32>(std::fma(afSample, KF_FULL_SCALE, KF_HEADER_ROUND)) & ~0xF;
    if (liValue > 0x7FF0)
        liValue = 0x7FF0;
    return liValue;
}

// The sub-block shift: the number of leading positions, from bit 14 down, before the peak
// error (rounded at three bits below each probe) reaches a set bit, at most 12.
s32 ShiftForPeak(f32 afPeakError)
{
    s32 liPeak = static_cast<s32>(afPeakError);
    if (liPeak > 0x7FFF)
        liPeak = 0x7FFF;
    if (liPeak < -0x8000)
        liPeak = -0x8000;

    s32 liShift = 0;
    for (s32 liMask = 0x4000; liShift < KI_MAX_SHIFT; liMask >>= 1)
    {
        if (((liMask >> 3) + liPeak) & liMask)
            break;
        ++liShift;
    }
    return liShift;
}

// Quantise one scaled residual: round at bit 11, keep the top four bits of the 16-bit
// range, clamp to s16.
s32 QuantiseResidual(f32 afScaled)
{
    s32 liValue = (static_cast<s32>(afScaled) + 0x800) & ~0xFFF;
    if (liValue > 0x7FFF)
        liValue = 0x7FFF;
    if (liValue < -0x8000)
        liValue = -0x8000;
    return liValue;
}
} // namespace

// -------------------------------------------------------------------------------------
// CreateInstance -- System::Alloc(pSystem, size, no name, align 16), then on success install
// the v-table and seed +0x20 / +0x08 / +0x0C / +0x04 in the console's store order. The
// console asks for its own sizeof (0x8024); the host asks for the host sizeof.
// -------------------------------------------------------------------------------------
Xas1Enc *Xas1Enc::CreateInstance(s32 iNumChannels, s32 iSampleRate, System *pSystem)
{
    void *lpMem = System::Alloc(pSystem, static_cast<u32>(sizeof(Xas1Enc)), nullptr, 16, nullptr);
    if (!lpMem)
        return nullptr;

    Xas1Enc *lpEnc = ::new (lpMem) Xas1Enc; // the v-table store, nothing else
    lpEnc->miBufferedSamples = 0;
    lpEnc->mfField08 = KF_ZERO;
    lpEnc->miField0C = KI_FIELD0C_SEED;
    lpEnc->mfDataRate = static_cast<f32>(iNumChannels * iSampleRate) * KF_BYTES_PER_SAMPLE;
    return lpEnc;
}

// -------------------------------------------------------------------------------------
// CustomInterleaveXAS -- the four 4-byte sub-block headers first (one XMemCpy each), then
// for each residual byte position i, that byte of sub-blocks 0, 1, 2, 3.
// -------------------------------------------------------------------------------------
void Xas1Enc::CustomInterleaveXAS(const u8 *pSubBlocks, u8 *pOutput)
{
    for (s32 liSub = 0; liSub < KI_SUBBLOCKS; ++liSub)
        std::memcpy(pOutput + KI_HEADER_BYTES * liSub, pSubBlocks + KI_SUBBLOCK_BYTES * liSub,
                    KI_HEADER_BYTES);

    u8 *lpDst = pOutput + KI_HEADER_BYTES * KI_SUBBLOCKS;
    for (s32 liByte = 0; liByte < KI_RESIDUAL_BYTES; ++liByte)
    {
        for (s32 liSub = 0; liSub < KI_SUBBLOCKS; ++liSub)
            lpDst[liSub] = pSubBlocks[KI_HEADER_BYTES + KI_SUBBLOCK_BYTES * liSub + liByte];
        lpDst += KI_SUBBLOCKS;
    }
}

// -------------------------------------------------------------------------------------
// EncodeBlock -- per channel, four 32-frame sub-blocks: take the sub-block's first two
// frames as its header samples, pick the predictor whose largest open-loop error over the
// remaining 30 frames is smallest (predictor 0 wins at once when its error is at most 7),
// derive the shift from that peak error, then quantise the scaled input against a
// closed-loop history seeded with the two header samples. The four 19-byte sub-blocks are
// assembled in a local block and reordered by CustomInterleaveXAS into the channel's 76
// output bytes. The selected predictor index is NOT reset between sub-blocks or channels.
// -------------------------------------------------------------------------------------
void Xas1Enc::EncodeBlock(const f32 *pafFrames, u8 *pOutput, s32 iNumChannels)
{
    s32 liPredictor = 0;

    for (s32 liChannel = 0; liChannel < iNumChannels; ++liChannel)
    {
        u8 lauSubBlocks[KI_SUBBLOCKS * KI_SUBBLOCK_BYTES];
        u8 *lpBytes = lauSubBlocks;
        const f32 *lpFrame = pafFrames;

        for (s32 liSub = 0; liSub < KI_SUBBLOCKS; ++liSub)
        {
            const s32 liHeader0 = HeaderSample(lpFrame[liChannel]);
            const s32 liHeader1 = HeaderSample(lpFrame[iNumChannels + liChannel]);
            const f32 *lpRest = lpFrame + 2 * iNumChannels + liChannel;

            f32 lfBestError = KF_BEST_ERROR_INIT;
            f32 lafPeakError[KI_PREDICTORS];
            f32 lafScaled[KI_PREDICTORS][KI_RESIDUALS];

            for (s32 liK = 0; liK < KI_PREDICTORS; ++liK)
            {
                const f32 lfC0 = KAF_PREDICTOR[liK][0];
                const f32 lfC1 = KAF_PREDICTOR[liK][1];
                f32 lfPeak = KF_ZERO;
                lafPeakError[liK] = KF_ZERO;

                s32 liPrev1 = liHeader1;
                s32 liPrev2 = liHeader0;
                const f32 *lpIn = lpRest;
                for (s32 liN = 0; liN < KI_RESIDUALS; ++liN)
                {
                    const f32 lfScaled = *lpIn * KF_FULL_SCALE;
                    lafScaled[liK][liN] = lfScaled;
                    f32 lfError = std::fma(static_cast<f32>(liPrev1), lfC0,
                                           static_cast<f32>(liPrev2) * lfC1) + lfScaled;
                    if (!(lfError > KF_ZERO))
                        lfError = -lfError;
                    if (!(lfPeak >= lfError))
                        lfPeak = lfError;

                    liPrev2 = liPrev1;
                    liPrev1 = static_cast<s32>(lfScaled);
                    lpIn += iNumChannels;
                }
                lafPeakError[liK] = lfPeak;

                if (!(lfBestError <= lfPeak))
                {
                    lfBestError = lfPeak;
                    liPredictor = liK;
                }
                if (liK == 0 && lafPeakError[0] <= KF_SILENT_ERROR)
                {
                    liPredictor = 0;
                    break;
                }
            }

            const s32 liShift = ShiftForPeak(lafPeakError[liPredictor]);

            const s32 liWord0 = liHeader0 | liPredictor;
            const s32 liWord1 = liShift | liHeader1;
            lpBytes[0] = static_cast<u8>(liWord0);
            lpBytes[1] = static_cast<u8>(liWord0 >> 8);
            lpBytes[2] = static_cast<u8>(liWord1);
            lpBytes[3] = static_cast<u8>(liWord1 >> 8);
            lpBytes += KI_HEADER_BYTES;

            const f32 lfC0 = KAF_PREDICTOR[liPredictor][0];
            const f32 lfC1 = KAF_PREDICTOR[liPredictor][1];
            const f32 lfScale = static_cast<f32>(1 << liShift);
            f32 lfHist2 = static_cast<f32>(liHeader0);
            f32 lfHist1 = static_cast<f32>(liHeader1);

            for (s32 liJ = 0; liJ < KI_RESIDUALS; ++liJ)
            {
                const f32 lfPrediction = lfC1 * lfHist2 + lfC0 * lfHist1;
                s32 liCode = QuantiseResidual((lfPrediction + lafScaled[liPredictor][liJ]) * lfScale);
                if (liJ & 1)
                {
                    *lpBytes = static_cast<u8>(*lpBytes | ((liCode >> 12) & 0xF));
                    ++lpBytes;
                }
                else
                {
                    *lpBytes = static_cast<u8>((liCode >> 8) & 0xF0);
                }
                liCode >>= liShift;
                lfHist2 = lfHist1;
                lfHist1 = static_cast<f32>(liCode) - lfPrediction;
            }

            lpFrame += KI_SUBBLOCK_SAMPLES * iNumChannels;
        }

        CustomInterleaveXAS(lauSubBlocks, pOutput);
        pOutput += KI_BLOCK_BYTES;
    }
}

// -------------------------------------------------------------------------------------
// Encode -- top up a buffered partial block first (coding it when it reaches 128 frames),
// code every whole block straight from the input, and buffer the remainder. Returns the
// frames coded by this call (a frame left in the buffer is counted when its block is coded).
// -------------------------------------------------------------------------------------
s32 Xas1Enc::Encode(const f32 *pafInput, void *pOutput, s32 iNumSamples, s32 *piBytesWritten,
                    s32 /*iArg5*/, s32 *piAuxOut)
{
    u8 *lpOut = static_cast<u8 *>(pOutput);
    s32 liCoded = 0;
    *piBytesWritten = 0;
    if (piAuxOut)
        *piAuxOut = 0;

    if (miBufferedSamples != 0)
    {
        s32 liTake = KI_BLOCK_SAMPLES - miBufferedSamples;
        if (iNumSamples < liTake)
            liTake = iNumSamples;
        std::memcpy(mafBuffer + mucChannelCount * miBufferedSamples, pafInput,
                    4 * mucChannelCount * liTake);
        iNumSamples -= liTake;
        miBufferedSamples += liTake;
        pafInput += mucChannelCount * liTake;

        if (miBufferedSamples == KI_BLOCK_SAMPLES)
        {
            EncodeBlock(mafBuffer, lpOut, mucChannelCount);
            liCoded = KI_BLOCK_SAMPLES;
            const s32 liBytes = KI_BLOCK_BYTES * mucChannelCount;
            *piBytesWritten += liBytes;
            lpOut += liBytes;
            miBufferedSamples = 0;
        }
    }

    s32 liBlocks = iNumSamples / KI_BLOCK_SAMPLES;
    if (liBlocks > 0)
    {
        iNumSamples -= liBlocks * KI_BLOCK_SAMPLES;
        liCoded += liBlocks * KI_BLOCK_SAMPLES;
        do
        {
            EncodeBlock(pafInput, lpOut, mucChannelCount);
            const s32 liBytes = KI_BLOCK_BYTES * mucChannelCount;
            lpOut += liBytes;
            pafInput += KI_BLOCK_SAMPLES * mucChannelCount;
            *piBytesWritten += liBytes;
        } while (--liBlocks);
    }

    if (iNumSamples != 0)
    {
        miBufferedSamples = iNumSamples;
        std::memcpy(mafBuffer, pafInput, 4 * mucChannelCount * iNumSamples);
    }
    return liCoded;
}

// -------------------------------------------------------------------------------------
// Flush -- when a partial block is buffered, repeat its last frame to fill the block, code
// it, report its bytes and return the frames that were buffered; otherwise report 0 bytes
// and return 0.
// -------------------------------------------------------------------------------------
s32 Xas1Enc::Flush(void *pOutput, s32 *piBytesWritten, s32 /*iArg3*/, s32 *piAuxOut)
{
    if (piAuxOut)
        *piAuxOut = 0;

    const s32 liBuffered = miBufferedSamples;
    if (liBuffered > 0 && liBuffered < KI_BLOCK_SAMPLES)
    {
        const f32 *lpLast = mafBuffer + (liBuffered - 1) * mucChannelCount;
        f32 *lpFill = mafBuffer + liBuffered * mucChannelCount;
        for (s32 liPad = KI_BLOCK_SAMPLES - liBuffered; liPad != 0; --liPad)
        {
            for (s32 liC = 0; liC < mucChannelCount; ++liC)
                *lpFill++ = lpLast[liC];
        }

        EncodeBlock(mafBuffer, static_cast<u8 *>(pOutput), mucChannelCount);
        miBufferedSamples = 0;
        *piBytesWritten = KI_BLOCK_BYTES * mucChannelCount;
        return liBuffered;
    }

    *piBytesWritten = 0;
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
