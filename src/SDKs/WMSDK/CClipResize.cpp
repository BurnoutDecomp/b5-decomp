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
// ClipResizeHighQuality (0x82ABB920, the format dispatch) and ~CClipResize
// (0x82AB47E0, the buffer teardown) are also bodied here -- both are short,
// fully-attested control flow (no unreliable pixel math).
// ===========================================================================

#include "SDKs/WMSDK/CClipResize.h"

#include <cstdlib>   // ::free (the PC heap leaf for the 0x7C scratch buffer)

// XDK physical allocator leaf used by the destructor (0x82AB4810 `bl XMemFree`);
// same prototype as the other WMSDK/D3D reconstructions.
extern void XMemFree(void* pAddress, u32 uAttributes);

// File-static FourCC flag written at 0x8323ED10 by ClipResizeHighQuality: set when
// the requested format is 'P411' (a planar passthrough that skips resampling).
static bool sbP411Format;

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

// ---------------------------------------------------------------------------
// CClipResize::ClipResizeHighQuality  @ 0x82ABB920
// Records whether the FourCC is 'P411' in the file-static flag, stashes the six
// plane pointers, then dispatches to the format-specific resampler. Planar
// 'P411'/'P422' are passthrough (return `this`); 'YUY2'/'UYVY' share the packed
// 4:2:2 path (liHigh=1 for YUY2, 0 for UYVY); 0 and 'AYUV' take the packed-32bpp
// path; everything else falls to the generic planar filter. The four doubles are
// forwarded verbatim to whichever resampler is tail-called.
// ---------------------------------------------------------------------------
CClipResize* CClipResize::ClipResizeHighQuality(u8* lpSrcY, u8* lpSrcU, u8* lpSrcV,
                                                u8* lpDstY, u8* lpDstU, u8* lpDstV,
                                                f64 lfScaleX, f64 lfScaleY,
                                                f64 lfStepY, f64 lfOffsetY, u32 luFourCC)
{
    sbP411Format = (luFourCC == 0x31313450u);   // 'P411' (byte_8323ED10)

    mpDstY = lpDstY;   // 0x70
    mpDstU = lpDstU;   // 0x74
    mpDstV = lpDstV;   // 0x78
    mpSrcY = lpSrcY;   // 0x64
    mpSrcU = lpSrcU;   // 0x68
    mpSrcV = lpSrcV;   // 0x6C

    if (sbP411Format || luFourCC == 0x50343232u)   // 'P411' / 'P422': passthrough
        return this;

    if (luFourCC == 0x32595559u)                    // 'YUY2'
        return ResizePackedYUYInt2(lfScaleX, lfScaleY, lfStepY, lfOffsetY, 1);
    if (luFourCC == 0x59565955u)                    // 'UYVY'
        return ResizePackedYUYInt2(lfScaleX, lfScaleY, lfStepY, lfOffsetY, 0);
    if (luFourCC == 0u || luFourCC == 0x41595556u)  // 0 / 'AYUV': packed 32bpp
        return ResizePacked32Int2(lfScaleX, lfScaleY, lfStepY, lfOffsetY);
    return ResizeInt2(lfScaleX, lfScaleY, lfStepY, lfOffsetY);
}

// ---------------------------------------------------------------------------
// CClipResize::~CClipResize  @ 0x82AB47E0
// Release the two owned scratch buffers (the 0x80 slot came from the XDK
// physical heap, the 0x7C slot from the CRT heap), null both, then mark the
// object torn down (0x40 = 1). Called explicitly by ~WMSDKRESIZER.
// ---------------------------------------------------------------------------
CClipResize::~CClipResize()
{
    if (mReserved80 != 0)
    {
        XMemFree(reinterpret_cast<void*>(static_cast<uintptr_t>(mReserved80)), 0x248C8000u);
        mReserved80 = 0;
    }
    if (mReserved7C != 0)
    {
        ::free(reinterpret_cast<void*>(static_cast<uintptr_t>(mReserved7C)));
        mReserved7C = 0;
    }
    mReserved40 = 1;
}
