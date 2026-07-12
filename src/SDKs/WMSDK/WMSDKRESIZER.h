// ===========================================================================
// SDKs/WMSDK/WMSDKRESIZER.h
//
// WMSDKRESIZER -- the Windows-Media (WMSDK) image/video resizer object from
// BURNOUT_X360_ARTIST.XEX. It owns an output BITMAPINFOHEADER, two scratch
// XMem buffers, and two embedded resamplers (CMSMtoN for the general case,
// CClipResize for the clip/crop case) and copies YUV/RGB frames plane-by-plane
// according to the output format's FourCC.
//
// Member byte offsets in the comments are the X360 (32-bit) form, pinned from
// the lwz/stw displacements in the asm (0x82A44BF0 BMPSize, 0x82A44CA0 Reset,
// 0x82A45C70 allocBuffer, 0x82A44B60 getType, 0x82A458B0 putField, 0x82A43F80
// dtor, 0x82A44380 CopyBuffer2D). This is a runtime-only object (never
// serialised), so the PC (64-bit) layout differs harmlessly -- every field is
// accessed BY NAME, so semantic parity does not depend on the byte offsets.
//
// The two embedded resamplers are held as opaque byte regions (CMSMtoN has no
// reconstructed layout yet; CClipResize's PC size differs from the X360 0x84).
// Their sub-object base addresses are handed to the resamplers' own reconstructed
// functions (CClipResize_Init / CMSMtoN::Reset / the two dtors) exactly as the
// asm does -- see WMSDKRESIZER.cpp.
//
// WMSDK is an external SDK boundary, so its class/method identifiers are kept
// verbatim per the naming convention (external boundary exception).
// ===========================================================================
#pragma once
#include "types.hpp"

// --- Standard Win32 BITMAPINFOHEADER (the output format descriptor). ---------
// biCompression carries the FourCC; biBitCount is the bits-per-pixel. Defined
// here (guarded) because there is no shared Windows header in this tree.
#ifndef WMSDK_BITMAPINFOHEADER_DEFINED
#define WMSDK_BITMAPINFOHEADER_DEFINED
struct BITMAPINFOHEADER
{
    u32 biSize;          // 0x00
    s32 biWidth;         // 0x04
    s32 biHeight;        // 0x08
    u16 biPlanes;        // 0x0C
    u16 biBitCount;      // 0x0E
    u32 biCompression;   // 0x10  (FourCC)
    u32 biSizeImage;     // 0x14
    s32 biXPelsPerMeter; // 0x18
    s32 biYPelsPerMeter; // 0x1C
    u32 biClrUsed;       // 0x20
    u32 biClrImportant;  // 0x24
};
#endif

// --- Xbox-360 XDK heap entry points (platform boundary; NOT reconstructed). --
// Declared with the shapes the X360 call sites use so the bodies compile.
void* XMemAlloc(u32 uSize, u32 uAttributes);
void  XMemFree(void* pAddress, u32 uAttributes);

class WMSDKRESIZER
{
public:
    // @ 0x82A44BF0 -- image size (bytes) of the output BITMAPINFOHEADER for the
    // given width/height: planar YUV formats pack tightly (bits*w*h/8); every
    // other format rounds each row up to a 4-byte (DWORD) boundary.
    u32 BMPSize(s32 iWidth, s32 iHeight);

    // @ 0x82A44B60 -- classify the output FourCC: 1 = planar 4:2:0-ish, 3 = NV1x,
    // 2 = packed/other, 0 = no header.
    int getType();

    // @ 0x82A44CA0 -- recompute the output header (width/height/size) and the
    // change flags, then re-init whichever resampler is active. Returns 1.
    int Reset();

    // @ 0x82A45C70 -- free and re-allocate the two scratch buffers, sized from
    // the cached output/source dimensions. Returns nonzero on success.
    int allocBuffer();

    // @ 0x82A458B0 -- copy the internal source buffer into the caller's frame
    // buffer as either planar (type 1) or packed (type 2) fields. Returns 1, or
    // 0 when there is nothing to do.
    //   pDst   (r4) : caller's destination frame buffer
    //   iField (r5) : select the field/interlace offset variant
    //   iFlag  (r6) : suppress the U/V swap (type 2 only)
    int putField(u8* pDst, int iField, int iFlag);

    // @ 0x82A43F80 -- free the two scratch buffers and the output header, then
    // destroy the two embedded resamplers (CClipResize first, then CMSMtoN, as
    // the asm does).
    ~WMSDKRESIZER();

    // @ 0x82A44380 -- the format-dispatched YUV/RGB plane copier with full
    // cropping. `a1` is the (unused) leading argument the X360 build passes in
    // r3; the remaining args are the plane bases, strides, crop rectangle,
    // FourCC, bit-depth and flip flags. Only a2/a3 are memory bases; the rest
    // are ints. Many positional args (a9..a27, a29, a31, a33, a35, a37, a39,
    // a41, a43) are never referenced by the body -- kept to preserve the exact
    // call signature.
    static int CopyBuffer2D(
        int a1, u8* pDst, u8* pSrc, int iDstStride, int iDstHeight, int iSrcStride,
        int iSrcHeight, int iSrcOffset, int a9, int a10, int a11, int a12, int a13,
        int a14, int a15, int a16, int a17, int a18, int a19, int a20, int a21,
        int a22, int a23, int a24, int a25, int a26, int a27, int iRect28, int a29,
        int iRect30, int a31, int iRect32, int a33, int iCopyWidth, int a35,
        int iCopyHeight, int a37, u32 uFourCC, int a39, int iBitDepth, int a41,
        int bDstFlip, int a43, int bSrcFlip);

private:
    s32               mReserved00;             // +0x00
    BITMAPINFOHEADER* mpBitmapInfo;            // +0x04  output format header
    s32               mReserved08;             // +0x08
    s32               mReserved0C;             // +0x0C
    s32               mDstWidth;               // +0x10
    s32               mDstHeight;              // +0x14
    s32               mDstOriginX;             // +0x18  (dst sub-rect origin; role inferred)
    s32               mDstOriginY;             // +0x1C
    s32               mSrcWidth;               // +0x20
    s32               mSrcHeight;              // +0x24
    s32               mAllocDstWidth;          // +0x28  (dims the output buffer was sized for)
    s32               mAllocDstHeight;         // +0x2C
    s32               mAllocSrcWidth;          // +0x30  (dims the source buffer was sized for)
    s32               mAllocSrcHeight;         // +0x34
    s32               mReserved38;             // +0x38
    s32               mReserved3C;             // +0x3C
    u8                mMStoNState[0x8C];       // +0x40  CMSMtoN sub-object (opaque storage)
    u8                mClipResizeState[0x84];  // +0xCC  CClipResize sub-object (opaque storage)
    s32               mReserved150;            // +0x150
    s32               mUseClipResize;          // +0x154 (nonzero -> use CClipResize)
    s32               mProgressiveToInterlaced;// +0x158
    s32               mResizeFlags;            // +0x15C
    u8*               mpBuffer0;               // +0x160 scratch buffer (output-sized)
    u8*               mpBuffer1;               // +0x164 scratch buffer (source-sized)
};
