#include "SDKs/EATech/rwmovie/CIntImage.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CIntImage::CIntImage   @0x82AC1718
//   CIntImage::allocate    @0x82AC1620
//   CIntImage::clean       @0x82AC15D8
//   CIntImage::~CIntImage  @0x82AC1788
//
// See CIntImage.h for the layout note. The RTCMV/WMV codec's integer-sample image buffer.

// CIntImage::CIntImage @0x82AC1718 -- construct an empty integer image and immediately
// allocate its pixel buffer. The rectangle is initialised to an inverted/empty extent
// (left=top=0, right=bottom=-1) so allocate() sees a zero-or-negative size unless the
// caller has filled the rect beforehand. The status returned by allocate() is written
// back through the caller-supplied out-parameter.
CIntImage::CIntImage(s32* lpiResult)
{
    mpAddress = 0;               // +0x00
    mRect.miTop = 0;             // +0x08
    mRect.miLeft = 0;            // +0x04
    mRect.miBottom = -1;         // +0x10
    mRect.miRight = -1;          // +0x0C
    muRawAllocation = 0;         // +0x14

    *lpiResult = allocate();
}

// CIntImage::allocate @0x82AC1620 -- (re)allocate the pixel buffer for the current rect.
//
//   * Normalises the rect through CRct::operator= (the asm passes only &mRect to the
//     copy-assign; here it is a self-normalise of mRect).
//   * Frees any previously held buffer.
//   * A degenerate rect (left>=right or top>=bottom) allocates nothing and returns 0.
//   * Byte size = width*height + 64 slack; the buffer is over-allocated to 2x that size
//     (16-bit / two-byte samples) with a 32-bit overflow guard that forces an impossible
//     0xFFFFFFFF (-1) request. The raw pointer is kept in muRawAllocation and mpAddress
//     holds the same block rounded up to a 64-byte boundary.
s32 CIntImage::allocate()
{
    mRect = mRect;

    if (mpAddress)
    {
        XMemFree(reinterpret_cast<void*>(mpAddress), KU_INTIMAGE_XMEM_ATTRIBUTES);
        mpAddress = 0;
    }

    if (mRect.miLeft >= mRect.miRight || mRect.miTop >= mRect.miBottom)
    {
        return 0;
    }

    u32 luSize = static_cast<u32>((mRect.miBottom - mRect.miTop) * (mRect.miRight - mRect.miLeft)) + 64u;
    s32 liAllocSize = static_cast<s32>(2u * luSize);
    if (luSize > 0x7FFFFFFFu)
    {
        liAllocSize = -1;
    }

    u32 luRaw = reinterpret_cast<u32>(XMemAlloc(static_cast<u32>(liAllocSize), KU_INTIMAGE_XMEM_ATTRIBUTES));
    muRawAllocation = luRaw;
    if (!luRaw)
    {
        return -3;
    }

    u32 luAligned = (luRaw + 63u) & 0xFFFFFFC0u;
    mpAddress = luAligned;
    return luAligned == 0 ? -3 : 0;
}

// CIntImage::clean @0x82AC15D8 -- free the raw pixel allocation (offset +0x14) if present.
// Note the asm frees the *raw* allocation (muRawAllocation), not the 64-byte-aligned view.
void CIntImage::clean()
{
    if (muRawAllocation)
    {
        XMemFree(reinterpret_cast<void*>(muRawAllocation), KU_INTIMAGE_XMEM_ATTRIBUTES);
        muRawAllocation = 0;
    }
}

// CIntImage::~CIntImage @0x82AC1788 -- frees the raw allocation (offset +0x14); the body is
// byte-identical to clean(). Non-virtual (no vtable pointer is ever stored on this object).
CIntImage::~CIntImage()
{
    if (muRawAllocation)
    {
        XMemFree(reinterpret_cast<void*>(muRawAllocation), KU_INTIMAGE_XMEM_ATTRIBUTES);
        muRawAllocation = 0;
    }
}
