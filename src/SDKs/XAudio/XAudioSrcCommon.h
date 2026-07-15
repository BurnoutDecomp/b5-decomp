#pragma once

// ===========================================================================
// XAUDIO::SRC -- shared job descriptor + sample-format tags for the XAudio
// middleware's sample-rate-conversion passes in BURNOUT_X360_ARTIST.XEX.
//
// This is the single OWNING home for the three types the SRC passes share:
//
//     XAUDIO::SRC::SHORTLE           -- little-endian 16-bit sample format tag
//     XAUDIO::SRC::SrcHistorySample  -- per-channel previous-sample slot
//     XAUDIO::SRC::XAUDIOSRCHDR      -- the SRC job header threaded through Process()
//
// Both the "native" (1:1) resampler (XAudioSrcNative) and the "2x" resampler
// (XAudioSrcDouble) include this header so the layout is defined exactly once
// (the two TUs previously each defined an incompatible fork of these types).
//
// There is no reference source and no DWARF for these TUs, so the SHAPE below
// is reconstructed purely from the X360 asm of the resampler kernels (pure
// scalar, no VMX). Field offsets are documented as the attested X360 32-bit
// layout; on the host's 64-bit pointer width the post-pointer offsets shift, so
// _AssertLayout() below pins only pointer-invariant ordering/spacing facts.
// `XAUDIO` is an external middleware boundary, so its API identifiers
// (`XAUDIOSRCHDR`, `SHORTLE`) are preserved verbatim; reconstructed members use
// the project Hungarian.
// ===========================================================================

#include "types.hpp"

#include <cstddef> // offsetof (used by the layout self-check)

namespace XAUDIO
{
namespace SRC
{

// ---------------------------------------------------------------------------
// SHORTLE -- format tag for a 16-bit PCM sample stored little-endian in the
// source stream. Distinct from the platform-native `short` (big-endian s16 on
// the X360, consumed as-is): the SHORTLE path byte-swaps before use. It is an
// EMPTY POLICY TAG -- it only selects a Process<> template specialisation and
// is NEVER dereferenced. The interleaved source is read as raw u16 words (the
// image's `lhz`; 2-byte stride) and byte-swapped at decode time.
// ---------------------------------------------------------------------------
struct SHORTLE
{
};

// ---------------------------------------------------------------------------
// SrcHistorySample -- per-channel "previous sample" carried across Process()
// calls (used by the 2x resampler's linear interpolation). During a call it
// holds the normalised float sample; between calls it persists in the raw
// input format (the kernel re-quantises it on exit and decodes it on entry),
// so the slot is a 4-byte union reinterpreted in place -- exactly as the X360
// code stores an f32 (stfs @+0x34) and later reloads it as u16/s16/u8
// (lhz/lbz @+0x34).
// ---------------------------------------------------------------------------
union SrcHistorySample
{
    f32 mFloat;    // working value (normalised, in flight)
    u16 mShortLE;  // persisted little-endian s16 slot (byte-swapped on store)
    s16 mShort;    // persisted native s16 slot
    u8  mByte;     // persisted 128-biased u8 slot
};

// ---------------------------------------------------------------------------
// XAUDIOSRCHDR -- the SRC job header threaded through Process(). Field offsets
// are grounded in the X360 asm load/store displacements. The channel count
// sits at +0x0D, echoing the format block the mixer keeps at +0x0D. The volume
// block at +0x24/+0x28 is a linear ramp: +0x24 is the running gain (loaded,
// scaled, stepped, and STORED BACK via `stfs 0x24`); +0x28 is the ramp target
// (loaded only). The per-channel history scratch begins at +0x34.
// ---------------------------------------------------------------------------
struct XAUDIOSRCHDR
{
    void* mpSource;       // +0x00 -- interleaved source PCM base
    s32   miSourceFrames; // +0x04 -- total source frames available
    s32   miSourcePos;    // +0x08 -- source frame cursor (read + written back)
    u8    mPad0C[1];      // +0x0C -- format-block leading byte (untouched here)
    u8    muChannels;     // +0x0D -- interleaved source channel count
    u8    mPad0E[2];      // +0x0E
    u8    mPad10[4];      // +0x10 -- (untouched by these TUs)
    f32*  mpDest;         // +0x14 -- planar f32 output base
    s32   miDestFrames;   // +0x18 -- output capacity in frames
    s32   miDestPos;      // +0x1C -- output frame cursor (read + written back)
    u8    mPad20[4];      // +0x20 -- (untouched by these TUs)
    f32   mfVolCurrent;   // +0x24 -- running volume (ramp start; written back)
    f32   mfVolTarget;    // +0x28 -- volume the ramp walks toward this call
    u8    mPad2C[8];      // +0x2C
    SrcHistorySample mScratch[6]; // +0x34 -- per-channel previous-sample history
};

// ---------------------------------------------------------------------------
// Never called -- pins the pointer-invariant field spacing of XAUDIOSRCHDR that
// the asm relies on (deltas below all live on one side of a pointer member, so
// they survive the X360 (32-bit) -> host (64-bit) pointer-width shift).
// ---------------------------------------------------------------------------
inline void _AssertLayout()
{
    static_assert(offsetof(XAUDIOSRCHDR, mpSource) == 0,
                  "XAUDIOSRCHDR: source base must lead the header");
    static_assert(offsetof(XAUDIOSRCHDR, miSourcePos) - offsetof(XAUDIOSRCHDR, miSourceFrames) == 4,
                  "XAUDIOSRCHDR: source frame-count/cursor spacing");
    static_assert(offsetof(XAUDIOSRCHDR, muChannels) - offsetof(XAUDIOSRCHDR, miSourcePos) == 5,
                  "XAUDIOSRCHDR: channel-count sits 5 bytes past the source cursor (+0x0D)");
    static_assert(offsetof(XAUDIOSRCHDR, miDestPos) - offsetof(XAUDIOSRCHDR, miDestFrames) == 4,
                  "XAUDIOSRCHDR: dest frame-count/cursor spacing");
    static_assert(offsetof(XAUDIOSRCHDR, mfVolCurrent) - offsetof(XAUDIOSRCHDR, miDestPos) == 8,
                  "XAUDIOSRCHDR: volume block sits 8 bytes past the dest cursor");
    static_assert(offsetof(XAUDIOSRCHDR, mfVolTarget) - offsetof(XAUDIOSRCHDR, mfVolCurrent) == 4,
                  "XAUDIOSRCHDR: current/target volume are adjacent f32s");
    static_assert(offsetof(XAUDIOSRCHDR, mScratch) - offsetof(XAUDIOSRCHDR, mfVolTarget) == 12,
                  "XAUDIOSRCHDR: history scratch begins 12 bytes past the target volume (+0x34)");
    static_assert(sizeof(SrcHistorySample) == 4, "history slot is a 4-byte punned union");
}

} // namespace SRC
} // namespace XAUDIO
