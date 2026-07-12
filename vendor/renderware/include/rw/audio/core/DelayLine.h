#pragma once

// =====================================================================================
// rw::audio::core::DelayLine -- the multi-channel ring-buffer delay line that backs the
// rw::audio::core::Delay audio plug-in and the rw::audio::core::ReverbModel1 reverb. It
// owns a System-allocated interleaved ring buffer (one contiguous span per channel, laid
// out at a per-channel `capacity` stride), tracks read/write/clean cursors into that ring,
// and drives an installed unit filter (SetFilter -> IFilter) across the marshalled delay
// taps once per processed frame (ApplyFilter). Delay/ReverbModel construct one, size it
// with Init/Resize, prime it with Reset, and each frame Set the local scratch buffer +
// filter, then ApplyFilter.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. No Feb-2007 leak source and no DecFIGS
// DWARF exist for this TU, and it is absent from the ProStreet08 rwaudio PDB, so each
// offset below is grounded directly in the disassembly of the bodied members:
//   DelayLine::DelayLine          @0x82B67EE8   (ctor -- clears the 0x3C-byte record)
//   DelayLine::Init               @0x82B6C968
//   DelayLine::Resize             @0x82B6CA38
//   DelayLine::Reset              @0x82B68068
//   DelayLine::Release            @0x82B6CC08
//   DelayLine::CalcChannelPointers@0x82B68090
//   DelayLine::ReadData           @0x82B6CC78
//   DelayLine::WriteData          @0x82B6CD30
//   DelayLine::LoadTaps           @0x82B6CDD8
//   DelayLine::IncrementalClean   @0x82B67F88
//   DelayLine::MarshalDelayData   @0x82B6CF10
//   DelayLine::UnmarshalDelayData @0x82B6CFE8
//   DelayLine::ApplyFilter        @0x82B6E4A0
//   DelayLine::GetValidDelaySamples/SetFilter/SetLocalBuffer/~DelayLine (trivial)
//
// X360 offsets are 4-byte-word; the PC layout below widens the three pointer members to
// x64 (only the member ORDER is load-bearing -- every access in the .cpp is by name, and
// the +0xNN comments are documentary X360 offsets).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp"                          // f32, s32, u8, u32
#include "rw/audio/core/IFilter.h"            // rw::audio::core::IFilter
#include "rw/audio/core/Iir2Filters.h"        // AudioChannelBuffer (in/out sample descriptors)

namespace rw
{
namespace audio
{
namespace core
{

class DelayLine
{
public:
    // ----------------------------------------------------------------------------------
    // Per-channel ring pointers computed by CalcChannelPointers (the `a2` output). Read by
    // ReadData / WriteData. Four consecutive words on the X360 stack.
    // ----------------------------------------------------------------------------------
    struct ChannelPointers
    {
        f32 *mpStart;   // +0x00 -- channel ring start   (mpBuffer + channel*miCapacity)
        f32 *mpEnd;     // +0x04 -- channel ring end     (mpStart + miCapacity)
        f32 *mpLoopEnd; // +0x08 -- ring end minus guard (mpEnd - miGuardSamples)
        f32 *mpCursor;  // +0x0C -- write cursor at the requested sample offset
    };

    // ----------------------------------------------------------------------------------
    // One marshalling tap descriptor (16 bytes on X360). MarshalDelayData builds an array
    // of these on the stack; LoadTaps rounds the lengths, resolves the selector, reads the
    // delay data into the local buffer and writes back mpOut.
    // ----------------------------------------------------------------------------------
    struct TapRecord
    {
        s32  miLength;  // +0x00 -- primary length (samples; rounded up to 32 by LoadTaps)
        s32  miLength2; // +0x04 -- secondary length
        s32  miSelect;  // +0x08 -- record selector (set by LoadTaps from the length compare)
        f32 *mpOut;     // +0x0C -- destination in the local buffer (written by LoadTaps)
    };

    // ----------------------------------------------------------------------------------
    // The per-tap context ApplyFilter builds and hands to the installed IFilter apply func
    // (the `a5` struct, 6 consecutive words on X360). ApplyFilter fills mpBase/mpOut/mpRamp
    // per channel; MarshalDelayData fills mpTap/mpTap2/mpLoadedEnd; the whole struct is
    // pointer-advanced by the chunk size between the two apply calls. A non-null mpTap2
    // doubles as the filter's crossfade-active flag (the DelayFilter apply tests this word
    // as a nonzero flag; see DelayFilter.h's DelayFilterContext).
    // ----------------------------------------------------------------------------------
    struct TapContext
    {
        f32 *mpBase;      // +0x00 -- delayed-input read base for the channel
        f32 *mpTap;       // +0x04 -- primary marshalled tap output (taps[0].mpOut)
        f32 *mpTap2;      // +0x08 -- secondary tap output / crossfade flag (taps[1].mpOut or 0)
        f32 *mpRamp;      // +0x0C -- crossfade coefficient ramp pointer (or 0)
        f32 *mpLoadedEnd; // +0x10 -- end of loaded tap data (LoadTaps return)
        f32 *mpOut;       // +0x14 -- output write base for the channel
    };

    DelayLine();                                                    // @0x82B67EE8
    ~DelayLine();                                                   // @0x82B6E498 (-> Release)

    // Allocate the ring for `channels` channels with `maxDelaySamples` max delay and
    // `readLength` per-call read length; returns false only if the buffer allocation fails.
    bool Init(s32 channels, s32 maxDelaySamples, s32 readLength);   // @0x82B6C968

    // Grow the ring to hold `maxDelaySamples` (re-allocating + migrating live samples when
    // the new capacity exceeds the current one); returns false only on allocation failure.
    bool Resize(s32 maxDelaySamples);                              // @0x82B6CA38

    // Prime the read / write / clean cursors for a fresh run starting `readPosition`
    // samples of delay in.
    void Reset(s32 readPosition);                                  // @0x82B68068

    // Re-target the primary marshal read position to `delaySamples` (a live delay change,
    // used by Delay::Process when the requested delay fits within the valid samples). Body
    // lives in DelayLine's own TU; DECLARED here so consumers (Delay::Process) compile
    // against the real DelayLine home rather than forking it. Grounded only by its call site
    // (rw::audio::core::Delay::Process @0x82BA2A08: r3 = this, r4 = int delay samples).
    void SetDelay(s32 delaySamples);

    void SetFilter(IFilter *pFilter);                             // @0x82B67F30
    void SetLocalBuffer(f32 *pLocalBuffer, s32 samples);          // @0x82B67F58
    s32  GetValidDelaySamples() const;                            // @0x82B67F68

    // Process `count` samples for every channel: marshal the delayed taps into the local
    // buffer, run the installed filter over them (crossfading through the coefficient ramp
    // on the frame after a filter change), unmarshal the result back into the ring, and
    // advance the ring cursors by one frame. `pInput`/`pOutput` are the graph's per-channel
    // sample descriptors; `src` is the pass-through argument handed to the filter apply.
    void ApplyFilter(s32 count, const AudioChannelBuffer *pInput,
                     const AudioChannelBuffer *pOutput, s32 src);  // @0x82B6E4A0

    // Free the ring back to the System allocator and clear the book-keeping pointers.
    void Release();                                                // @0x82B6CC08

private:
    void CalcChannelPointers(ChannelPointers *pOut, s32 channel, s32 offset) const; // @0x82B68090
    s32  ReadData(const ChannelPointers *pPtrs, f32 *pDest, s32 backOffset, s32 count) const;  // @0x82B6CC78
    void WriteData(const ChannelPointers *pPtrs, const f32 *pSrc, s32 backOffset, s32 count) const; // @0x82B6CD30
    f32 *LoadTaps(const ChannelPointers *pPtrs, TapRecord *pTaps, s32 tapCount) const;          // @0x82B6CDD8
    void IncrementalClean(s32 count, s32 offset, TapContext *pCtx) const;             // @0x82B67F88
    s32  MarshalDelayData(s32 channel, s32 count, s32 offset, TapContext *pCtx);      // @0x82B6CF10
    void UnmarshalDelayData(s32 channel, s32 count, TapContext *pCtx);                // @0x82B6CFE8

public:
    f32     *mpBuffer;             // +0x00 -- interleaved ring buffer (System-allocated)
    IFilter *mpFilter;            // +0x04 -- installed unit filter (apply dispatched per chunk)
    f32     *mpLocalBuffer;       // +0x08 -- external scratch/local buffer base (SetLocalBuffer)
    s32      miMaxDelaySamples;   // +0x0C -- max delay read length in samples
    s32      miReadLength;        // +0x10 -- per-call read length / grain (Init readLength)
    s32      miCapacity;          // +0x14 -- ring capacity in samples (per-channel stride)
    s32      miGuardSamples;      // +0x18 -- wrap guard / overlap region in samples
    s32      miLocalBufferSamples;// +0x1C -- local buffer length (SetLocalBuffer samples)
    s32      miAvailableSamples;  // +0x20 -- samples available / valid (grows toward capacity)
    s32      miCleanCursor;       // +0x24 -- incremental-clean cursor
    s32      miReadPosition;      // +0x28 -- primary marshal read position
    s32      miFilterReadPosition;// +0x2C -- secondary read position (filter/crossfade path)
    s32      miChannelCount;      // +0x30 -- channel count
    s32      miWritePhase;        // +0x34 -- ring write phase
    u8       mbFilterChanged;     // +0x38 -- filter-changed / crossfade-pending flag
};

} // namespace core
} // namespace audio
} // namespace rw
