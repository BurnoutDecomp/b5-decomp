// =====================================================================================
// rw::audio::core::DelayLine bodies -- the multi-channel ring-buffer delay line behind the
// rw::audio::core::Delay plug-in and rw::audio::core::ReverbModel1 reverb.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for the stores, cursors and branches. LegacySrc delayline.h gives
// the SampleBuffer interface and the delay and crossfade constants.
//
// The `_savegprlr_* / _restgprlr_*` calls in the pseudocode are the compiler's register
// save/restore prologue/epilogue helpers -- not source-level calls -- so they are dropped.
// The `__twllei/__twlgei` traps are the PPC signed-divide guard instructions the compiler
// emits around `%` / `divw`; they are dropped and the plain modulo/division restored.
// XMemCpy is the Xbox platform interleaved block copy (declared here, defined in the
// platform TU, same as Decoder/Collection). off_83271928 is the shared rwaudio System
// singleton; the ring is taken from / returned to its ICoreAllocator at System+0x14 (the
// asm dispatches vtable[1] = Alloc(size,name,flags,align,offset) and vtable[3] = Free) --
// the same singleton Collection.cpp / Decoder.cpp reach.
// =====================================================================================

#if defined(_XBOX) && defined(_MSC_VER)
// Size optimization reproduces ARTIST Release at 0x82B6CC08 and preserves the other matches.
#pragma optimize("s", on)
#endif

#include "rw/audio/core/DelayLine.h"
#include "rw/audio/core/PlugIn.h"

#include "coreallocator/icoreallocator_interface.h" // EA::Allocator::ICoreAllocator

namespace rw
{
namespace audio
{
namespace core
{

// Xbox platform interleaved block copy (declaration only for the per-TU compile gate).
extern "C" void *XMemCpy(void *pDest, const void *pSrc, unsigned int uiCount);

extern "C" System *off_83271928;

namespace
{
// The debug tag every DelayLine ring allocation carries (verbatim X360 rodata).
const char KA_DELAY_BUFFER[] = "rw::audio::core::DelayLine::DelayBuffer";

// flt_82001CC0 == 0.0f (the zero the clean loops store).
const f32 KF_ZERO = 0.0f;
// The crossfade ramp seed / step: flt_8214B27C == 0.9921875 (127/128) and
// flt_8214B278 == 0.0078125 (1/128). The 128-entry ramp descends 127/128 .. 0.
const s32 KI_RAMP_LEN   = 128;
} // namespace

// -------------------------------------------------------------------------------------
// DelayLine::DelayLine @0x82B67EE8 -- clear every book-keeping field (15 words + the flag).
// -------------------------------------------------------------------------------------
DelayLine::DelayLine()
    : mpBuffer(0)
    , mpFilter(0)
    , mpLocalBuffer(0)
    , miMaxDelaySamples(0)
    , miReadLength(0)
    , miCapacity(0)
    , miGuardSamples(0)
    , miLocalBufferSamples(0)
    , miAvailableSamples(0)
    , miCleanCursor(0)
    , miReadPosition(0)
    , miFilterReadPosition(0)
    , miChannelCount(0)
    , miWritePhase(0)
    , mbFilterChanged(0)
{
}

// -------------------------------------------------------------------------------------
// DelayLine::~DelayLine @0x82B6E498 -- tail-calls Release.
// -------------------------------------------------------------------------------------
DelayLine::~DelayLine()
{
    Release();
}

// -------------------------------------------------------------------------------------
// DelayLine::SetFilter @0x82B67F30 -- install the unit filter dispatched by ApplyFilter.
// -------------------------------------------------------------------------------------
// ARTIST 82B67F38: retain the previous delay for the next crossfade.
void DelayLine::SetDelay(s32 delaySamples)
{
    if (delaySamples != miReadPosition)
    {
        miFilterReadPosition = miReadPosition;
        miReadPosition = delaySamples;
        mbFilterChanged = 1;
    }
}

void DelayLine::SetFilter(IFilter *pFilter)
{
    mpFilter = pFilter; // stw @ +0x04
}

// -------------------------------------------------------------------------------------
// DelayLine::SetLocalBuffer @0x82B67F58 -- point at the external scratch buffer + length.
// -------------------------------------------------------------------------------------
void DelayLine::SetLocalBuffer(f32 *pLocalBuffer, s32 samples)
{
    mpLocalBuffer = pLocalBuffer;    // stw @ +0x08
    miLocalBufferSamples = samples;  // stw @ +0x1C
}

// -------------------------------------------------------------------------------------
// DelayLine::GetValidDelaySamples @0x82B67F68 -- the available delay clamped to the max.
// -------------------------------------------------------------------------------------
s32 DelayLine::GetValidDelaySamples() const
{
    s32 valid = miAvailableSamples;
    const s32 maximum = miMaxDelaySamples;
    if (valid >= maximum)
        valid = maximum;
    return valid;
}

// -------------------------------------------------------------------------------------
// DelayLine::Init @0x82B6C968 -- size the ring. The capacity holds the (32-aligned) max
// delay plus a (32-aligned) read-length head-room; the ring is `4 * capacity * channels`
// bytes tagged "rw::audio::core::DelayLine::DelayBuffer", 128-aligned, perm memory.
// -------------------------------------------------------------------------------------
bool DelayLine::Init(s32 channels, s32 maxDelaySamples, s32 readLength)
{
    // The effective delay must leave room for a full read window.
    s32 delay = maxDelaySamples;
    if (delay <= readLength + 255)
        delay = readLength + 255;

    // capacity = roundUp32(delay + 32) + roundDown32(readLength + 30).
    const u32 capacity = ((delay + 32) & 0xFFFFFFE0u) + ((readLength + 30) & 0xFFFFFFE0u);

    void *pRing = 0;
    bool  ok = true;
    if (delay)
    {
        pRing = off_83271928->mpAllocator->Alloc(4u * capacity * static_cast<u32>(channels),
                                   KA_DELAY_BUFFER, EA::Allocator::kFlagPermMemory, 128, 0);
        ok = (pRing != 0);
    }

    if (ok)
    {
        miMaxDelaySamples = delay;                    // +0x0C
        miReadLength = readLength;                    // +0x10
        miGuardSamples = 0;                           // +0x18
        miWritePhase = 0;                             // +0x34
        miChannelCount = channels;                    // +0x30
        miCapacity = static_cast<s32>(capacity);      // +0x14
        miAvailableSamples = static_cast<s32>(capacity); // +0x20
        mpBuffer = static_cast<f32 *>(pRing);         // +0x00
    }
    return ok;
}

// -------------------------------------------------------------------------------------
// DelayLine::Reset @0x82B68068 -- prime the cursors for a fresh run. Read cursor starts at
// `readPosition` samples of delay; the available/valid count and write phase reset to the
// full capacity / guard, and the clean cursor + filter-changed flag clear.
// -------------------------------------------------------------------------------------
void DelayLine::Reset(s32 readPosition)
{
    const s32 capacity = miCapacity;    // +0x14
    const s32 guard = miGuardSamples;   // +0x18
    miReadPosition = readPosition;      // +0x28
    miCleanCursor = 0;                  // +0x24
    miAvailableSamples = capacity;      // +0x20
    miWritePhase = guard;               // +0x34
    mbFilterChanged = 0;               // +0x38
}

// -------------------------------------------------------------------------------------
// DelayLine::CalcChannelPointers @0x82B68090 -- resolve the four ring pointers for
// `channel` at write `offset`. The channel occupies [start, end) at a miCapacity stride;
// the loop-end trims the guard tail; the cursor is the wrapped write position + guard.
// -------------------------------------------------------------------------------------
// ARTIST calls this helper out of line from both marshalling routines and Resize.
__declspec(noinline) void DelayLine::CalcChannelPointers(ChannelPointers *pOut, s32 channel, s32 offset) const
{
    f32 *const pStart = mpBuffer + static_cast<usize>(channel) * miCapacity; // +0x00
    pOut->mpStart = pStart;
    pOut->mpEnd = pStart + miCapacity;                                        // +0x04
    pOut->mpLoopEnd = pOut->mpEnd - miGuardSamples;                           // +0x08
    // Wrapped write cursor: ((writePhase + offset) mod capacity) + guard samples in.
    const s32 wrapped = (miWritePhase + offset) % miCapacity;
    pOut->mpCursor = pStart + (wrapped + miGuardSamples);                     // +0x0C
}

// -------------------------------------------------------------------------------------
// DelayLine::ReadData @0x82B6CC78 -- copy up to `count` delayed samples ending `backOffset`
// before the cursor out of the ring into `pDest`, wrapping at the ring end. Returns the
// number of samples produced (min(count, backOffset)).
// -------------------------------------------------------------------------------------
s32 DelayLine::ReadData(const ChannelPointers *pPtrs, f32 *pDest, s32 backOffset,
                        s32 count) const
{
    if (count == 0)
        return 0;

    count = count < backOffset ? count : backOffset;

    const f32 *pSrc = pPtrs->mpCursor - backOffset;
    if (pSrc < pPtrs->mpStart || pPtrs->mpEnd <= pSrc)
        pSrc += (miCapacity - miGuardSamples); // wrap into the ring body

    const s32 untilEnd = static_cast<s32>(pPtrs->mpEnd - pSrc);
    const s32 first = count < untilEnd ? count : untilEnd;
    const s32 remaining = count - first;
    f32 *const pNext = pDest + first;
    XMemCpy(pDest, pSrc, sizeof(*pSrc) * first);
    XMemCpy(pNext, pPtrs->mpStart, sizeof(*pSrc) * remaining);
    return count;
}

// -------------------------------------------------------------------------------------
// DelayLine::WriteData @0x82B6CD30 -- copy `count` samples from `pSrc` into the ring ending
// `backOffset` before the cursor, wrapping at the ring end. A no-op when `count` would
// exceed the whole channel span.
// -------------------------------------------------------------------------------------
void DelayLine::WriteData(const ChannelPointers *pPtrs, const f32 *pSrc, s32 backOffset,
                          s32 count) const
{
    f32 *pDst = pPtrs->mpCursor - backOffset;
    if (pDst < pPtrs->mpStart || pPtrs->mpEnd <= pDst)
        pDst += (miCapacity - miGuardSamples); // wrap into the ring body

    if (count < static_cast<s32>(pPtrs->mpEnd - pPtrs->mpStart))
    {
        const s32 untilEnd = static_cast<s32>(pPtrs->mpEnd - pDst);
        const s32 first = count < untilEnd ? count : untilEnd;
        const s32 remaining = count - first;
        const f32 *const pNext = pSrc + first;
        XMemCpy(pDst, pSrc, sizeof(*pSrc) * first);
        XMemCpy(pPtrs->mpStart, pNext, sizeof(*pSrc) * remaining);
    }
}

// -------------------------------------------------------------------------------------
// DelayLine::LoadTaps @0x82B6CDD8 -- read the delay taps described by `pTaps` into the
// local buffer. With two taps the longer one is served first (miSelect picks the order);
// each record's data is read via ReadData and its landing pointer written back to mpOut.
// Returns the end of the loaded data in the local buffer.
// -------------------------------------------------------------------------------------
f32 *DelayLine::LoadTaps(const ChannelPointers *pPtrs, TapRecord *pTaps, s32 tapCount) const
{
    pTaps[0].miSelect = 0;
    if (tapCount == 2)
    {
        const bool firstLonger = pTaps[0].miLength >= pTaps[1].miLength;
        pTaps[0].miSelect = firstLonger ? 0 : 1;
        pTaps[1].miSelect = firstLonger ? 1 : 0;
    }

    f32 *pDest = mpLocalBuffer;
    // Running available window, seeded from the selected record's 32-aligned length.
    s32 avail = (pTaps[pTaps[0].miSelect].miLength + 31) & ~31;

    for (s32 tap = 0; tap < tapCount; ++tap)
    {
        TapRecord *const pRec = &pTaps[pTaps[tap].miSelect];
        const s32 lenRounded = (pRec->miLength + 31) & ~31;
        const s32 pad = lenRounded - pRec->miLength;
        const s32 len2Rounded = (pRec->miLength2 + pad + 31) & ~31;

        s32 read;
        if (lenRounded > avail)
        {
            s32 span = avail - (lenRounded - len2Rounded);
            pRec->mpOut = pDest + (pad - lenRounded + avail);
            if (span < 0)
                span = 0;
            read = ReadData(pPtrs, pDest, avail, span);
            avail += read;
        }
        else
        {
            pRec->mpOut = pDest + pad;
            read = ReadData(pPtrs, pDest, lenRounded, len2Rounded);
            avail = lenRounded - read;
        }
        pDest += read;
    }
    return pDest;
}

// -------------------------------------------------------------------------------------
// DelayLine::IncrementalClean @0x82B67F88 -- zero the not-yet-written head-room of the
// freshly marshalled tap buffers (pDelayFull and, during a crossfade, pDelayCross) so a
// partially-filled ring reads as silence rather than stale data.
// -------------------------------------------------------------------------------------
void DelayLine::IncrementalClean(s32 numMarshaled, s32 numProcessed, IFilter::LocationParams *pParams)
{
    if (miCleanCursor >= miCapacity)
        return;

    s32 numToClean = miReadPosition - miCleanCursor - numProcessed;
    const s32 maxClean = numMarshaled + miReadLength - 1;
    if (numToClean >= maxClean)
        numToClean = maxClean;
    if (numToClean <= 0)
        numToClean = 0;
    for (s32 sample = 0; sample < numToClean; ++sample)
    {
        // ARTIST reloads the tap pointer each iteration at 0x82B67FDC.
        f32 *pDelayFull = pParams->pDelayFull;
        pDelayFull[sample] = KF_ZERO;
    }

    if (pParams->pDelayCross)
    {
        const s32 readLength = miReadLength;
        s32 numCrossToClean = miFilterReadPosition - numProcessed - miCleanCursor;
        if (numCrossToClean >= readLength + KI_RAMP_LEN - 1)
            numCrossToClean = readLength + KI_RAMP_LEN - 1;
        const s32 maxCrossClean = readLength + numMarshaled - 1;
        if (numCrossToClean >= maxCrossClean)
            numCrossToClean = maxCrossClean;
        if (numCrossToClean <= 0)
            numCrossToClean = 0;
        for (s32 sample = 0; sample < numCrossToClean; ++sample)
            pParams->pDelayCross[sample] = KF_ZERO;
    }
}

// -------------------------------------------------------------------------------------
// DelayLine::MarshalDelayData @0x82B6CF10 -- gather the delayed input for `channel` at
// write `offset` into the local buffer: build the ring pointers, describe the primary tap
// (and, on a filter change, a second cross-fade tap capped at 128), load them, publish the
// results into the context, then incrementally clean the head-room. Returns `count`.
// -------------------------------------------------------------------------------------
s32 DelayLine::MarshalDelayData(s32 channel, s32 count, s32 offset, TapContext *pCtx)
{
    ChannelPointers ptrs;
    TapRecord taps[2];

    CalcChannelPointers(&ptrs, channel, offset);

    s32 tapCount = 1;
    taps[0].mpOut = 0;
    taps[0].miLength = miReadPosition;
    taps[0].miLength2 = miReadLength + count - 1;

    if (mbFilterChanged)
    {
        taps[1].miLength = miFilterReadPosition;
        const s32 window = count < KI_RAMP_LEN ? count : KI_RAMP_LEN;
        taps[1].mpOut = 0;
        tapCount = 2;
        taps[1].miLength2 = miReadLength + window - 1;
    }

    f32 *const pLoadedEnd = LoadTaps(&ptrs, taps, tapCount);
    pCtx->pDelayFull = taps[0].mpOut;      // a5[1]
    pCtx->pDelayZero = pLoadedEnd;   // a5[4]
    pCtx->pDelayCross = mbFilterChanged ? taps[1].mpOut : 0; // a5[2]

    IncrementalClean(count, offset, pCtx);
    return count;
}

// -------------------------------------------------------------------------------------
// DelayLine::UnmarshalDelayData @0x82B6CFE8 -- write the last `count` filtered samples from
// the loaded tap data back into the ring at the current write cursor.
// -------------------------------------------------------------------------------------
void DelayLine::UnmarshalDelayData(s32 channel, s32 count, TapContext *pCtx)
{
    ChannelPointers ptrs;
    CalcChannelPointers(&ptrs, channel, 0);
    WriteData(&ptrs, pCtx->pDelayZero - count, 0, count);
}

// -------------------------------------------------------------------------------------
// DelayLine::ApplyFilter @0x82B6E4A0 -- the per-frame process. For each channel it marshals
// the delayed input, then dispatches the installed filter over the samples: the frame after
// a filter change it first runs a crossfade chunk (<=128) through the descending coefficient
// ramp, then the remainder with no ramp; each chunk advances the whole context. After every
// channel the ring cursors advance one frame and the filter-changed flag clears.
// -------------------------------------------------------------------------------------
void DelayLine::ApplyFilter(s32 count, SampleBuffer *pInput,
                            SampleBuffer *pOutput, IFilter::FilterOutputMode mode)
{
    // Descending crossfade coefficients 127/128 .. 0 (only built when a filter change is
    // pending; ApplyFilter re-tests the flag per channel below).
    f32 ramp[KI_RAMP_LEN];
    if (mbFilterChanged)
    {
        f32 coeff = 127.0f / 128.0f;
        for (s32 k = 0; k < KI_RAMP_LEN; ++k)
        {
            ramp[k] = coeff;
            coeff -= 1.0f / 128.0f;
        }
    }

    for (s32 channel = 0; channel < miChannelCount; ++channel)
    {
        s32 rampRemaining = 0;

        TapContext ctx;
        ctx.pInput = pInput->LockChannel(channel);
        ctx.pOutput = pOutput->LockChannel(channel);
        ctx.pDelayCross = 0;
        ctx.pCrossFadeCoeff = 0;
        if (mbFilterChanged)
        {
            rampRemaining = KI_RAMP_LEN;
            ctx.pCrossFadeCoeff = ramp;
        }

        for (s32 i = 0; i < count; UnmarshalDelayData(channel, i, &ctx))
        {
            const s32 total = MarshalDelayData(channel, count, i, &ctx);
            s32 remainder = total;

            if (rampRemaining)
            {
                const s32 chunk = rampRemaining < total ? rampRemaining : total;

                ctx.pCrossFadeCoeff = ramp + (KI_RAMP_LEN - rampRemaining);
                mpFilter->mpApplyFunc(mpFilter, chunk, mode, channel, &ctx);

                i += chunk;
                rampRemaining -= chunk;
                ctx.pInput += chunk;
                ctx.pDelayFull += chunk;
                ctx.pDelayCross += chunk;
                ctx.pCrossFadeCoeff += chunk;
                ctx.pDelayZero += chunk;
                ctx.pOutput += chunk;
                remainder = total - chunk;
            }

            if (remainder)
            {
                ctx.pDelayCross = 0;
                ctx.pCrossFadeCoeff = 0;
                mpFilter->mpApplyFunc(mpFilter, remainder, mode, channel, &ctx);

                i += remainder;
                ctx.pInput += remainder;
                ctx.pDelayFull += remainder;
                ctx.pDelayCross += remainder;
                ctx.pCrossFadeCoeff += remainder;
                ctx.pDelayZero += remainder;
                ctx.pOutput += remainder;
            }
        }
    }

    // Store the wrapped position before clamping the guard (ARTIST 0x82B6E70C).
    // The clean cursor and readable sample count both saturate at capacity.
    s32 capacity = miCapacity;
    s32 writePhase = (miWritePhase + count) % capacity;
    miWritePhase = writePhase;
    if (writePhase <= miGuardSamples)
        writePhase = miGuardSamples;
    miWritePhase = writePhase;

    s32 clean = miCleanCursor + count;
    if (clean >= capacity)
        clean = capacity;
    miCleanCursor = clean;

    const s32 avail = miAvailableSamples + count;
    if (avail < capacity)
        capacity = avail;
    miAvailableSamples = capacity;
    mbFilterChanged = 0;
}

// -------------------------------------------------------------------------------------
// DelayLine::Resize @0x82B6CA38 -- grow the ring to hold `maxDelaySamples`. With no buffer
// yet it forwards to Init. Otherwise, only when the new capacity exceeds the current one, a
// fresh ring is allocated, each channel's live samples are migrated into the wider stride
// (head + wrap + guard copies), the old ring is freed, and the capacity/write phase update.
// Returns false only on allocation failure.
// -------------------------------------------------------------------------------------
bool DelayLine::Resize(s32 maxDelaySamples)
{
    if (!mpBuffer)
        return Init(miChannelCount, maxDelaySamples, miReadLength);

    bool ok = true;
    const s32 newCapacity = ((maxDelaySamples + 32) & ~31) + miGuardSamples;

    if (miCapacity < newCapacity)
    {
        f32 *const pNew = static_cast<f32 *>(
            off_83271928->mpAllocator->Alloc(4u * static_cast<u32>(miChannelCount) * static_cast<u32>(newCapacity),
                               KA_DELAY_BUFFER, EA::Allocator::kFlagPermMemory, 128, 0));
        if (!pNew)
            return false;

        f32 *pChannelDst = pNew;
        for (s32 channel = 0; channel < miChannelCount; ++channel)
        {
            ChannelPointers old;
            CalcChannelPointers(&old, channel, 0);

            const s32 clean = miCleanCursor; // +0x24
            const f32 *pSrc = old.mpCursor - clean;
            if (pSrc < old.mpStart || old.mpEnd <= pSrc)
                pSrc += (miCapacity - miGuardSamples);

            // The migrated body lands `newCapacity - guard` samples into the new channel.
            f32 *const pBody = pChannelDst + (newCapacity - miGuardSamples);
            f32 *const pBodyStart = pBody - clean;

            s32 first = clean;
            const s32 tail = static_cast<s32>(old.mpLoopEnd - pSrc);
            if (clean >= tail)
                first = tail;

            XMemCpy(pBodyStart, pSrc, static_cast<u32>(4 * first));
            XMemCpy(pBodyStart + first, old.mpStart, static_cast<u32>(4 * (miCleanCursor - first)));
            XMemCpy(pChannelDst, pBody, static_cast<u32>(4 * miGuardSamples));

            pChannelDst += newCapacity;
        }

        if (mpBuffer)
            off_83271928->mpAllocator->Free(mpBuffer, 0);

        miWritePhase = miGuardSamples; // +0x34 <- old +0x18
        mpBuffer = pNew;               // +0x00
        miCapacity = newCapacity;      // +0x14
    }

    miMaxDelaySamples = maxDelaySamples; // +0x0C
    return ok;
}

// -------------------------------------------------------------------------------------
// DelayLine::Release @0x82B6CC08 -- free the ring back to the System allocator and clear
// the buffer / filter / local pointers.
// -------------------------------------------------------------------------------------
void DelayLine::Release()
{
    if (mpBuffer)
    {
        off_83271928->mpAllocator->Free(mpBuffer, 0);
        mpBuffer = 0;
    }
    mpLocalBuffer = 0; // +0x08
    mpFilter = 0;      // +0x04
    miMaxDelaySamples = 0; // +0x0C
}

} // namespace core
} // namespace audio
} // namespace rw

namespace rw { namespace audio { namespace core {
} } }
