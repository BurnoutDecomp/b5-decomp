// ===========================================================================
// SDKs/WMSDK/CClipResize.cpp
//
// CClipResize -- packed/planar video clip resampler from the Burnout 5 Windows-
// Media (WMSDK) resizer path. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This batch bodies:
//   CClipResize::CClipResize   0x82AB4790  (store-for-store: zeroes all working
//                                           state, miOne(+0x4C)=1)
//   CClipResize::ResizeInt2            0x82AB74F0  (BLOCKED stub -> this)
//   CClipResize::ResizePacked32Int2    0x82AB9000  (BLOCKED stub -> this)
//   CClipResize::ResizePackedYUYInt2   0x82AB9E08  (BLOCKED stub -> this)
//
// The three resamplers are IDA-flagged 'local variable allocation has failed'
// ~700..2500-instruction separable cubic filters with pervasive __int64 register
// puns (HIDWORD = byte pointer, LODWORD = fixed-point scalar) and an aliased
// register window. Per the anti-fabrication rule ("a blocked function beats a
// fabricated one") their pixel math is DEFERRED to a dedicated asm-grounded pass;
// each returns `this` (the X360 verified return value) and is otherwise a stub.
//
// ClipResizeHighQuality (0x82ABB920) and ~CClipResize (0x82AB47E0) are NOT bodied
// in this batch (declared-only in the header; homed by their own follow-on slices).
// ===========================================================================

#include "SDKs/WMSDK/CClipResize.h"

// ---------------------------------------------------------------------------
// CClipResize::CClipResize  @ 0x82AB4790
// Zero-init all working state; miOne (0x4C) starts at 1.
// (Every store below matches the asm stw displacement sequence 1:1.)
// ---------------------------------------------------------------------------
CClipResize::CClipResize()
{
    mReserved00 = 0;   // 0x00
    mReserved04 = 0;   // 0x04
    mReserved08 = 0;   // 0x08
    mReserved0C = 0;   // 0x0C
    mReserved10 = 0;   // 0x10
    mReserved14 = 0;   // 0x14
    mReserved18 = 0;   // 0x18
    mReserved1C = 0;   // 0x1C
    mReserved20 = 0;   // 0x20
    mReserved24 = 0;   // 0x24
    mReserved28 = 0;   // 0x28
    mReserved2C = 0;   // 0x2C
    mReserved40 = 0;   // 0x40
    miOne        = 1;  // 0x4C
    miInterlaced = 0;  // 0x60
    mReserved7C  = 0;  // 0x7C
    mReserved80  = 0;  // 0x80
}

// X360 0x82AB74F0  CClipResize::ResizeInt2
// BLOCKED (confidence=low). The Hex-Rays for this ~700-line 5-tap separable
// planar (Y + half-res U/V) resampler ships with 'local variable allocation
// has failed' and pervasive __int64 register puns. Emitting a subtly-wrong
// filter is worse than blocking, so the body is intentionally left as a stub
// for a dedicated verify pass. The X360 function returns `this`.
CClipResize* CClipResize::ResizeInt2(f64 lfDX, f64 lfSX, f64 lfDY, f64 lfOY)
{
    (void)lfDX; (void)lfSX; (void)lfDY; (void)lfOY;
    return this;   // BLOCKED: body needs asm-grounded register-pun disentangling
}

// X360 0x82AB9000  CClipResize::ResizePacked32Int2
// BLOCKED (confidence=low). Packed-32bpp (4 bytes/pixel, e.g. AYUV) sibling of
// ResizeInt2 with the same 5-tap filter shape and the same __int64 register
// pun. Left as a stub rather than risk a subtly-wrong pixel filter. Returns `this`.
CClipResize* CClipResize::ResizePacked32Int2(f64 lfDX, f64 lfSX, f64 lfDY, f64 lfOY)
{
    (void)lfDX; (void)lfSX; (void)lfDY; (void)lfOY;
    return this;   // BLOCKED: body needs asm-grounded register-pun disentangling
}

// X360 0x82AB9E08  CClipResize::ResizePackedYUYInt2
// BLOCKED (confidence=low). Per-scanline packed-YUY2 (4:2:2) bicubic upscaler.
// ~2500 instructions; IDA emits 'local variable allocation has failed' and the
// r11 register window is aliased across the whole body as an overlapping
// _BYTE v13[12], so the pseudocode's reads are DISTINCT register temporaries that
// cannot be disambiguated without a full asm-driven re-derivation. Deferred.
//
// Attested entry facts (from 0x82AB9E08 asm), preserved for the re-derivation:
//   * liHigh selects the interlace path (caller passes 1 for YUY2, 0 for UYVY).
//   * luma base   = this+0x64 (mpSrcY), stride-2 packed Y writes.
//   * chroma base = this+0x70 (mpDstY), stride-4 U/V writes.
//   * dst height  = this+0x5C, field index = this+0x60.
//   * returns r3 == this unchanged (entry stw r3,arg_14; exit return result).
CClipResize* CClipResize::ResizePackedYUYInt2(f64 lfDX, f64 lfSX, f64 lfDY,
                                              f64 lfOY, s32 liHigh)
{
    (void)lfDX; (void)lfSX; (void)lfDY; (void)lfOY; (void)liHigh;
    return this;   // BLOCKED: body needs asm-grounded register-pun disentangling
}
