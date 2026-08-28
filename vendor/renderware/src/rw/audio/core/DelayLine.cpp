// =====================================================================================
// rw::audio::core::DelayLine bodies -- the multi-channel ring-buffer delay line behind the
// rw::audio::core::Delay plug-in and rw::audio::core::ReverbModel1 reverb.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store, cursor and branch. No Feb-2007 leak source and no DecFIGS
// DWARF exist. See DelayLine.h for the byte-exact layout and the per-function addresses.
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

#include "rw/audio/core/DelayLine.h"

#include "coreallocator/icoreallocator_interface.h" // EA::Allocator::ICoreAllocator

namespace rw
{
namespace audio
{
namespace core
{

// Xbox platform interleaved block copy (declaration only for the per-TU compile gate).
extern "C" void *XMemCpy(void *pDest, const void *pSrc, unsigned int uiCount);

namespace
{
// The shared rwaudio System singleton (off_83271928). Its full type lives in the System
// sub-system TU; the only field DelayLine touches is the ICoreAllocator pointer at +0x14,
// so it is reached through this minimal reconstructed view (identical to the one
// Collection.cpp uses). FLAGGED: the object itself is defined/initialised elsewhere.
struct AudioSystemView
{
    char                          mHeader[0x14]; // +0x00 .. +0x13 -- opaque header
    EA::Allocator::ICoreAllocator *mpAllocator;  // +0x14 -- block allocator slot
};

extern "C" AudioSystemView *off_83271928; // pointer-to-System global (System TU owns it)

inline EA::Allocator::ICoreAllocator *Allocator()
{
    return off_83271928->mpAllocator;
}

// The debug tag every DelayLine ring allocation carries (verbatim X360 rodata).
const char KA_DELAY_BUFFER[] = "rw::audio::core::DelayLine::DelayBuffer";

// flt_82001CC0 == 0.0f (the zero the clean loops store).
const f32 KF_ZERO = 0.0f;
// The crossfade ramp seed / step: flt_8214B27C == 0.9921875 (127/128) and
// flt_8214B278 == 0.0078125 (1/128). The 128-entry ramp descends 127/128 .. 0.
const f32 KF_RAMP_START = 0.9921875f;
const f32 KF_RAMP_STEP  = 0.0078125f;
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
    if (miAvailableSamples >= miMaxDelaySamples) // +0x20 vs +0x0C
        return miMaxDelaySamples;
    return miAvailableSamples;
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
        pRing = Allocator()->Alloc(4u * capacity * static_cast<u32>(channels),
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
void DelayLine::CalcChannelPointers(ChannelPointers *pOut, s32 channel, s32 offset) const
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

    s32 produced = count;
    if (count >= backOffset)
        produced = backOffset;

    const f32 *pSrc = pPtrs->mpCursor - backOffset;
    if (pSrc < pPtrs->mpStart || pPtrs->mpEnd <= pSrc)
        pSrc += (miCapacity - miGuardSamples); // wrap into the ring body

    s32 first = static_cast<s32>(pPtrs->mpEnd - pSrc);
    if (produced < first)
        first = produced;

    XMemCpy(pDest, pSrc, static_cast<u32>(4 * first));
    XMemCpy(pDest + first, pPtrs->mpStart, static_cast<u32>(4 * (produced - first)));
    return produced;
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
        s32 first = static_cast<s32>(pPtrs->mpEnd - pDst);
        if (count < first)
            first = count;
        XMemCpy(pDst, pSrc, static_cast<u32>(4 * first));
        XMemCpy(pPtrs->mpStart, pSrc + first, static_cast<u32>(4 * (count - first)));
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
// freshly marshalled tap buffers (mpTap, and mpTap2 when a crossfade is in flight) so a
// partially-filled ring reads as silence rather than stale data.
// -------------------------------------------------------------------------------------
void DelayLine::IncrementalClean(s32 count, s32 offset, TapContext *pCtx) const
{
    if (miCleanCursor >= miCapacity) // +0x24 vs +0x14
        return;

    s32 n = miReadPosition - miCleanCursor - offset; // +0x28 - +0x24 - offset
    const s32 limit = count + miReadLength - 1;      // count + +0x10 - 1
    if (n >= limit)
        n = limit;
    if (n < 0)
        n = 0;
    for (s32 k = 0; k < n; ++k)
        pCtx->mpTap[k] = KF_ZERO;

    if (pCtx->mpTap2)
    {
        const s32 base = miReadLength;                                     // +0x10
        s32 m = miFilterReadPosition - offset - miCleanCursor;             // +0x2C - offset - +0x24
        if (m >= base + 127)
            m = base + 127;
        const s32 cap2 = base + count - 1;
        if (m >= cap2)
            m = cap2;
        if (m < 0)
            m = 0;
        for (s32 k = 0; k < m; ++k)
            pCtx->mpTap2[k] = KF_ZERO;
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
    taps[0].miLength = miReadPosition;                 // +0x28
    taps[0].miLength2 = miReadLength + count - 1;      // +0x10 + count - 1
    taps[0].mpOut = 0;                                 // overwritten by LoadTaps

    if (mbFilterChanged) // +0x38
    {
        s32 window = count;
        if (count >= 128)
            window = 128;
        taps[1].miLength = miFilterReadPosition;       // +0x2C
        taps[1].miLength2 = miReadLength + window - 1;
        taps[1].mpOut = 0;
        tapCount = 2;
    }

    f32 *const pLoadedEnd = LoadTaps(&ptrs, taps, tapCount);
    pCtx->mpTap = taps[0].mpOut;      // a5[1]
    pCtx->mpLoadedEnd = pLoadedEnd;   // a5[4]
    pCtx->mpTap2 = mbFilterChanged ? taps[1].mpOut : 0; // a5[2]

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
    WriteData(&ptrs, pCtx->mpLoadedEnd - count, 0, count);
}

// -------------------------------------------------------------------------------------
// DelayLine::ApplyFilter @0x82B6E4A0 -- the per-frame process. For each channel it marshals
// the delayed input, then dispatches the installed filter over the samples: the frame after
// a filter change it first runs a crossfade chunk (<=128) through the descending coefficient
// ramp, then the remainder with no ramp; each chunk advances the whole context. After every
// channel the ring cursors advance one frame and the filter-changed flag clears.
// -------------------------------------------------------------------------------------
void DelayLine::ApplyFilter(s32 count, const AudioChannelBuffer *pInput,
                            const AudioChannelBuffer *pOutput, s32 src)
{
    // Descending crossfade coefficients 127/128 .. 0 (only built when a filter change is
    // pending; ApplyFilter re-tests the flag per channel below).
    f32 ramp[KI_RAMP_LEN];
    if (mbFilterChanged)
    {
        f32 coeff = KF_RAMP_START;
        for (s32 k = 0; k < KI_RAMP_LEN; ++k)
        {
            ramp[k] = coeff;
            coeff -= KF_RAMP_STEP;
        }
    }

    for (s32 channel = 0; channel < miChannelCount; ++channel)
    {
        s32 rampRemaining = 0;

        TapContext ctx;
        ctx.mpBase = pInput->mpSamples + static_cast<usize>(pInput->muStride) * channel;
        ctx.mpOut = pOutput->mpSamples + static_cast<usize>(pOutput->muStride) * channel;
        ctx.mpTap2 = 0;
        ctx.mpRamp = 0;
        if (mbFilterChanged)
        {
            rampRemaining = KI_RAMP_LEN;
            ctx.mpRamp = ramp;
        }

        for (s32 i = 0; i < count; UnmarshalDelayData(channel, i, &ctx))
        {
            const s32 total = MarshalDelayData(channel, count, i, &ctx);
            s32 remainder = total;

            if (rampRemaining)
            {
                s32 chunk = rampRemaining;
                if (rampRemaining >= total)
                    chunk = total;

                ctx.mpRamp = ramp + (KI_RAMP_LEN - rampRemaining);
                mpFilter->mpApplyFunc(mpFilter, chunk, src, channel, &ctx);

                i += chunk;
                rampRemaining -= chunk;
                ctx.mpBase += chunk;
                ctx.mpTap += chunk;
                ctx.mpTap2 += chunk;
                ctx.mpRamp += chunk;
                ctx.mpLoadedEnd += chunk;
                ctx.mpOut += chunk;
                remainder = total - chunk;
            }

            if (remainder)
            {
                ctx.mpTap2 = 0;
                ctx.mpRamp = 0;
                mpFilter->mpApplyFunc(mpFilter, remainder, src, channel, &ctx);

                i += remainder;
                ctx.mpBase += remainder;
                ctx.mpTap += remainder;
                ctx.mpTap2 += remainder;
                ctx.mpRamp += remainder;
                ctx.mpLoadedEnd += remainder;
                ctx.mpOut += remainder;
            }
        }
    }

    // Advance the ring cursors by one frame.
    const s32 capacity = miCapacity;
    s32 writePhase = (count + miWritePhase) % capacity;
    if (writePhase <= miGuardSamples)
        writePhase = miGuardSamples;
    miWritePhase = writePhase;                       // +0x34

    s32 clean = count + miCleanCursor;
    if (clean >= capacity)
        clean = capacity;
    miCleanCursor = clean;                           // +0x24

    s32 avail = miAvailableSamples + count;
    if (avail >= capacity)
        avail = capacity;
    miAvailableSamples = avail;                      // +0x20

    mbFilterChanged = 0;                             // +0x38
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
            Allocator()->Alloc(4u * static_cast<u32>(miChannelCount) * static_cast<u32>(newCapacity),
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
            Allocator()->Free(mpBuffer, 0);

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
        Allocator()->Free(mpBuffer, 0);
        mpBuffer = 0;
    }
    mpLocalBuffer = 0; // +0x08
    mpFilter = 0;      // +0x04
    miMaxDelaySamples = 0; // +0x0C
}

} // namespace core
} // namespace audio
} // namespace rw

// ---------------------------------------------------------------------------
// FLAG PC link-closure stub (descriptor-record wave 2026-08-28): SetDelay is
// declared-only in DelayLine.h (grounded solely by its Delay::Process call
// site; no exact console address recovered). The Delay plug-in itself is NOT
// registered on this build, so Process -- and therefore this -- is unreachable;
// decode the real re-target body when the Delay plug-in's slice lands.
// ---------------------------------------------------------------------------
namespace rw { namespace audio { namespace core {
void DelayLine::SetDelay(s32 /*delaySamples*/)
{
}
} } }