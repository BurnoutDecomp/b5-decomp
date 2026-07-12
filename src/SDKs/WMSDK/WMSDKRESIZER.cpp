// ===========================================================================
// SDKs/WMSDK/WMSDKRESIZER.cpp
//
// WMSDKRESIZER -- Windows-Media (WMSDK) image/video resizer, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Format FourCC constants are the exact 32-bit values
// from the X360 asm (these equal the MAKEFOURCC integer for the format on any
// platform, so the comparisons are endianness-independent).
//
// Fidelity notes:
//  * Shifts mirror the pseudocode operators exactly: `>> 3` == srawi (floor),
//    `/ 8` / `/ 2` / `/ 4` == srawi+addze (toward-zero) -- reproducing the exact
//    rounding the X360 compiler emitted.
//  * CopyBuffer2D returns r3 verbatim: the X360 body leaves the last memcpy
//    return value (a destination pointer) in r3, so the reconstruction returns
//    that pointer truncated to int. The value is a copy-loop artifact; the real
//    output is the memcpy side effect. On the 32-bit console the pointer fit r3
//    exactly; the truncation here only drops high bits the caller does not use.
// ===========================================================================

#include "SDKs/WMSDK/WMSDKRESIZER.h"
#include "SDKs/WMSDK/CClipResize.h"
#include "SDKs/WMSDK/CMSMtoN.h"

#include <cstring>  // memcpy
#include <cstdint>  // intptr_t

// XMem attribute word baked into every alloc/free here (lis 0x248C ; ori 0x8000).
static const u32 KU_WM_XMEM_ATTRIBUTES = 0x248C8000u;

// Signed abs matching the srawi/xor/subf idiom in the asm.
static inline s32 WM_Abs32(s32 v) { return v < 0 ? -v : v; }

// ---------------------------------------------------------------------------
// WMSDKRESIZER::BMPSize  @ 0x82A44BF0
// ---------------------------------------------------------------------------
u32 WMSDKRESIZER::BMPSize(s32 iWidth, s32 iHeight)
{
    const BITMAPINFOHEADER* pInfo = mpBitmapInfo;   // *(this+4)
    u32 uCompression = pInfo->biCompression;        // *(v3+16)
    s32 iBitCount    = pInfo->biBitCount;           // *(v3+14)  (lhz -> zero-extended)

    if (uCompression == 0x56555949u /*IYUV*/ || uCompression == 0x30323449u /*I420*/ ||
        uCompression == 0x32315659u /*YV12*/ || uCompression == 0x3231564Eu /*NV12*/ ||
        uCompression == 0x3131564Eu /*NV11*/)
    {
        return WM_Abs32(iBitCount * iWidth * iHeight / 8);
    }
    // Non-planar: round each row up to a DWORD (4-byte) boundary, times height.
    return WM_Abs32(((iBitCount * iWidth / 8 + 3) / 4) * iHeight * 4);
}

// ---------------------------------------------------------------------------
// WMSDKRESIZER::getType  @ 0x82A44B60
// ---------------------------------------------------------------------------
int WMSDKRESIZER::getType()
{
    const BITMAPINFOHEADER* pInfo = mpBitmapInfo;   // *(this+4)
    if (!pInfo)
        return 0;

    u32 uCompression = pInfo->biCompression;        // *(v1+16)
    if (uCompression == 0x56555949u /*IYUV*/ || uCompression == 0x30323449u /*I420*/ ||
        uCompression == 0x32315659u /*YV12*/ || uCompression == 0x39555659u /*YVU9*/)
    {
        return 1;
    }
    if (uCompression == 0x3231564Eu /*NV12*/ || uCompression == 0x3131564Eu /*NV11*/)
        return 3;
    return 2;
}

// ---------------------------------------------------------------------------
// WMSDKRESIZER::Reset  @ 0x82A44CA0
// (pseudocode register aliases resolved: v5 == this, v7 == mpBitmapInfo.)
// ---------------------------------------------------------------------------
int WMSDKRESIZER::Reset()
{
    s32 iSrcHeight = mSrcHeight;                    // v1 = this[9] (0x24)
    BITMAPINFOHEADER* pOut = mpBitmapInfo;          // this[1]
    mResizeFlags = 0;                               // this[87] = 0

    if (mProgressiveToInterlaced)                   // this[86] (0x158)
    {
        mResizeFlags = 3;
        pOut->biHeight = mDstHeight / 2;
    }
    else
    {
        if (mDstWidth != mAllocDstWidth || mDstHeight != mAllocDstHeight)
            mResizeFlags = 1;
        if (mSrcWidth != mAllocSrcWidth || iSrcHeight != mAllocSrcHeight)
            mResizeFlags += 2;
        pOut->biHeight = mDstHeight;
    }

    pOut->biWidth     = mDstWidth;                                       // *(this[1]+4)
    pOut->biSizeImage = BMPSize(pOut->biWidth, pOut->biHeight);          // *(this[1]+20)

    if (mUseClipResize)                             // this[85] (0x154)
        CClipResize_Init(reinterpret_cast<CClipResize*>(&mClipResizeState),
                         mDstWidth, pOut->biHeight, mSrcWidth);
    else
        reinterpret_cast<CMSMtoN*>(&mMStoNState)->Reset(
            mDstWidth, pOut->biHeight, mSrcWidth);

    return 1;
}

// ---------------------------------------------------------------------------
// WMSDKRESIZER::allocBuffer  @ 0x82A45C70
// ---------------------------------------------------------------------------
int WMSDKRESIZER::allocBuffer()
{
    if (mpBuffer0)
    {
        XMemFree(mpBuffer0, KU_WM_XMEM_ATTRIBUTES);
        mpBuffer0 = nullptr;
    }
    if (mpBuffer1)
    {
        XMemFree(mpBuffer1, KU_WM_XMEM_ATTRIBUTES);
        mpBuffer1 = nullptr;
    }

    u32 uSize0 = BMPSize(mAllocDstWidth, mAllocDstHeight);   // this[10], this[11]
    u32 uSize1 = BMPSize(mAllocSrcWidth, mAllocSrcHeight);   // this[12], this[13]

    mpBuffer0 = static_cast<u8*>(XMemAlloc(uSize0, KU_WM_XMEM_ATTRIBUTES));
    if (!mpBuffer0)
        return 0;

    mpBuffer1 = static_cast<u8*>(XMemAlloc(uSize1, KU_WM_XMEM_ATTRIBUTES));
    return mpBuffer1 != nullptr;
}

// ---------------------------------------------------------------------------
// WMSDKRESIZER::~WMSDKRESIZER  @ 0x82A43F80
// Frees the scratch buffers + output header, then destroys the two embedded
// resamplers in the asm's order (CClipResize first, then CMSMtoN).
// ---------------------------------------------------------------------------
WMSDKRESIZER::~WMSDKRESIZER()
{
    if (mpBuffer0)
    {
        XMemFree(mpBuffer0, KU_WM_XMEM_ATTRIBUTES);
        mpBuffer0 = nullptr;
    }
    if (mpBuffer1)
    {
        XMemFree(mpBuffer1, KU_WM_XMEM_ATTRIBUTES);
        mpBuffer1 = nullptr;
    }
    if (mpBitmapInfo)
    {
        XMemFree(mpBitmapInfo, KU_WM_XMEM_ATTRIBUTES);
        mpBitmapInfo = nullptr;
    }

    reinterpret_cast<CClipResize*>(&mClipResizeState)->~CClipResize();
    reinterpret_cast<CMSMtoN*>(&mMStoNState)->~CMSMtoN();
}

// ---------------------------------------------------------------------------
// WMSDKRESIZER::putField  @ 0x82A458B0
// Copies the internal source buffer (mpBuffer1) into the caller's frame buffer
// as interlaced fields: planar (type 1) or packed (type 2).
// ---------------------------------------------------------------------------
int WMSDKRESIZER::putField(u8* pDst, int iField, int iFlag)
{
    if (!pDst || !mpBuffer0)                 // !a2 || !*(this+352)
        return 0;

    int iType = getType();

    // ---- Type 1: planar Y + two chroma planes -----------------------------
    if (iType == 1)
    {
        u32 uCompression = mpBitmapInfo->biCompression;   // *(this[1]+16)

        int iLumaW = 0, iChromaW = 0, iChromaW2 = 0;      // v7, v8, v9
        int iLumaRows = 0, iCURows = 0, iCVRows = 0;      // v11, v12, v13
        int iCacheW = 0, iCacheHW = 0;                    // v14, v15
        int iOffY = 0, iOffU = 0, iOffV = 0;              // v16, v17, v18

        if (uCompression == 0x56555949u /*IYUV*/ ||
            uCompression == 0x30323449u /*I420*/ ||
            uCompression == 0x32315659u /*YV12*/)
        {
            iLumaW    = mSrcWidth;                        // v7  = this[8]
            iCacheW   = mAllocSrcWidth;                   // v14 = this[12]
            iChromaW  = iLumaW / 2;                       // v8
            int iOx   = mDstOriginX;                      // v19 = this[6]
            int iOy   = mDstOriginY;                      // v20 = this[7]
            iChromaW2 = iLumaW / 2;                       // v9
            iLumaRows = mSrcHeight / 2;                   // v11 = this[9]/2
            int iField21 = mAllocSrcHeight * iCacheW;     // v21 = this[13]*v14
            iCURows   = iLumaRows / 2;                    // v12
            iCVRows   = iLumaRows / 2;                    // v13
            iCacheHW  = iCacheW / 2;                      // v15

            if (iField)
            {
                int t  = (iOy / 2 + 1) * iCacheHW;        // v24
                iOffY  = (iOy + 1) * iCacheW + iOx;       // v16
                int t2 = iOx / 2 + t;                     // v25
                iOffV  = 5 * iField21 / 4 + iOx / 2 + t;  // v18
                iOffU  = t2 + iField21;                   // v17
            }
            else
            {
                int t  = iOy * iCacheHW / 2;              // v22
                int t2 = 5 * iField21 / 4;                // v23
                iOffY  = iOy * iCacheW + iOx;             // v16
                iOffU  = iOx / 2 + iField21 + t;          // v17
                iOffV  = t2 + iOx / 2 + t;                // v18
            }
        }

        u8* pSrc     = mpBuffer1;                         // v26 = this[89]
        u8* pDstY    = pDst + iOffY;                      // v27
        u8* pDstU    = pDst + iOffU;                      // v28
        u8* pDstV    = pDst + iOffV;                      // v29
        int iYStride = 2 * iCacheW;                       // v30
        int iCStride = 2 * iCacheHW;                      // v31

        for (int r = iLumaRows; r > 0; --r)
        {
            memcpy(pDstY, pSrc, static_cast<size_t>(iLumaW));
            pDstY += iYStride;
            pSrc  += iLumaW;
        }
        for (int r = iCURows; r > 0; --r)
        {
            memcpy(pDstU, pSrc, static_cast<size_t>(iChromaW));
            pDstU += iCStride;
            pSrc  += iChromaW;
        }
        for (int r = iCVRows; r > 0; --r)
        {
            memcpy(pDstV, pSrc, static_cast<size_t>(iChromaW2));
            pDstV += iCStride;
            pSrc  += iChromaW2;
        }
        return 1;
    }

    // ---- Type 2: single packed field --------------------------------------
    if (iType == 2)
    {
        const BITMAPINFOHEADER* pInfo = mpBitmapInfo;     // v35 = this[1]
        int iCacheW    = mAllocSrcWidth;                  // v36 = this[12]
        int iBits      = pInfo->biBitCount;               // v37 = *(v35+14)
        int iAbsCacheW = iCacheW;                          // v38
        int iRowBytes  = ((mSrcWidth * iBits + 31) & ~31) >> 3;   // v39
        int iDir;                                          // v40
        if (iCacheW > 0)
        {
            iDir = 1;
        }
        else
        {
            iAbsCacheW = -iCacheW;
            iDir = -1;
        }
        int iSrcH   = mSrcHeight;                          // v41 = this[9]
        int iStride = (((iAbsCacheW * iBits + 31) & ~31) / 8) * iDir;   // v44
        int iRows   = iSrcH / 2;                           // v45
        int bSwap   = 0;                                   // v43
        if (!iFlag && pInfo->biCompression <= 3u)
            bSwap = (pInfo->biHeight > 0) ? 1 : 0;

        u8* pSrc = mpBuffer1;                              // v46 = this[89]
        int iOff1, iOff2;                                  // v48, v49
        if (iField)
        {
            if (bSwap)
            {
                int a  = mDstOriginX * iBits;             // v52
                int b  = WM_Abs32(mAllocSrcHeight) + 1 - mDstOriginY - iSrcH;   // v53
                int c  = a >> 3;                          // v54
                int d  = (a < 0 && (a & 7) != 0) ? 1 : 0; // v55
                iOff2  = b * iStride;                     // v49
                iOff1  = c + d;                           // v48
            }
            else
            {
                int e  = mDstOriginY + 1;                 // v51
                iOff1  = mDstOriginX * iBits / 8;         // v48
                iOff2  = e * iStride;                     // v49
            }
        }
        else if (bSwap)
        {
            int f = WM_Abs32(mAllocSrcHeight) - mDstOriginY;   // v50
            iOff1 = mDstOriginX * iBits / 8;              // v48
            iOff2 = (f - iSrcH) * iStride;                // v49
        }
        else
        {
            int g = mDstOriginX * iBits;                  // v47
            iOff1 = mDstOriginY * iStride;                // v48
            iOff2 = g / 8;                                // v49
        }

        int iDstStride = 2 * iStride;                     // v56
        u8* pRow = pDst + iOff2 + iOff1;                  // v57
        for (int r = iRows; r > 0; --r)
        {
            memcpy(pRow, pSrc, static_cast<size_t>(iRowBytes));
            pRow += iDstStride;
            pSrc += iRowBytes;
        }
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// WMSDKRESIZER::CopyBuffer2D  @ 0x82A44380
// Format-dispatched YUV/RGB plane copier with full cropping.
// ---------------------------------------------------------------------------
int WMSDKRESIZER::CopyBuffer2D(
    int a1, u8* pDst, u8* pSrc, int iDstStride, int iDstHeight, int iSrcStride,
    int iSrcHeight, int iSrcOffset, int a9, int a10, int a11, int a12, int a13,
    int a14, int a15, int a16, int a17, int a18, int a19, int a20, int a21,
    int a22, int a23, int a24, int a25, int a26, int a27, int iRect28, int a29,
    int iRect30, int a31, int iRect32, int a33, int iCopyWidth, int a35,
    int iCopyHeight, int a37, u32 uFourCC, int a39, int iBitDepth, int a41,
    int bDstFlip, int a43, int bSrcFlip)
{
    // Unused positional arguments (kept to preserve the call signature).
    (void)a1;  (void)a9;  (void)a10; (void)a11; (void)a12; (void)a13; (void)a14;
    (void)a15; (void)a16; (void)a17; (void)a18; (void)a19; (void)a20; (void)a21;
    (void)a22; (void)a23; (void)a24; (void)a25; (void)a26; (void)a27; (void)a29;
    (void)a31; (void)a33; (void)a35; (void)a37; (void)a39; (void)a41; (void)a43;

    int iDstStrideAbs = iDstStride;                       // v44 = a4
    if ((bDstFlip || (uFourCC != 0 && uFourCC != 3)) && iDstHeight <= 0)
        iDstHeight = -iDstHeight;
    if ((bSrcFlip || (uFourCC != 0 && uFourCC != 3)) && iSrcHeight <= 0)
        iSrcHeight = -iSrcHeight;

    int iDstDir = 0;                                      // v46
    intptr_t rResult = 0;                                 // r3

    // ===== Planar formats: Y plane + two chroma planes (LABEL_18/24/17 -> _20)
    if (uFourCC == 0x56555949u /*IYUV*/ || uFourCC == 0x30323449u /*I420*/ ||
        uFourCC == 0x32315659u /*YV12*/ || uFourCC == 0x50343232u /*24-family*/ ||
        uFourCC == 0x39555659u /*YVU9*/)
    {
        int v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v63;

        if (uFourCC == 0x50343232u)
        {
            // v50 = 24: chroma full height, half width
            v48 = iCopyWidth;   v52 = iRect30;      v51 = iDstStride / 2;
            v49 = iCopyHeight;  v53 = iRect28;      v54 = iRect32;
            v57 = iCopyHeight;  v55 = iSrcStride / 2; v60 = iRect28;
            v56 = iCopyWidth / 2; v63 = iRect32;    v58 = iSrcOffset / 2;
            v59 = iRect30 / 2;  v50 = 24;
        }
        else if (uFourCC == 0x39555659u /*YVU9*/)
        {
            // v50 = 17: 4:1:0 (quarter)
            v48 = iCopyWidth;   v49 = iCopyHeight;  v50 = 17;
            v51 = iDstStride / 4; v52 = iRect30;    v53 = iRect28;
            v54 = iRect32;      v55 = iSrcStride / 4; v56 = iCopyWidth / 4;
            v57 = iCopyHeight / 4; v58 = iSrcOffset / 4; v59 = iRect30 / 4;
            v60 = iRect28 / 4;
            int v62 = (iRect32 < 0 && (iRect32 & 3) != 0) ? 1 : 0;
            int v61 = iRect32 >> 2;
            v63 = v61 + v62;
        }
        else
        {
            // v50 = 20: IYUV / I420 / YV12 (4:2:0, half)
            v48 = iCopyWidth;   v49 = iCopyHeight;  v50 = 20;
            v51 = iDstStride / 2; v52 = iRect30;    v53 = iRect28;
            v54 = iRect32;      v55 = iSrcStride / 2; v56 = iCopyWidth / 2;
            v57 = iCopyHeight / 2; v58 = iSrcOffset / 2; v59 = iRect30 / 2;
            v60 = iRect28 / 2;
            int v62 = (iRect32 < 0 && (iRect32 & 1) != 0) ? 1 : 0;
            int v61 = iRect32 >> 1;
            v63 = v61 + v62;
        }

        int v64 = iDstStrideAbs * iDstHeight;
        int v65 = v60 * v51;
        int v66 = iSrcStride * iSrcHeight;
        int v67 = v63 * v55;
        u8* v68 = pSrc + (iDstStrideAbs * v53 + iSrcOffset);   // src luma
        int v69 = v65 + v64 + v58;
        int v50x64 = v50 * v64 / 16 + v65;                     // pre-loop r3
        int v70 = v50x64 + v58;
        int v71 = v67 + v66 + v59;
        int v72 = v50 * v66 / 16 + v67 + v59;
        u8* v73 = pDst + (iSrcStride * v54 + v52);             // dst luma
        u8* v74 = pSrc + v69;
        u8* v75 = pSrc + v70;
        u8* v76 = pDst + v71;
        u8* v77 = pDst + v72;
        if (uFourCC == 0x32315659u /*YV12*/)                   // swap the two chroma planes
        {
            v76 = pDst + v72;
            v75 = pSrc + v69;
            v74 = pSrc + v70;
            v77 = pDst + v71;
        }

        rResult = v50x64;
        for (int r = v49; r > 0; --r)                          // Y plane
        {
            rResult = reinterpret_cast<intptr_t>(memcpy(v73, v68, static_cast<size_t>(v48)));
            v68 += iDstStrideAbs;
            v73 += iSrcStride;
        }
        for (int r = v57; r > 0; --r)                          // U and V planes
        {
            memcpy(v76, v74, static_cast<size_t>(v56));
            v74 += v51;
            v76 += v55;
            rResult = reinterpret_cast<intptr_t>(memcpy(v77, v75, static_cast<size_t>(v56)));
            v75 += v51;
            v77 += v55;
        }
        return static_cast<int>(rResult);
    }

    // ===== Packed / RGB formats: single linear stride copy (LABEL_56/0-3 -> _42)
    if (uFourCC == 0 || uFourCC == 3)
    {
        // RGB (uncompressed): direction from the flip flags.
        iDstDir = bDstFlip ? (iDstStride <= 0 ? -1 : 1) : (iDstHeight > 0 ? -1 : 1);
        rResult = bSrcFlip ? (iSrcStride > 0 ? 1 : -1) : (iSrcHeight <= 0 ? 1 : -1);
    }
    else if (uFourCC == 0x32595559u /*YUY2*/ || uFourCC == 0x50313459u ||
             uFourCC == 0x56323136u /*V216*/ ||
             uFourCC == 0x55595659u /*YVYU*/ || uFourCC == 0x54313459u ||
             uFourCC == 0x41595556u)
    {
        // Packed 4:2:2 family. NOTE: 0x59565955 (UYVY) and 0x54323459 (Y42T) are
        // deliberately NOT in this set -- the asm's `b loc_82A44614` compare (!= 0x41595556)
        // falls them through to the UNKNOWN default (iDstDir=0, result=0) rather than the
        // packed formula.
        iDstDir = bDstFlip ? (iDstStride > 0 ? 1 : -1) : 1;
        rResult = bSrcFlip ? (iSrcStride > 0 ? 1 : -1) : 1;
    }
    // else: unknown format -> iDstDir/rResult stay 0.

    int result = static_cast<int>(rResult);                    // src direction / status
    if (iDstStride <= 0)
        iDstStrideAbs = -iDstStride;

    int v80 = ((iDstStrideAbs * iBitDepth + 31) & ~31) >> 3;
    int v81 = v80 * iDstDir;
    int v82;
    if (bDstFlip)
    {
        v82 = v81 * iRect28;
    }
    else if (iDstDir == 1)
    {
        v82 = v81 * iRect28;
    }
    else
    {
        int v83 = v80 * iDstDir;
        if (v81 <= 0)
            v83 = -v81;
        int v84 = iDstHeight;
        if (iDstHeight <= 0)
            v84 = -iDstHeight;
        v82 = (v84 - iRect28 - 1) * v83;
    }

    int v85 = ((iSrcOffset * iBitDepth) >> 3) + v82;
    if (iSrcStride <= 0)
        iSrcStride = -iSrcStride;
    int v86 = ((iSrcStride * iBitDepth + 31) & ~31) >> 3;
    int v87 = v86 * result;
    int v88;
    if (bSrcFlip)
    {
        v88 = v87 * iRect32;
    }
    else if (result == 1)
    {
        v88 = v87 * iRect32;
    }
    else
    {
        int v89 = v86 * result;
        if (v87 <= 0)
            v89 = -v87;
        int v90 = iSrcHeight;
        if (iSrcHeight <= 0)
            v90 = -iSrcHeight;
        v88 = (v90 - iRect32 - 1) * v89;
    }

    u8* v91 = pSrc + v85;                                       // src row ptr
    u8* v93 = pDst + (((iRect30 * iBitDepth) >> 3) + v88);      // dst row ptr
    int iCopyBytes = ((iCopyWidth * iBitDepth + 31) & ~31) >> 3;
    for (int r = iCopyHeight; r > 0; --r)
    {
        rResult = reinterpret_cast<intptr_t>(memcpy(v93, v91, static_cast<size_t>(iCopyBytes)));
        v91 += v81;
        v93 += v87;
    }
    return static_cast<int>(rResult);
}
