// =====================================================================================
// CPictureFYUV420 -- planar YUV 4:2:0 picture/frame for the RTCMV/WMV video-object
// encoder. Owns three CIntImage planes (Y at luma resolution, U and V at chroma
// resolution) plus their cached extents/areas. Constructed and torn down by
// CWMVideoObjectEncoder (initMultiThreadVars / resetMultiThreadVars / clean).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CPictureFYUV420::CPictureFYUV420  @0x82A7DBF8
//   CPictureFYUV420::~CPictureFYUV420 @0x82A7DBF0
//   CPictureFYUV420::Clean            @0x82A7DB30
//   CPictureFYUV420::getPlane         @0x82A7DAF8
//
// Layout (0x44 bytes, no vtable -- the destructor tail-calls Clean and is non-virtual):
//   +0x00 mrcLuma          CRct luma-plane extent   {left,top,right,bottom}
//   +0x10 mrcChroma        CRct chroma-plane extent (half size for 4:2:0)
//   +0x20 miLumaArea       lumaWidth   * lumaHeight
//   +0x24 miChromaArea     chromaWidth * chromaHeight
//   +0x28 miLumaWidth      mrcLuma.right   - mrcLuma.left
//   +0x2C miLumaHeight     mrcLuma.bottom  - mrcLuma.top
//   +0x30 miChromaWidth    mrcChroma.right  - mrcChroma.left
//   +0x34 miChromaHeight   mrcChroma.bottom - mrcChroma.top
//   +0x38 muPlaneY         CIntImage* (Y) -- getPlane(0)
//   +0x3C muPlaneU         CIntImage* (U) -- getPlane(1)
//   +0x40 muPlaneV         CIntImage* (V) -- getPlane(2)
//
// The three plane fields hold X360 (32-bit) heap-object pointers, stored as u32 exactly as
// the X360 `stw` writes them (same convention as the sibling CIntImage / CVideoFrameAnalyst
// TUs); they are converted through lToPtr/lFromPtr on the boundary. The CIntImage objects
// are raw-allocated via XMemAlloc(sizeof(CIntImage), 0x248C8000) and placement-constructed.
// =====================================================================================
#pragma once

#include <cstddef>

#include "types.hpp"
#include "SDKs/EATech/rwmovie/CIntImage.h"   // CRct + CIntImage (their real homes)

class CPictureFYUV420
{
public:
    // Builds the three planes for the supplied luma/chroma extents. The out-parameter
    // lpiResult receives 0 on success or a negative CIntImage error code (-3 on allocation
    // failure) if any plane could not be built. lrLumaRect drives the Y plane; lrChromaRect
    // drives the U and V planes.
    CPictureFYUV420(s32* lpiResult, const CRct& lrLumaRect, const CRct& lrChromaRect);

    // Non-virtual: destruction is a plain tail-call to Clean (no vptr on this object).
    ~CPictureFYUV420();

    // Cleans then destroys+frees any allocated plane, resetting each plane pointer to 0.
    void Clean();

    // Returns the CIntImage plane for index 0 (Y), 1 (U) or 2 (V); 0 for any other index.
    CIntImage* getPlane(s32 iPlane);

private:
    CRct mrcLuma;         // +0x00  luma-plane extent
    CRct mrcChroma;       // +0x10  chroma-plane extent
    s32  miLumaArea;      // +0x20  lumaWidth * lumaHeight
    s32  miChromaArea;    // +0x24  chromaWidth * chromaHeight
    s32  miLumaWidth;     // +0x28  mrcLuma.right - mrcLuma.left
    s32  miLumaHeight;    // +0x2C  mrcLuma.bottom - mrcLuma.top
    s32  miChromaWidth;   // +0x30  mrcChroma.right - mrcChroma.left
    s32  miChromaHeight;  // +0x34  mrcChroma.bottom - mrcChroma.top
    u32  muPlaneY;        // +0x38  CIntImage* Y plane
    u32  muPlaneU;        // +0x3C  CIntImage* U plane
    u32  muPlaneV;        // +0x40  CIntImage* V plane

    // Pin the byte layout so the plane pointers and cached extents land at their attested
    // X360 offsets (getPlane reads +0x38/+0x3C/+0x40 directly).
    static void _AssertLayout()
    {
        static_assert(offsetof(CPictureFYUV420, mrcChroma) == 0x10, "mrcChroma @ +0x10");
        static_assert(offsetof(CPictureFYUV420, miLumaArea) == 0x20, "miLumaArea @ +0x20");
        static_assert(offsetof(CPictureFYUV420, miChromaArea) == 0x24, "miChromaArea @ +0x24");
        static_assert(offsetof(CPictureFYUV420, miLumaWidth) == 0x28, "miLumaWidth @ +0x28");
        static_assert(offsetof(CPictureFYUV420, miLumaHeight) == 0x2C, "miLumaHeight @ +0x2C");
        static_assert(offsetof(CPictureFYUV420, miChromaWidth) == 0x30, "miChromaWidth @ +0x30");
        static_assert(offsetof(CPictureFYUV420, miChromaHeight) == 0x34, "miChromaHeight @ +0x34");
        static_assert(offsetof(CPictureFYUV420, muPlaneY) == 0x38, "muPlaneY @ +0x38");
        static_assert(offsetof(CPictureFYUV420, muPlaneU) == 0x3C, "muPlaneU @ +0x3C");
        static_assert(offsetof(CPictureFYUV420, muPlaneV) == 0x40, "muPlaneV @ +0x40");
        static_assert(sizeof(CPictureFYUV420) == 0x44, "CPictureFYUV420 size == 68");
    }
};
