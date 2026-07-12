// ============================================================================
// SDKs/EATech/include/snd/CMTBLKDecf.cpp
//
// Out-of-line definitions for the MicroTalk (MT) block decoder:
//   - Snd::discardbits          @ 0x82B79600
//   - Snd::CMTBLKDecf::Feed      @ 0x82B7A3B8
//   - Snd::CMTBLKDecf::Decode    @ 0x82B84F28
//   - Snd::CMTBLKDecf::initmut   @ 0x82B79EC0
// Reconstructed from the X360 assembly. Cited by X360 address only -- no
// leaked-source provenance for this TU.
// ============================================================================

#include "SDKs/EATech/include/snd/CMTBLKDecf.h"

#include <cstring>  // memcpy

namespace Snd
{
    // Frame count staged into the window per decodemut() refill, and the coded
    // stride the window streams at. Both attested as 0x1B0 in the Decode asm.
    static const s32 KI_WINDOW_SAMPLES = 432;

    // Leading byte value that flags the start of the next MT frame (0xEE).
    static const u8 KU_FRAME_MARKER = 0xEE;

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

    // 0x82B7A3B8. Hand a coded MicroTalk block to the decoder.
    //
    // Store-for-store with the X360:
    //   cmplwi r4 -> return -1 if apSource is null.
    //   lwz +0xD54 -> return -1 if miBlockRemaining already non-zero.
    //   lwz +0xD64 mode; stw a4 -> +0xD54; stw a3 -> +0xD58; stw 0 -> +0xD44;
    //   stw a2 -> +0xD48. In mode 1 only: miFrameFlag = (*apSource == 0xEE),
    //   then apSource++. Finally initmut(this, apSource, this, miInitFlag).
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

    // 0x82B84F28. Stream decoded samples out of mafWindow into the caller's buffer,
    // refilling the window with decodemut() (and, in mode 1, re-parsing the per-frame
    // header) whenever it drains, until aiCount (clamped to miBlockRemaining) samples
    // have been produced.
    //
    // Store-for-store with the X360:
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
                        mReader.mpSource = lpNext;   // first of the two X360 stores
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

    // 0x82B79EC0. Prime the bit reader from apSource, then (when aiInit != 0) build
    // the per-block scale table and zero the working buffers. Operates on apDec (the
    // X360 r5); the r3 "this" is not touched by the body.
    //
    // Store-for-store with the X360:
    //   reader.mpSource = apSource+1; reader.miBitsLeft = 8; reader.muAccum = *apSource.
    //   if aiInit:
    //     miReadA = getbits(1); miReadB = 32 - getbits(4);
    //     mafScale[0] = (getbits(4) + 1) * 8.0f;
    //     ratio = getbits(6) * 0.001f + 1.04f;
    //     for i in 1..63: mafScale[i] = mafScale[i-1] * ratio;   (63 iterations)
    //     zero mafGainA[12] and mafGainB[12]; zero mauState[324].
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

            for (s32 li = 0; li < 12; ++li)
            {
                apDec->mafGainA[li] = 0.0f;
                apDec->mafGainB[li] = 0.0f;
            }

            for (s32 li = 0; li < 324; ++li)
                apDec->mauState[li] = 0;
        }
    }
} // namespace Snd
