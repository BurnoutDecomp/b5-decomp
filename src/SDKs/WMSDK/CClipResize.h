// CClipResize.h  -- X360 WMSDK-style planar/packed video clip resampler.
// Reconstructed from asm at 0x82AB4790 (ctor), 0x82ABB920 (ClipResizeHighQuality),
// 0x82AB74F0 (ResizeInt2), 0x82AB9000 (ResizePacked32Int2), 0x82AB9E08
// (ResizePackedYUYInt2). Member byte offsets are pinned directly from the stw/lwz
// displacements in the asm (NOT Hex-Rays /4 indices) and cross-checked against the
// ctor store sequence. Object is >= 0x84 bytes on X360.
//
// This batch bodies the ctor (0x82AB4790, store-for-store) plus the three format
// resamplers as returning stubs: ResizeInt2 / ResizePacked32Int2 / ResizePackedYUYInt2
// are IDA-flagged-unreliable ~700..2500-instruction separable filters with aliased
// register windows; per the anti-fabrication rule their pixel math is DEFERRED
// (bodies return `this`) rather than transcribed from the unreliable pseudocode.
// ClipResizeHighQuality (0x82ABB920) and ~CClipResize (0x82AB47E0) are declared here
// but bodied by their own follow-on slices (out of this batch).
//
// CClipResize is external middleware, so its class/method identifiers are kept
// verbatim per the naming convention (external boundary exception).
#pragma once
#include "types.hpp"

class CClipResize
{
public:
    CClipResize();                                                   // 0x82AB4790

    // High-level entry (0x82ABB920): stores the six plane pointers, records the
    // FourCC in the file-static P411 flag, then dispatches to the format-specific
    // resampler. The four doubles are only forwarded to the tail call. Returns `this`.
    // (Declared here for the resampler callers; bodied by its own follow-on slice.)
    CClipResize* ClipResizeHighQuality(u8* lpSrcY, u8* lpSrcU, u8* lpSrcV,
                                       u8* lpDstY, u8* lpDstU, u8* lpDstV,
                                       f64 lfScaleX, f64 lfScaleY,
                                       f64 lfStepY, f64 lfOffsetY, u32 luFourCC);

    // Format-specific resamplers (tail-called from ClipResizeHighQuality). The four
    // doubles are scale/step/offset params forwarded from the caller.
    CClipResize* ResizeInt2(f64 lfDX, f64 lfSX, f64 lfDY, f64 lfOY);           // 0x82AB74F0
    CClipResize* ResizePacked32Int2(f64 lfDX, f64 lfSX, f64 lfDY, f64 lfOY);   // 0x82AB9000
    CClipResize* ResizePackedYUYInt2(f64 lfDX, f64 lfSX, f64 lfDY, f64 lfOY,
                                     s32 liHigh);                             // 0x82AB9E08

    // 0x82AB47E0 -- destructor. Called explicitly by ~WMSDKRESIZER (which embeds
    // a CClipResize sub-object). Declared here; bodied by its own follow-on slice.
    ~CClipResize();

private:
    // ---- 0x00..0x3C : working/accumulator state (all zeroed in ctor) ----
    s32 mReserved00;   // 0x00
    s32 mReserved04;   // 0x04
    s32 mReserved08;   // 0x08
    s32 mReserved0C;   // 0x0C
    s32 mReserved10;   // 0x10
    s32 mReserved14;   // 0x14
    s32 mReserved18;   // 0x18
    s32 mReserved1C;   // 0x1C
    s32 mReserved20;   // 0x20
    s32 mReserved24;   // 0x24
    s32 mReserved28;   // 0x28
    s32 mReserved2C;   // 0x2C
    s32 mUnused30[4];  // 0x30..0x3C (not touched by these funcs)
    s32 mReserved40;   // 0x40 (zeroed in ctor)
    s32 mUnused44[2];  // 0x44,0x48
    s32 miOne;         // 0x4C (=1 in ctor)
    // ---- geometry (read by the resamplers) ----
    s32 miSrcStride;   // 0x50  (source width / row stride; >>1 -> chroma stride)
    s32 miSrcHeight;   // 0x54  (source height; compared as height-2 / height-4)
    s32 miDstWidth;    // 0x58
    s32 miDstHeight;   // 0x5C
    s32 miInterlaced;  // 0x60  (interlace/field flag; zeroed in ctor)
    // ---- source plane pointers ----
    u8* mpSrcY;        // 0x64  (also packed-32bpp src base)
    u8* mpSrcU;        // 0x68
    u8* mpSrcV;        // 0x6C
    // ---- destination plane pointers ----
    u8* mpDstY;        // 0x70  (also packed-32bpp dst base)
    u8* mpDstU;        // 0x74
    u8* mpDstV;        // 0x78
    s32 mReserved7C;   // 0x7C  (zeroed in ctor)
    s32 mReserved80;   // 0x80  (zeroed in ctor)
};

// Free-function initialiser used by WMSDKRESIZER::Reset (X360 `bl CClipResize_Init`
// @ 0x82A44D94): sets up the clip resampler for (dstWidth, height, srcWidth).
// Declared here; bodied by its own slice.
CClipResize* CClipResize_Init(CClipResize* pThis, int iDstWidth, int iHeight, int iSrcWidth);
