// ============================================================================
// SDKs/EATech/include/snd/CMTBLKDecf.cpp
//
// Out-of-line definitions for the MicroTalk (MT) block decoder:
//   - Snd::discardbits / Snd::getbits      bit reader
//   - Snd::readsamples                     excitation entropy decoder
//   - Snd::decodemut                       one-frame decoder
//   - (file-static) synthesisefilter       12-pole synthesis filter
//   - Snd::CMTBLKDecf::Feed / Decode / initmut
// Reconstructed from the console assembly; every table below is dumped from the
// image (big-endian dwords/bytes), never retyped from a reference decoder.
// ============================================================================

#include "SDKs/EATech/include/snd/CMTBLKDecf.h"

#include <cstring>  // memcpy

namespace Snd
{
    // Frame count staged into the window per decodemut() refill, and the coded
    // stride the window streams at. Both attested as 0x1B0 in the Decode asm.
    static const s32 KI_WINDOW_SAMPLES = 432;

    // One excitation subframe (a quarter of the window).
    static const s32 KI_SUBFRAME_SAMPLES = 108;

    // Order of the synthesis filter / number of reflection coefficients.
    static const s32 KI_FILTER_ORDER = 12;

    // Leading byte value that flags the start of the next MT frame (0xEE).
    static const u8 KU_FRAME_MARKER = 0xEE;

    namespace
    {
        // getbits mask table: kauBitMask[n] == (1 << n) - 1 for n = 0..8.
        const u32 kauBitMask[9] =
        {
            0, 1, 3, 7, 15, 31, 63, 127, 255,
        };

        // Reflection-coefficient dequantiser. The six-bit coefficients (k0..k3)
        // index all 64 entries; the five-bit coefficients (k4..k11) index the
        // 32-entry window starting at entry 16 (the image holds one table and the
        // five-bit path reads it through that offset).
        const f32 kafReflection[64] =
        {
            0.0f, -0.996776f, -0.990327f, -0.983879f, -0.977431f, -0.970982f, -0.964534f, -0.958085f,
            -0.951637f, -0.930754f, -0.90496f, -0.879167f, -0.853373f, -0.827579f, -0.801786f, -0.775992f,
            -0.750198f, -0.724405f, -0.698611f, -0.670635f, -0.619048f, -0.56746f, -0.515873f, -0.464286f,
            -0.412698f, -0.361111f, -0.309524f, -0.257937f, -0.206349f, -0.154762f, -0.103175f, -0.051587f,
            0.0f, 0.051587f, 0.103175f, 0.154762f, 0.206349f, 0.257937f, 0.309524f, 0.361111f,
            0.412698f, 0.464286f, 0.515873f, 0.56746f, 0.619048f, 0.670635f, 0.698611f, 0.724405f,
            0.750198f, 0.775992f, 0.801786f, 0.827579f, 0.853373f, 0.879167f, 0.90496f, 0.930754f,
            0.951637f, 0.958085f, 0.964534f, 0.970982f, 0.977431f, 0.983879f, 0.990327f, 0.996776f,
        };
        static const s32 KI_REFLECTION_5BIT_BASE = 16;

        // Variable-length excitation codebook: two 256-entry lookup rows indexed by
        // the low accumulator byte (row chosen by the previous code), giving the code
        // index into kaMtCode.
        const u8 kauMtLookup[2][256] =
        {
            {
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 21,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 25,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 22,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 0,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 21,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 26,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 22,
                4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
                4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 2,
            },
            {
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 27,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 1,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 28,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 3,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 27,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 1,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 28,
                4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
                4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 3,
            },
        };

        // Excitation code: next lookup row, code length in bits, and the decoded
        // value. Codes 0..1 are the long-magnitude escape, 2..3 the zero-run escape.
        struct MtCode
        {
            s32 miNextRow;
            s32 miBits;
            f32 mfValue;
        };

        const MtCode kaMtCode[29] =
        {
            { 1, 8, 0.0f },
            { 1, 7, 0.0f },
            { 0, 8, 0.0f },
            { 0, 7, 0.0f },
            { 0, 2, 0.0f },
            { 0, 2, -1.0f },
            { 0, 2, 1.0f },
            { 0, 3, -1.0f },
            { 0, 3, 1.0f },
            { 1, 4, -2.0f },
            { 1, 4, 2.0f },
            { 1, 3, -2.0f },
            { 1, 3, 2.0f },
            { 1, 5, -3.0f },
            { 1, 5, 3.0f },
            { 1, 4, -3.0f },
            { 1, 4, 3.0f },
            { 1, 6, -4.0f },
            { 1, 6, 4.0f },
            { 1, 5, -4.0f },
            { 1, 5, 4.0f },
            { 1, 7, -5.0f },
            { 1, 7, 5.0f },
            { 1, 6, -5.0f },
            { 1, 6, 5.0f },
            { 1, 8, -6.0f },
            { 1, 8, 6.0f },
            { 1, 7, -6.0f },
            { 1, 7, 6.0f },
        };

        // Gain applied to the reflection-coefficient step so the new coefficients
        // are reached over four interpolation steps.
        static const f32 KF_REFLECTION_STEP = 0.25f;

        // Long-term predictor gain quantiser step (four-bit gain, 1/15).
        static const f32 KF_PITCH_GAIN_STEP = 0.06666667f;

        // Half-band interpolator taps for the decimated (two-phase) excitation.
        static const f32 KF_INTERP_TAP1 = 0.59738594f;
        static const f32 KF_INTERP_TAP3 = 0.11459156f;
        static const f32 KF_INTERP_TAP5 = 0.01803268f;

        // Apply one quarter of the coefficient change to the live reflection set.
        inline void steprefl(CMTBLKDecf* apDec, const f32* apDelta)
        {
            for (s32 li = 0; li < KI_FILTER_ORDER; ++li)
                apDec->mafReflection[li] = apDec->mafReflection[li] + apDelta[li];
        }

        // Convert the live reflection coefficients to a direct-form predictor and
        // run aiBlocks blocks of 12 samples of mafWindow (from aiStart) through the
        // all-pole synthesis filter, in place.
        //
        // The predictor is derived from the lattice itself: the lattice state is
        // seeded with k0..k10 and a unit value in its feedback slot, run for twelve
        // steps (each step's output feeds back into the slot), and the predictor is
        // the deconvolution of that response. The filter history is a 12-entry ring
        // in mafSynthHistory; sample k of each block is written to slot 11 - k and is
        // predicted from every slot with the coefficient rotated by k, i.e.
        //   y[n] = x[n] + sum_{j=0..11} c[j] * y[n-1-j].
        void synthesisefilter(CMTBLKDecf* apDec, s32 aiStart, s32 aiBlocks)
        {
            const f32* lpRefl = apDec->mafReflection;

            // lafLattice[0] is the feedback slot, lafLattice[1 + i] the state of stage i.
            f32 lafLattice[KI_FILTER_ORDER];
            for (s32 li = 10; li >= 0; --li)
                lafLattice[1 + li] = lpRefl[li];

            const f32 lfLastRefl = lpRefl[11];
            lafLattice[0] = 1.0f;

            f32 lafResponse[KI_FILTER_ORDER];
            f32 lafCoef[KI_FILTER_ORDER];
            for (s32 liStep = 0; liStep < KI_FILTER_ORDER; ++liStep)
            {
                f32 lfValue = -(lfLastRefl * lafLattice[11]);
                for (s32 liStage = 10; liStage >= 0; --liStage)
                {
                    const f32 lfK    = lpRefl[liStage];
                    const f32 lfPrev = lafLattice[liStage];
                    lfValue = lfValue - lfPrev * lfK;
                    lafLattice[liStage + 1] = lfK * lfValue + lfPrev;
                }
                lafLattice[0] = lfValue;
                lafResponse[liStep] = lfValue;

                for (s32 li = 0; li < liStep; ++li)
                    lfValue = lfValue - lafCoef[li] * lafResponse[liStep - 1 - li];
                lafCoef[liStep] = lfValue;
            }

            f32* lpHistory = apDec->mafSynthHistory;
            f32* lpOut = &apDec->mafWindow[aiStart];
            for (s32 liBlock = 0; liBlock < aiBlocks; ++liBlock)
            {
                for (s32 liSample = 0; liSample < KI_FILTER_ORDER; ++liSample)
                {
                    f32 lfAcc = lpHistory[0] * lafCoef[liSample];
                    for (s32 li = 1; li < KI_FILTER_ORDER; ++li)
                        lfAcc = lfAcc + lpHistory[li] * lafCoef[(li + liSample) % KI_FILTER_ORDER];
                    lfAcc = lfAcc + lpOut[liSample];

                    lpHistory[(KI_FILTER_ORDER - 1) - liSample] = lfAcc;
                    lpOut[liSample] = lfAcc;
                }
                lpOut += KI_FILTER_ORDER;
            }
        }
    } // namespace

    // Store-for-store with the console:
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

    // The low aiCount bits of the accumulator (masked through kauBitMask) are the
    // result; the reader then advances exactly as discardbits does.
    s32 getbits(MtBitReader* apBits, s32 aiCount)
    {
        const u32 luAccum    = apBits->muAccum;
        const s32 liBitsLeft = apBits->miBitsLeft - aiCount;
        const s32 liResult   = static_cast<s32>(kauBitMask[aiCount] & luAccum);
        apBits->miBitsLeft   = liBitsLeft;

        const u32 luShifted  = luAccum >> aiCount;
        apBits->muAccum      = luShifted;

        if (liBitsLeft < 8)
        {
            const u8 luByte = *apBits->mpSource;
            apBits->miBitsLeft = liBitsLeft + 8;
            apBits->mpSource += 1;
            apBits->muAccum = (static_cast<u32>(luByte) << liBitsLeft) | luShifted;
        }
        return liResult;
    }

    // Decode one subframe of excitation, writing every aiStride-th sample of
    // apDest until 108 sample positions have been covered.
    //
    // aiCodebook != 0: walk the two-row variable-length code. Codes above 3 emit
    // their table value; codes 2..3 emit a zero run of getbits(6) + 7 samples
    // (clamped to the subframe); codes 0..1 emit a long magnitude (7 plus a unary
    // count of one-bits) with a trailing sign bit.
    // aiCodebook == 0: a ternary code read straight off the accumulator -- 01 is
    // -2, 11 is +2 (two bits each), anything with a clear low bit is 0 (one bit).
    void readsamples(CMTBLKDecf* apDec, s32 aiCodebook, f32* apDest, s32 aiStride)
    {
        MtBitReader* lpBits = &apDec->mReader;
        s32 liPos = 0;

        if (aiCodebook != 0)
        {
            s32 liRow = 0;
            do
            {
                const u32 luCode = kauMtLookup[liRow][lpBits->muAccum & 0xFFu];
                const MtCode& lrCode = kaMtCode[luCode];
                liRow = lrCode.miNextRow;
                discardbits(lpBits, lrCode.miBits);

                if (luCode > 3)
                {
                    apDest[liPos] = lrCode.mfValue;
                    liPos += aiStride;
                }
                else if (luCode > 1)
                {
                    s32 liRun = getbits(lpBits, 6) + 7;
                    if (liRun * aiStride + liPos > KI_SUBFRAME_SAMPLES)
                        liRun = (KI_SUBFRAME_SAMPLES - liPos) / aiStride;

                    for (s32 li = 0; li < liRun; ++li)
                    {
                        apDest[liPos] = 0.0f;
                        liPos += aiStride;
                    }
                }
                else
                {
                    s32 liMagnitude = 7;
                    while (getbits(lpBits, 1) == 1)
                        ++liMagnitude;

                    if (getbits(lpBits, 1) == 1)
                        apDest[liPos] = static_cast<f32>(liMagnitude);
                    else
                        apDest[liPos] = static_cast<f32>(-liMagnitude);
                    liPos += aiStride;
                }
            } while (liPos < KI_SUBFRAME_SAMPLES);
        }
        else
        {
            f32* lpDest = apDest;
            do
            {
                const u32 luBits = lpBits->muAccum & 3u;
                if (luBits == 1)
                {
                    *lpDest = -2.0f;
                    discardbits(lpBits, 2);
                }
                else if (luBits == 3)
                {
                    *lpDest = 2.0f;
                    discardbits(lpBits, 2);
                }
                else
                {
                    *lpDest = 0.0f;
                    discardbits(lpBits, 1);
                }
                liPos  += aiStride;
                lpDest += aiStride;
            } while (liPos < KI_SUBFRAME_SAMPLES);
        }
    }

    // Decode one frame into mafWindow.
    //
    // 1. Twelve reflection coefficients (four six-bit, eight five-bit); the first
    //    six-bit value also selects the excitation codebook (below miReadB -> the
    //    variable-length code). Each delta is a quarter of the change from the
    //    live coefficient.
    // 2. Four subframes of 108 samples: lag (8 bits, relative to the subframe's
    //    position in the 756-sample excitation history), pitch gain (4 bits,
    //    /15), excitation gain (6 bits, through mafScale), then the excitation --
    //    either full-rate, or decimated by two with a phase bit and a zero-fill
    //    bit; when not zero-filled the missing phase is rebuilt by a 6-tap
    //    half-band interpolator and the gain is halved. Output sample = excitation
    //    * gain + delayed history * pitch gain.
    // 3. Slide the last 324 window samples into the excitation history.
    // 4. Step the reflection coefficients four times, filtering 12, 12, 12 and
    //    396 samples with each step.
    void decodemut(CMTBLKDecf* apDec)
    {
        MtBitReader* lpBits = &apDec->mReader;

        const s32 liFirst = getbits(lpBits, 6);
        const s32 liCodebook = (liFirst < apDec->miReadB) ? 1 : 0;

        f32 lafDelta[KI_FILTER_ORDER];
        lafDelta[0] = (kafReflection[liFirst] - apDec->mafReflection[0]) * KF_REFLECTION_STEP;
        for (s32 li = 1; li < 4; ++li)
            lafDelta[li] = (kafReflection[getbits(lpBits, 6)] - apDec->mafReflection[li]) * KF_REFLECTION_STEP;
        for (s32 li = 4; li < KI_FILTER_ORDER; ++li)
            lafDelta[li] = (kafReflection[KI_REFLECTION_5BIT_BASE + getbits(lpBits, 5)] - apDec->mafReflection[li])
                         * KF_REFLECTION_STEP;

        // Five guard samples either side for the interpolator taps.
        f32 lafSamples[5 + KI_SUBFRAME_SAMPLES + 5];
        f32* lpSamples = &lafSamples[5];

        // mafExcitation and mafWindow form one contiguous history: subframe s reads
        // its delayed excitation from position 216 + 108*s - lag.
        const f32* lpHistory = apDec->mafExcitation;
        f32* lpOut = apDec->mafWindow;
        for (s32 liBase = 216; liBase < 648; liBase += KI_SUBFRAME_SAMPLES)
        {
            const s32 liDelayed = liBase - getbits(lpBits, 8);
            const f32 lfPitchGain = static_cast<f32>(getbits(lpBits, 4)) * KF_PITCH_GAIN_STEP;
            f32 lfGain = apDec->mafScale[getbits(lpBits, 6)];

            if (apDec->miReadA == 0)
            {
                readsamples(apDec, liCodebook, lpSamples, 1);
            }
            else
            {
                const s32 liPhase = getbits(lpBits, 1);
                const s32 liZeroFill = getbits(lpBits, 1);
                readsamples(apDec, liCodebook, lpSamples + liPhase, 2);

                if (liZeroFill != 0)
                {
                    for (s32 li = 0; li < KI_SUBFRAME_SAMPLES / 2; ++li)
                        lpSamples[(1 - liPhase) + 2 * li] = 0.0f;
                }
                else
                {
                    for (s32 li = 0; li < 5; ++li)
                    {
                        lafSamples[li] = 0.0f;
                        lafSamples[5 + KI_SUBFRAME_SAMPLES + li] = 0.0f;
                    }

                    for (s32 li = 0; li < KI_SUBFRAME_SAMPLES / 2; ++li)
                    {
                        f32* lpS = &lpSamples[(1 - liPhase) + 2 * li];
                        const f32 lfOuter = lpS[-5] + lpS[5];
                        const f32 lfMiddle = lpS[-3] + lpS[3];
                        const f32 lfInner = lpS[-1] + lpS[1];
                        *lpS = (lfOuter * KF_INTERP_TAP5 - lfMiddle * KF_INTERP_TAP3) + lfInner * KF_INTERP_TAP1;
                    }
                    lfGain = lfGain * 0.5f;
                }
            }

            const f32* lpDelayed = lpHistory + liDelayed;
            for (s32 li = 0; li < KI_SUBFRAME_SAMPLES; ++li)
                lpOut[li] = lpSamples[li] * lfGain + lpDelayed[li] * lfPitchGain;
            lpOut += KI_SUBFRAME_SAMPLES;
        }

        std::memcpy(apDec->mafExcitation, &apDec->mafWindow[KI_SUBFRAME_SAMPLES], sizeof(apDec->mafExcitation));

        steprefl(apDec, lafDelta);
        synthesisefilter(apDec, 0, 1);
        steprefl(apDec, lafDelta);
        synthesisefilter(apDec, 12, 1);
        steprefl(apDec, lafDelta);
        synthesisefilter(apDec, 24, 1);
        steprefl(apDec, lafDelta);
        synthesisefilter(apDec, 36, 33);
    }

    // Hand a coded MicroTalk block to the decoder.
    //
    // Store-for-store with the console:
    //   return -1 if apSource is null or miBlockRemaining is already non-zero.
    //   miBlockRemaining = aiRemaining; miCapacity = aiCapacity; miWindowCount = 0;
    //   mpBlock = apSource. In mode 1 only: miFrameFlag = (*apSource == 0xEE),
    //   then apSource++. Finally initmut(apSource, this, miInitFlag).
    s32 CMTBLKDecf::Feed(u8* apSource, s32 aiCapacity, s32 aiRemaining)
    {
        if (apSource == 0 || miBlockRemaining != 0)
            return -1;

        const s32 liMode = miMode;

        miBlockRemaining = aiRemaining;
        miCapacity       = aiCapacity;
        miWindowCount    = 0;
        mpBlock          = apSource;

        if (liMode == 1)
        {
            miFrameFlag = (*apSource == KU_FRAME_MARKER) ? 1 : 0;
            ++apSource;
        }

        initmut(apSource, this, miInitFlag);
        return 0;
    }

    // Stream decoded samples out of mafWindow into the caller's buffer, refilling
    // the window with decodemut() (and, in mode 1, re-parsing the per-frame
    // header) whenever it drains, until aiCount (clamped to miBlockRemaining)
    // samples have been produced.
    //
    // Store-for-store with the console:
    //   v4 = miBlockRemaining; return 0 if 0. mpOutCursor = *appDest.
    //   v7 = min(aiCount, v4); loop while v8(remaining-to-emit) > 0:
    //     chunk = min(v8, miWindowCount);
    //     memcpy(mpOutCursor, &mafWindow[432 - miWindowCount], 4*chunk);
    //     mpOutCursor += chunk; miWindowCount -= chunk; v8 -= chunk;
    //     if v8 <= 0 break; decodemut(this); miWindowCount = 432;
    //     if mode 1: re-prime the reader / re-parse the frame header.
    //   miBlockRemaining -= v7; return v7.
    s32 CMTBLKDecf::Decode(f32* const* appDest, s32 aiCount)
    {
        const s32 liBlockRemaining = miBlockRemaining;
        if (liBlockRemaining == 0)
            return 0;

        mpOutCursor = *appDest;

        s32 liTotal = aiCount;
        if (liTotal >= liBlockRemaining)
            liTotal = liBlockRemaining;

        s32 liLeft = liTotal;
        if (liLeft > 0)
        {
            s32 liChunk = miWindowCount;
            for (;;)
            {
                if (liLeft < liChunk)
                    liChunk = liLeft;

                std::memcpy(mpOutCursor,
                            &mafWindow[KI_WINDOW_SAMPLES - miWindowCount],
                            sizeof(f32) * static_cast<usize>(liChunk));

                mpOutCursor   += liChunk;
                miWindowCount -= liChunk;
                liLeft        -= liChunk;
                if (liLeft <= 0)
                    break;

                decodemut(this);
                miWindowCount = KI_WINDOW_SAMPLES;
                liChunk       = KI_WINDOW_SAMPLES;

                if (miMode == 1)
                {
                    const u8* lpBlock = mReader.mpSource - 1;
                    mpBlock = lpBlock;

                    if (miFrameFlag != 0)
                    {
                        // 4-byte big-endian frame header:
                        //   [0..1] sample count, [2..3] window offset.
                        miHeaderB = static_cast<s16>((lpBlock[0] << 8) | lpBlock[1]);
                        miHeaderA = static_cast<s16>((lpBlock[2] << 8) | lpBlock[3]);

                        const u8* lpVals = lpBlock + 4;
                        const u8* lpNext = lpBlock + (2 * miHeaderA + 4);
                        mReader.mpSource = lpNext;   // first of the two console stores
                        mpBlock          = lpNext;
                        mReader.mpSource = lpNext + 1;

                        f32* lpOut = &mafWindow[miHeaderB];
                        for (s16 liIdx = 0; liIdx < miHeaderA; ++liIdx)
                        {
                            const s16 liSample =
                                static_cast<s16>((lpVals[0] << 8) | lpVals[1]);
                            lpVals += 2;
                            *lpOut++ = static_cast<f32>(liSample);
                        }

                        miFrameFlag = 0;
                    }

                    if (*mpBlock == KU_FRAME_MARKER)
                        miFrameFlag = 1;

                    const u8* lpSrc = mReader.mpSource;
                    mReader.miBitsLeft = 8;
                    mReader.muAccum    = lpSrc[0];
                    mReader.mpSource   = lpSrc + 1;
                }
            }
        }

        miBlockRemaining -= liTotal;
        return liTotal;
    }

    // Prime the bit reader from apSource, then (when aiInit != 0) build the
    // per-block scale table and zero the filter state. Operates on apDec; the
    // `this` pointer is not touched by the body.
    //
    // Store-for-store with the console:
    //   reader.mpSource = apSource+1; reader.miBitsLeft = 8; reader.muAccum = *apSource.
    //   if aiInit:
    //     miReadA = getbits(1); miReadB = 32 - getbits(4);
    //     mafScale[0] = (getbits(4) + 1) * 8.0f;
    //     ratio = getbits(6) * 0.001f + 1.04f;
    //     for i in 1..63: mafScale[i] = mafScale[i-1] * ratio;   (63 iterations)
    //     zero mafReflection[12] and mafSynthHistory[12]; zero mafExcitation[324].
    void CMTBLKDecf::initmut(u8* apSource, CMTBLKDecf* apDec, s32 aiInit)
    {
        apDec->mReader.mpSource   = apSource + 1;
        apDec->mReader.miBitsLeft = 8;
        apDec->mReader.muAccum    = apSource[0];

        if (aiInit != 0)
        {
            apDec->miReadA = getbits(&apDec->mReader, 1);
            apDec->miReadB = 32 - getbits(&apDec->mReader, 4);

            const s32 liScale = getbits(&apDec->mReader, 4) + 1;
            apDec->mafScale[0] = static_cast<f32>(liScale) * 8.0f;

            const s32 liRatioBits = getbits(&apDec->mReader, 6);
            const f32 lfRatio = static_cast<f32>(liRatioBits) * 0.001f + 1.04f;
            for (s32 li = 1; li < 64; ++li)
                apDec->mafScale[li] = apDec->mafScale[li - 1] * lfRatio;

            for (s32 li = 0; li < KI_FILTER_ORDER; ++li)
            {
                apDec->mafReflection[li] = 0.0f;
                apDec->mafSynthHistory[li] = 0.0f;
            }

            for (s32 li = 0; li < 324; ++li)
                apDec->mafExcitation[li] = 0.0f;
        }
    }
} // namespace Snd
