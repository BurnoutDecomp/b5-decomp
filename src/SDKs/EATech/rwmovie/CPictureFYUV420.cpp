#include "SDKs/EATech/rwmovie/CPictureFYUV420.h"

#include <new>   // placement new for the plane CIntImage objects

// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative.
//   CPictureFYUV420::CPictureFYUV420  @0x82A7DBF8
//   CPictureFYUV420::~CPictureFYUV420 @0x82A7DBF0
//   CPictureFYUV420::Clean            @0x82A7DB30
//   CPictureFYUV420::getPlane         @0x82A7DAF8
//
// See CPictureFYUV420.h for the layout note. Planar YUV 4:2:0 picture for the WMV encoder.
// XMemAlloc/XMemFree (declared in CIntImage.h) are the raw XDK heap API, called with no
// `this` argument and attributes 0x248C8000 -- the same convention as every sibling rwmovie
// TU (CIntImage / CQueue / CLocalHuffman / CVideoFrameAnalyst).

namespace
{
    // XMemAlloc/XMemFree attribute word for this codec's plane objects (lis 0x248C / ori 0x8000).
    const u32 KU_PICTUREFYUV420_XMEM_ATTRIBUTES = 0x248C8000u;

    // The plane fields hold X360 (32-bit) heap-object pointers; convert on the boundary.
    inline void* lToPtr(u32 luValue)
    {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(luValue));
    }
    inline u32 lFromPtr(void* lpValue)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpValue));
    }
    inline CIntImage* lPlane(u32 luValue)
    {
        return static_cast<CIntImage*>(lToPtr(luValue));
    }
}

// CPictureFYUV420::CPictureFYUV420 @0x82A7DBF8 -- build the three YUV 4:2:0 planes.
//
// The luma extent (lrLumaRect) sizes the Y plane; the chroma extent (lrChromaRect) sizes the
// U and V planes. Each plane's CIntImage is raw-allocated (XMemAlloc, sizeof 0x18) and
// placement-constructed with the extent constructor, which writes its own status through
// lpiResult. The planes are built U, then V, then Y (matching the asm's store order to
// +0x3C/+0x40/+0x38). Any allocation failure sets *lpiResult = -3; any plane whose extent
// constructor reported an error (*lpiResult != 0) aborts without overwriting that code. On
// either failure the partially built picture is torn down through Clean() -- which is why the
// plane pointers are zero-initialised before the first allocation (Clean walks all three).
CPictureFYUV420::CPictureFYUV420(s32* lpiResult, const CRct& lrLumaRect, const CRct& lrChromaRect)
    : muPlaneY(0)
    , muPlaneU(0)
    , muPlaneV(0)
{
    // Cache both extents (the inlined CRct default-construct that precedes each copy in the
    // asm sets an empty {0,0,-1,-1} extent that is immediately overwritten here).
    mrcLuma   = lrLumaRect;
    mrcChroma = lrChromaRect;

    miLumaWidth  = mrcLuma.miRight  - mrcLuma.miLeft;
    miLumaHeight = mrcLuma.miBottom - mrcLuma.miTop;
    miLumaArea   = miLumaWidth * miLumaHeight;

    miChromaWidth  = mrcChroma.miRight  - mrcChroma.miLeft;
    miChromaHeight = mrcChroma.miBottom - mrcChroma.miTop;
    miChromaArea   = miChromaWidth * miChromaHeight;

    // ---- U plane (chroma extent) ----------------------------------------------------
    void* lpMemU = XMemAlloc(sizeof(CIntImage), KU_PICTUREFYUV420_XMEM_ATTRIBUTES);
    muPlaneU = lpMemU ? lFromPtr(new (lpMemU) CIntImage(lpiResult, lrChromaRect, 0)) : 0u;
    if (!muPlaneU)
    {
        *lpiResult = -3;
        Clean();
        return;
    }
    if (*lpiResult)
    {
        Clean();
        return;
    }

    // ---- V plane (chroma extent) ----------------------------------------------------
    void* lpMemV = XMemAlloc(sizeof(CIntImage), KU_PICTUREFYUV420_XMEM_ATTRIBUTES);
    muPlaneV = lpMemV ? lFromPtr(new (lpMemV) CIntImage(lpiResult, lrChromaRect, 0)) : 0u;
    if (!muPlaneV)
    {
        *lpiResult = -3;
        Clean();
        return;
    }
    if (*lpiResult)
    {
        Clean();
        return;
    }

    // ---- Y plane (luma extent) ------------------------------------------------------
    void* lpMemY = XMemAlloc(sizeof(CIntImage), KU_PICTUREFYUV420_XMEM_ATTRIBUTES);
    muPlaneY = lpMemY ? lFromPtr(new (lpMemY) CIntImage(lpiResult, lrLumaRect, 0)) : 0u;
    if (!muPlaneY)
    {
        *lpiResult = -3;
        Clean();
        return;
    }
    if (*lpiResult)
    {
        Clean();
    }
}

// CPictureFYUV420::~CPictureFYUV420 @0x82A7DBF0 -- non-virtual; tail-calls Clean.
CPictureFYUV420::~CPictureFYUV420()
{
    Clean();
}

// CPictureFYUV420::Clean @0x82A7DB30 -- release every allocated plane.
//
// First clean()s all three planes (frees their pixel buffers), then destroys and frees each
// plane object, zeroing its pointer. The asm performs the two sweeps separately in exactly
// this order (the destructor's own buffer-free is therefore redundant with the preceding
// clean(), but both are reproduced faithfully).
void CPictureFYUV420::Clean()
{
    if (muPlaneY) { lPlane(muPlaneY)->clean(); }
    if (muPlaneU) { lPlane(muPlaneU)->clean(); }
    if (muPlaneV) { lPlane(muPlaneV)->clean(); }

    if (muPlaneY)
    {
        lPlane(muPlaneY)->~CIntImage();
        XMemFree(lToPtr(muPlaneY), KU_PICTUREFYUV420_XMEM_ATTRIBUTES);
        muPlaneY = 0;
    }
    if (muPlaneU)
    {
        lPlane(muPlaneU)->~CIntImage();
        XMemFree(lToPtr(muPlaneU), KU_PICTUREFYUV420_XMEM_ATTRIBUTES);
        muPlaneU = 0;
    }
    if (muPlaneV)
    {
        lPlane(muPlaneV)->~CIntImage();
        XMemFree(lToPtr(muPlaneV), KU_PICTUREFYUV420_XMEM_ATTRIBUTES);
        muPlaneV = 0;
    }
}

// CPictureFYUV420::getPlane @0x82A7DAF8 -- Y/U/V plane by index (0/1/2), else 0.
CIntImage* CPictureFYUV420::getPlane(s32 iPlane)
{
    switch (iPlane)
    {
        case 0: return lPlane(muPlaneY);   // +0x38  Y
        case 1: return lPlane(muPlaneU);   // +0x3C  U
        case 2: return lPlane(muPlaneV);   // +0x40  V
    }
    return 0;
}
