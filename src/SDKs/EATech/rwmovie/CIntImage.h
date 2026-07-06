// =====================================================================================
// CIntImage -- integer-sample image buffer for the RTCMV/WMV video (VP6) codec path.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CIntImage::CIntImage   @0x82AC1718
//   CIntImage::allocate    @0x82AC1620
//   CIntImage::clean       @0x82AC15D8
//   CIntImage::~CIntImage  @0x82AC1788
//
// Base pointer in the asm is byte-addressed (int* this with this[N] == byte offset N*4).
// Layout (24 bytes, no vtable):
//   +0x00 mpAddress        64-byte-aligned view of the pixel buffer (raw + up-to-63 slack)
//   +0x04 mRect            CRct extent {left,top,right,bottom} driving the alloc size
//   +0x14 muRawAllocation  raw XMemAlloc pointer (what is actually freed)
//
// allocate() sizes the buffer as ((bottom-top)*(right-left) + 64) two-byte samples, i.e.
// 2*that many bytes, then keeps the raw pointer in muRawAllocation and the 64-byte-aligned
// pointer in mpAddress. Both clean() and the destructor free muRawAllocation (the raw
// pointer). XMemAlloc/XMemFree are the raw XDK heap API called with attributes 0x248C8000
// -- no `this` argument and no CXMemMemoryManager wrapper (matches the sibling
// CLocalHuffman TU).
// =====================================================================================
#pragma once

#include "types.hpp"

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// XMemAlloc/XMemFree attribute word for this codec's image buffers (lis 0x248C / ori 0x8000).
static const u32 KU_INTIMAGE_XMEM_ATTRIBUTES = 0x248C8000u;

// CRct -- 16-byte rectangle value type (see CRct.cpp). Redeclared here as the by-value
// member; the definition lives in CRct.cpp / its own header.
class CRct
{
public:
    CRct& operator=(const CRct& lrSrc);

    s32 miLeft;    // +0x00
    s32 miTop;     // +0x04
    s32 miRight;   // +0x08
    s32 miBottom;  // +0x0C
};

class CIntImage
{
public:
    // Constructs the image and immediately calls allocate(); the resulting status code
    // is written back through lpiResult.
    explicit CIntImage(s32* lpiResult);
    ~CIntImage();

    // (Re)allocates the pixel buffer for the current mRect. Returns 0 on success (or on a
    // degenerate rect), -3 on allocation failure.
    s32 allocate();

    // Frees the raw pixel allocation if present.
    void clean();

private:
    u32  mpAddress;         // +0x00  64-byte-aligned pixel buffer pointer
    CRct mRect;             // +0x04  image extent
    u32  muRawAllocation;   // +0x14  raw XMemAlloc pointer (freed by clean/dtor)
};
