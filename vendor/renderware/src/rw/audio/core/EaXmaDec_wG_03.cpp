// =====================================================================================
// rw::audio::core::EaXmaDec bodies (part 3 of the TU): the decode pump.
//
//   DecodeEvent       @0x82B96380
//   CopySamplesOut    @0x82B8FC18
//   EnterRecoveryMode @0x82B96148
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width, branch and side-effect. No Feb-2007 source and no DecFIGS DWARF exist
// for this TU. The pool side of the codec, the leaf bit-stream helpers and the static
// member definitions live in EaXmaDec.cpp; the instance lifecycle in EaXmaDec_wG_02.cpp;
// the input pump (Service / StartPair / Reset) in EaXmaDec_wG_04.cpp.
//
// CROSS-CUTTING NOTES (they recur below and are not repeated at every site):
//  * `_savegprlr_17` / `_restgprlr_17` in the listing are the compiler's register
//    save/restore helpers, not source-level calls, so they are dropped.
//  * `twllei rX, 0` immediately after a `divwu` is the PPC compiler's divide-by-zero trap
//    guard emitted for the plain C division; the divisions are written normally.
//  * `dcbt` / `dcbz` are cache-management hints with no architectural data effect here
//    (`dcbz` only primes lines the code then fully overwrites); they are not reproduced.
//  * The per-pair table is indexed with the console stride 0x1C in the asm (`addi r,r,0x1C`
//    walking in lockstep with the pair index). That literal is the CONSOLE sizeof(DecPair);
//    on the x64 host the record's pointers widen, so every access here is `mpPairs[index]`
//    -- reproducing the byte stride would walk off the table and corrupt memory.
//  * The decoded PCM sitting in a hardware output ring is produced by the XMA hardware
//    decoder, so it is read as NATIVE s16 (no byte swap). Only the encoded XMA stream words
//    that Service/StartPair pull out of request data are serialized big-endian.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"

#include "SDKs/XAudio/XmaHardware.h" // XmaContext + the XMA* hardware HAL

#include <cstring> // memset

// Platform tick source (XDK / Win32); declared in the established platform-API style, cf.
// SDKs/XCam/XCamPacketizer.h and SDKs/XCam/XCamQOSController.h.
extern "C" u32 GetTickCount();

namespace rw
{
namespace audio
{
namespace core
{

// s16 -> f32 scale, the rodata word flt_820ADBE8 (= 1/32768) the VMX path expresses as
// `vcfsx v,v,15` and the scalar tails multiply in explicitly. Same constant and name as
// the committed Pcm16BigDec.cpp.
static const f32 KF_PCM16_SCALE = 1.0f / 32768.0f;

// Decoded-PCM ring size, bytes (one hardware output buffer; 0x1800 in the asm, and the
// 0x18 = 6144/256 block count Reset programs into XmaContextInit).
static const s32 KI_OUTPUT_RING_BYTES = 6144;

// XMA frame length in samples; the request start/end sample arithmetic is quantised to it.
static const s32 KI_XMA_FRAME_SAMPLES = 512;

// Sample bias applied to a request that does not carry the "already aligned" flag
// (muReserved10): three quarters of a frame of decoder priming.
static const s32 KI_XMA_PRIME_SAMPLES = 384;

// -------------------------------------------------------------------------------------
// CopySamplesOut @0x82B8FC18
//
// Convert decoded s16 PCM out of one hardware output ring into planar f32, de-interleaving
// a stereo pair into pOutLeft/pOutRight (mono writes pOutLeft only and never touches
// pOutRight). The ring wrap is handled by two segments: `iFramesFirst` frames read from
// pRing + (iRingByteOffset & ~1), then `iFramesSecond` frames read from the ring base.
//
// A member in the binary (every call site passes the instance in r3) but the asm overwrites
// r3 before use, so `this` is unreferenced -- the same shape the committed XMAmemcpy has.
//
// DE-VECTORISATION JUSTIFICATION. The X360 body is hand-written VMX: 128-byte unaligned
// block loads (lvlx/lvrx + vor), a `vperm` de-interleave, vupkhsh/vupklsh sign-extension
// and `vcfsx v,v,15` fixed-point conversion, with stvlx/stvrx unaligned stores. The scalar
// loops below are NOT a paraphrase of that -- they are the binary's OWN remainder tails,
// which every vector loop falls through to:
//   * mono  tail @0x82B8FD60 / @0x82B8FE08: `lha` (signed s16 load) -> fcfid -> frsp ->
//     `fmuls f13, f13, flt_820ADBE8` -> `stfs`, cursor += 2, output += 4;
//   * stereo tail @0x82B900FC: two `lha`s from consecutive halfwords, the first stored to
//     r11 (pOutLeft), the second to r8 (pOutRight), both advancing by 4.
// and `vcfsx v,v,15` is exactly a multiply by 2^-15 = flt_820ADBE8, while the vperm mask
// built on the stack (00 01 04 05 08 09 0C 0D | 02 03 06 07 0A 0B 0E 0F, stored as the
// words 0x00010405/0x08090C0D/0x02030607/0x0A0B0E0F @0x82B8FEB4..) is exactly the stereo
// L/R de-interleave the scalar tail performs pairwise. So the per-sample math and the
// element order are attested identical by the same function; the vector loops are a
// throughput optimisation over them. Same treatment the committed XMAmemcpy applies to its
// pure VMX block copy.
//
// The ring holds hardware-decoder output, so the s16s are read natively (no byte swap):
// this is decoded PCM in memory, not serialized resource data.
// -------------------------------------------------------------------------------------
void EaXmaDec::CopySamplesOut(f32 *pOutLeft, f32 *pOutRight, const s16 *pRing,
                              s32 iRingByteOffset, s32 iNumChannels, s32 iFramesFirst,
                              s32 iFramesSecond)
{
    // `srawi r8, r7, 1; slwi r8, r8, 1` -- the byte offset is forced even (it indexes s16
    // frames) before being added to the ring base, in both the mono and the stereo path.
    const s16 *lpSource = reinterpret_cast<const s16 *>(
        reinterpret_cast<const u8 *>(pRing) + (iRingByteOffset & ~1));

    if (iNumChannels == 1)
    {
        // Mono is the asm's fall-through path (`cmpwi cr6, r8, 1; bne cr6, <stereo>`).
        for (s32 iFrame = 0; iFrame < iFramesFirst; ++iFrame)
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;

        lpSource = pRing; // segment 2 restarts at the ring base (`mr r9, r6`)
        for (s32 iFrame = 0; iFrame < iFramesSecond; ++iFrame)
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
    }
    else
    {
        for (s32 iFrame = 0; iFrame < iFramesFirst; ++iFrame)
        {
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
            *pOutRight++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
        }

        lpSource = pRing; // segment 2 restarts at the ring base (`mr r9, r6`)
        for (s32 iFrame = 0; iFrame < iFramesSecond; ++iFrame)
        {
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
            *pOutRight++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
        }
    }
}

// -------------------------------------------------------------------------------------
// EnterRecoveryMode @0x82B96148
//
// The hardware has stalled (DecodeEvent's wedge probe or its stall timeout fired). Latch
// recovery mode -- DecodeEvent then zero-fills its output instead of decoding -- re-arm
// every hardware context with the current stream mode, drop all per-pair streaming state,
// step the request read cursor one slot past the decode cursor, and re-prime the input
// pump.
// -------------------------------------------------------------------------------------
void EaXmaDec::EnterRecoveryMode()
{
    mbRecoveryMode = true;
    Reset(muStreamMode);

    // The asm walks the pair table by pointer, re-deriving `mpPairs + 0x1C * muPairCount`
    // as the loop bound on every iteration; neither operand is written in the loop, so the
    // index form below is the same walk (and avoids the console stride -- see the header
    // note). Store order is exactly the asm's.
    for (u32 uPair = 0; uPair < muPairCount; ++uPair)
    {
        DecPair &lPair = mpPairs[uPair];
        lPair.miInputBytesRemaining = 0;
        lPair.muRingReadOffset = 0;
        lPair.mbNextInputIsBuffer1 = false;
        lPair.muSamplesQueued = 0;
        lPair.mpInputCursor = 0;
    }

    // The asm stores mucRequestDecodeIndex into mucRequestReadIndex and then immediately
    // stores the incremented byte over it (@0x82B961B8 / @0x82B961CC) -- the intermediate
    // value is never observable, so the two stores are collapsed into one.
    mucRequestReadIndex = static_cast<u8>(mucRequestDecodeIndex + 1);
    if (mucRequestReadIndex >= mucRequestCount)
        mucRequestReadIndex = 0;

    miPendingInputBytes = 0;
    Service();
}

// -------------------------------------------------------------------------------------
// DecodeEvent @0x82B96380
//
// The decode pump, and the codec's DecoderDesc decode callback (desc slot +0x0C). Produces
// up to iNumSamples planar f32 samples per channel into pOutput, one request-chunk at a
// time: pull the next request when the current one is exhausted, drain each pair's hardware
// output ring through CopySamplesOut, keep the hardware read offset re-synced to the
// software cursor, pump Service when a ring is short, and fall into recovery mode (zero
// fill) when the hardware wedges or stalls past suRecoveryTimeoutMs. Returns the sample
// count produced.
//
// iCounted (r29) and iRecoveryReason (r18) are initialised ONCE at function entry, not per
// chunk: r29 carries the last chunk's produced-sample count into the next iteration's
// `iProduced += iCounted` when a chunk copies nothing, and r18 latches the recovery reason
// across chunks. Both are function-scope here to match.
// -------------------------------------------------------------------------------------
s32 EaXmaDec::DecodeEvent(DecoderBuffer *pOutput, s32 iNumSamples)
{
    if (!iNumSamples)
        return 0;

    s32 iProduced = 0;           // r20 -- total samples handed back
    s32 iCounted = 0;            // r29 -- samples the last chunk actually produced
    s32 iRecoveryReason = 0;     // r18 -- 1 = stall timeout, 2 = wedged context
    s32 iRemaining = iNumSamples; // r19

    do
    {
        if (!miSamplesRemaining)
        {
            // Pull the request at the decode cursor. The asm nulls the pointer when the
            // slot is empty (miEndSample == 0) and then dereferences it UNCONDITIONALLY
            // three instructions later (`lbz r11, 0x10(r7)` @0x82B963EC, `lwz r11, 8(r7)`
            // @0x82B96408). That is the shape the binary ships -- the null is only ever a
            // marker for the mpCurrentRequest store below, and the caller is expected never
            // to reach here with an empty slot -- so it is reproduced verbatim rather than
            // "fixed" with a guard the hardware build does not have.
            DecoderRequest *lpRequest = &RequestQueue()[mucRequestDecodeIndex];
            if (!lpRequest->miEndSample)
                lpRequest = 0;

            // ⭐ CONFIRMED AND NAMED 2026-08-28 (phase E). This TU had already deduced from
            // its own `lbz` that +0x10 is a single BYTE, and noted that the then-frozen
            // Decoder.h modelled it as the ring slot's word. Raw-decoding the ring's
            // producer, Decoder::Feed @0x82B67920, settles both the width (it writes it
            // with `stb`) and the meaning: the sound players pass "this is NOT a decoder
            // reset", i.e. mucContinue means the chunk continues the current stream.
            // That reading is exactly what this arithmetic wants -- a continuing stream is
            // already primed, and only a fresh one needs the XMA priming samples added.
            const s32 iBias = lpRequest->mucContinue ? 0 : KI_XMA_PRIME_SAMPLES;
            const s32 iStartSample = lpRequest->miStartSample;

            // Signed division (`srawi`+`addze`+`slwi`): the offset of the request's start
            // sample inside its 512-sample XMA frame, plus the bias carried over from the
            // previous request's tail.
            const s32 iQueueDelta =
                iStartSample -
                ((iStartSample + iBias) / KI_XMA_FRAME_SAMPLES) * KI_XMA_FRAME_SAMPLES +
                iBias + miOutputSampleBias;

            for (u32 uPair = 0; uPair < muPairCount; ++uPair)
                mpPairs[uPair].muSamplesQueued += static_cast<u32>(iQueueDelta);

            miOutputSampleBias = 0;
            mpCurrentRequest = lpRequest;
            mbRecoveryMode = false;
            miSamplesRemaining = lpRequest->miEndSample - lpRequest->miStartSample;
        }

        // This chunk: whatever is left of the request, capped by what the caller still wants.
        s32 iChunk = miSamplesRemaining; // r26
        if (iRemaining <= iChunk)
            iChunk = iRemaining;

        u32 uPairIndex = 0; // r24
        u32 uRetry = 0;     // r27 -- 0 or 1 only; see the tick anchor note below

        if (!mbRecoveryMode)
        {
            // The pair walk does not advance while uRetry is set: a pair whose ring was
            // short is re-examined after Service until it fills, wedges or times out.
            while (uPairIndex < muPairCount || uRetry)
            {
                DecPair &lPair = mpPairs[uPairIndex];
                XmaContext *lpContext = lPair.mpRecord->mpContext;

                // Bytes the hardware has written past the software read cursor. The write
                // offset is reported in 256-byte units (`slwi r11, r3, 8`).
                s32 iAvailableBytes = static_cast<s32>(XMAGetOutputBufferWriteOffset(lpContext) << 8) -
                                      static_cast<s32>(lPair.muRingReadOffset);
                const u32 uHardwareReadOffset = XMAGetOutputBufferReadOffset(lpContext);

                // Wrapped (or exactly full) ring: the writer is behind the reader in the
                // linear sense, so add a whole ring. Not on the very first decode, and not
                // when a zero difference is merely an invalidated (drained) buffer.
                if (iAvailableBytes <= 0 && !mbFirstDecode &&
                    (iAvailableBytes != 0 || !XMAIsOutputBufferValid(lpContext)))
                {
                    iAvailableBytes += KI_OUTPUT_RING_BYTES;
                }

                // Discard frames the request-pull queued up for skipping (the start-sample
                // offset inside the first XMA frame), as far as the ring allows.
                if (lPair.muSamplesQueued != 0 && iAvailableBytes != 0)
                {
                    const u32 uChannels = lPair.mucChannels;
                    const u32 uQueuedBytes = 2 * uChannels * lPair.muSamplesQueued;

                    u32 uSkipBytes = static_cast<u32>(iAvailableBytes);
                    if (uSkipBytes > uQueuedBytes)
                        uSkipBytes = uQueuedBytes;

                    const u32 uAdvanced = lPair.muRingReadOffset + uSkipBytes;
                    lPair.muRingReadOffset = uAdvanced;
                    if (uAdvanced >= static_cast<u32>(KI_OUTPUT_RING_BYTES))
                        lPair.muRingReadOffset = uAdvanced - KI_OUTPUT_RING_BYTES;

                    iAvailableBytes -= static_cast<s32>(uSkipBytes);
                    lPair.muSamplesQueued -= uSkipBytes / (2 * uChannels);
                }

                const s32 iChannels = lPair.mucChannels;
                if (iAvailableBytes >= 2 * iChannels * iChunk && iChunk != 0)
                {
                    const u32 uRingOffset = lPair.muRingReadOffset;
                    uRetry = 0;

                    // Frames available before the ring wraps; anything past that is read
                    // from the ring base as a second segment.
                    const s32 iFramesToWrap =
                        static_cast<s32>((static_cast<u32>(KI_OUTPUT_RING_BYTES) - uRingOffset) /
                                         static_cast<u32>(2 * iChannels));

                    s32 iFramesSecond = 0;
                    s32 iFramesFirst = iChunk;
                    if (iChunk > iFramesToWrap)
                    {
                        iFramesSecond = iChunk - iFramesToWrap;
                        iFramesFirst = iFramesToWrap;
                    }

                    // Planar destination for this pair's channels. A pair carries 1 or 2
                    // channels (CreateInstanceEvent), which is why two slots suffice; the
                    // console reserves a larger stack scratch array and fills mucChannels
                    // of it. NOTE the attested asymmetry: these pointers take NO iProduced
                    // offset, while the recovery memset below DOES -- so on a chunk that
                    // decodes, the second and later chunks of one call overwrite the first.
                    // The asm builds them from pOutput alone (@0x82B965D8..@0x82B96608).
                    f32 *alpChannelOut[2] = {0, 0};
                    if (lPair.mucChannels != 0)
                    {
                        f32 *lpChannel = pOutput->mpData + 2u * pOutput->muStride * uPairIndex;
                        for (u32 uChannel = 0; uChannel < lPair.mucChannels; ++uChannel)
                        {
                            alpChannelOut[uChannel] = lpChannel;
                            lpChannel += pOutput->muStride;
                        }
                    }

                    CopySamplesOut(alpChannelOut[0], alpChannelOut[1],
                                   reinterpret_cast<const s16 *>(lPair.mpRecord->mpOutputBuffer),
                                   static_cast<s32>(uRingOffset), iChannels, iFramesFirst,
                                   iFramesSecond);

                    if (iFramesSecond)
                        lPair.muRingReadOffset = static_cast<u32>(2 * lPair.mucChannels * iFramesSecond);
                    else
                        lPair.muRingReadOffset =
                            uRingOffset + static_cast<u32>(2 * lPair.mucChannels * iFramesFirst);

                    iCounted = iChunk;
                }
                else
                {
                    uRetry = 1;
                    Service();
                }

                // Re-sync the hardware read offset to the software cursor. The hardware
                // counts in subframes: 4 * (bytes / (channels * 1024)) * channels.
                const u32 uCurrentOffset = lPair.muRingReadOffset;
                u32 uNewReadOffset;
                if (uCurrentOffset == static_cast<u32>(KI_OUTPUT_RING_BYTES))
                {
                    uNewReadOffset = 0;
                    lPair.muRingReadOffset = 0;
                }
                else
                {
                    const u32 uChannels = lPair.mucChannels;
                    uNewReadOffset = 4 * (uCurrentOffset / (uChannels * 1024)) * uChannels;
                }

                if (uHardwareReadOffset != uNewReadOffset)
                {
                    XMADisableContext(lpContext, 1);
                    XMASetOutputBufferReadOffset(lpContext, uNewReadOffset);
                    XMASetOutputBufferValid(lpContext);
                    XMAEnableContext(lpContext);
                }

                if (uRetry)
                {
                    // PROBABLE SHIPPED BUG, reproduced: the elapsed-time anchor is uRetry
                    // itself, which is the constant 1 on this path, so the binary computes
                    // GetTickCount() - 1 (`subf r30, r27, r3` @0x82B966F8) rather than the
                    // difference from a start tick. The stall timeout below therefore fires
                    // on the very first retry (tick-1 >= 4 ms is true for any live tick).
                    // The asm has no other write to r27, so this is what shipped.
                    const u32 uElapsedMs = GetTickCount() - uRetry;

                    XMAEnableContext(lpContext);

                    // Hardware-wedge probe: a released context, or either input-buffer
                    // status byte with bit 0x80 set (`clrrwi.` keeps only that bit).
                    XmaContext *lpProbe = lPair.mpRecord->mpContext;
                    bool bWedged = true;
                    if (lpProbe && !(lpProbe->mucStatus08 & 0x80) && !(lpProbe->mucStatus0C & 0x80))
                        bWedged = false;

                    if (bWedged)
                    {
                        iRecoveryReason = 2;
                        EnterRecoveryMode();
                        break;
                    }

                    if (uElapsedMs >= suRecoveryTimeoutMs)
                        iRecoveryReason = 1;

                    if (iRecoveryReason)
                    {
                        EnterRecoveryMode();
                        break;
                    }
                    // Otherwise retry this same pair -- uPairIndex is deliberately not
                    // advanced (the asm branches straight back to the loop head).
                }
                else
                {
                    ++uPairIndex;
                }
            }
        }

        if (mbRecoveryMode)
        {
            // Nothing was decoded: claim the whole chunk and hand back silence. Unlike the
            // copy path above, this one DOES bias the destination by iProduced -- the
            // asymmetry is in the binary (`add r11, r11, r20` @0x82B9679C).
            iCounted = iChunk;
            for (u32 uChannel = 0; uChannel < mucChannelCount; ++uChannel)
            {
                std::memset(pOutput->mpData + pOutput->muStride * uChannel + iProduced, 0,
                            static_cast<usize>(iChunk) * sizeof(f32));
            }
        }

        iProduced += iCounted;
        iRemaining -= iChunk;
        miSamplesRemaining -= iChunk;

        // Request boundary: carry the alignment slack of the request's end sample into the
        // next request, so the next start lands on an XMA frame boundary again.
        if (miSamplesRemaining == 0 && iChunk != 0)
        {
            DecoderRequest *lpRequest = mpCurrentRequest;
            s32 iEndSample = lpRequest->miEndSample;
            // Same rule as above: only a chunk that does NOT continue the current stream
            // carries the XMA priming samples (see Decoder::Feed's mucContinue).
            if (!lpRequest->mucContinue)
                iEndSample += KI_XMA_PRIME_SAMPLES;

            const s32 iFramePhase = iEndSample & (KI_XMA_FRAME_SAMPLES - 1); // `clrlwi. r11,r11,23`
            if (iFramePhase)
                miOutputSampleBias = KI_XMA_FRAME_SAMPLES - iFramePhase;

            if (mbRecoveryMode)
                miOutputSampleBias = 0;
        }

        Service();
    } while (iRemaining);

    return iProduced;
}

} // namespace core
} // namespace audio
} // namespace rw
