// =====================================================================================
// CVideoFrameAnalyst -- per-frame analysis record for the RTCMV/WMV video-object encoder.
// See CVideoFrameAnalyst.h for the byte layout; CVideoAnalyst owns an array of these.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   CVideoFrameAnalyst::Init                           @0x82A486E0  (bodied here)
//   CVideoFrameAnalyst::detectHighTextureBand          @0x82A47318  (bodied here)
//   CVideoFrameAnalyst::~CVideoFrameAnalyst            @0x82A487E8  (bodied here)
//   CVideoFrameAnalyst::`vector deleting destructor'   @0x82A490C8  (bodied here)
//
//   CVideoFrameAnalyst::computeBlockAvg                @0x82A47450  (bodied here)
//   CVideoFrameAnalyst::computeFrameTextureMap         @0x82A47640  (bodied here)
// The analysis kernels dispatch through the ARTIST virtual routine table.
//
// The four leading words are heap-buffer pointers stored X360-width (32-bit), matching the
// asm `stw`/`lwz`; the record keeps X360 widths so its array stride stays byte-exact (see
// KU_VIDEOFRAMEANALYST_STRIDE and CVideoFrameAnalyst::_AssertLayout).
// =====================================================================================
#include "types.hpp"

#include <cstdlib>
#include <cstring>

#include "CVideoFrameAnalyst.h"
#include "CWMVFARoutines.h"

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
// The asm calls XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD) directly (r3 =
// size/pAddress, r4 = attributes), with no `this` argument -- matching the sibling
// CVideoAnalyst / CIntImage / CQueue rwmovie TUs.
#include <xtl.h> // XDK XMemAlloc / XMemFree

// XMemAlloc/XMemFree attribute word for this codec's analysis pools (lis 0x248C / ori 0x8000).
static const u32 KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES = 0x248C8000u;

// The two 4*area allocations cap the element count before multiplying by 4: on overflow the
// asm passes 0xFFFFFFFF so the allocation fails (cmplw area, 0x3FFFFFFF).
static const u32 KU_AREA_TIMES4_LIMIT = 0x3FFFFFFFu;

// The four leading words hold X360 (32-bit) heap-buffer pointers; convert on the boundary.
static inline void* lToPtr(u32 luValue)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(luValue));
}
static inline u32 lFromPtr(void* lpValue)
{
    return static_cast<u32>(reinterpret_cast<uintptr_t>(lpValue));
}

// CVideoFrameAnalyst::Init @0x82A486E0.
// Frees any prior buffers, records the frame geometry, and (re)allocates the three analysis
// buffers sized off the macro-block area. Returns the last allocation (vestigial r3).
int CVideoFrameAnalyst::Init(u32 uWidth, s32 iHeight)
{
    if (muField00)
    {
        XMemFree(lToPtr(muField00), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField00 = 0;
    }
    if (muField04)
    {
        XMemFree(lToPtr(muField04), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField04 = 0;
    }
    if (muField0C)
    {
        XMemFree(lToPtr(muField0C), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField0C = 0;
    }

    muField4020 = uWidth;                       // frame width
    muField4024 = static_cast<u32>(iHeight);    // frame height
    muSourceId  = 0xFFFFFFFFu;                  // no source frame recorded yet

    // 4 * blocks-wide * blocks-high, both rounded up to a 16-pixel macro-block grid.
    u32 luBlocksW = (uWidth + 15) >> 4;
    u32 luBlocksH = (static_cast<u32>(iHeight) + 15) >> 4;
    u32 luArea    = 4u * luBlocksW * luBlocksH;

    muArea          = luArea;
    muWidthInBlocks = uWidth >> 4;
    muAreaHalf      = static_cast<u32>(static_cast<s32>(luArea) >> 1);

    // Buffer 0: one word per macro-block area unit.
    muField00 = lFromPtr(XMemAlloc(luArea, KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES));

    // Buffers 1 and 3: 4 * area each, with the overflow guard the asm applies.
    u32 luArea1     = muArea;
    u32 luBytes1    = (luArea1 > KU_AREA_TIMES4_LIMIT) ? 0xFFFFFFFFu : 4u * luArea1;
    muField04       = lFromPtr(XMemAlloc(luBytes1, KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES));

    u32 luArea3     = muArea;
    u32 luBytes3    = (luArea3 > KU_AREA_TIMES4_LIMIT) ? 0xFFFFFFFFu : 4u * luArea3;
    void* lpBuffer3 = XMemAlloc(luBytes3, KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
    muField0C       = lFromPtr(lpBuffer3);

    return static_cast<int>(reinterpret_cast<intptr_t>(lpBuffer3));
}

// CVideoFrameAnalyst::detectHighTextureBand @0x82A47318.
// Slides a weighted, width-10 window across maActivityHistogram (bins 1..3071, weight ==
// bin index) and finds the window with the greatest weighted activity. Reports the band
// position via *pOutBand and returns 1 when that band is both far enough into the histogram
// (> 30) and dominant (10 * best > running total); otherwise returns 0. All sums are 64-bit,
// each term a 32-bit product zero-extended -- exactly as the asm (mullw + clrldi + cmpld).
int CVideoFrameAnalyst::detectHighTextureBand(s32* pOutBand)
{
    const s32* lpHistogram = maActivityHistogram;

    // Seed the running total and the window with bins 1..9 (weights 1..9).
    u64 luTotal = 0;
    for (u32 luWeight = 1; luWeight <= 9; ++luWeight)
    {
        luTotal += static_cast<u64>(static_cast<u32>(luWeight * static_cast<u32>(lpHistogram[luWeight])));
    }

    u64 luWindow   = luTotal;
    u64 luBest     = luTotal;
    s32 liBestPos  = 10;

    for (u32 luPos = 10; luPos < 3072; ++luPos)
    {
        u64 luAdd = static_cast<u64>(static_cast<u32>(luPos * static_cast<u32>(lpHistogram[luPos])));
        luTotal  += luAdd;
        luWindow += luAdd - static_cast<u64>(static_cast<u32>((luPos - 10) * static_cast<u32>(lpHistogram[luPos - 10])));
        if (luWindow > luBest)
        {
            luBest    = luWindow;
            liBestPos = static_cast<s32>(luPos);
        }
    }

    s32 liBand = liBestPos - 10;
    if (static_cast<u32>(liBand) <= 30u)
    {
        return 0;
    }
    if (static_cast<u64>(10) * luBest <= luTotal)
    {
        return 0;
    }

    *pOutBand = liBand;
    return 1;
}

// CVideoFrameAnalyst::~CVideoFrameAnalyst @0x82A487E8.
// Frees the four analysis buffers (order 0, 1, 3, 2 as the asm) and clears each pointer.
CVideoFrameAnalyst::~CVideoFrameAnalyst()
{
    if (muField00)
    {
        XMemFree(lToPtr(muField00), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField00 = 0;
    }
    if (muField04)
    {
        XMemFree(lToPtr(muField04), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField04 = 0;
    }
    if (muField0C)
    {
        XMemFree(lToPtr(muField0C), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField0C = 0;
    }
    if (muField08)
    {
        XMemFree(lToPtr(muField08), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField08 = 0;
    }
}

// CVideoFrameAnalyst::`vector deleting destructor' @0x82A490C8.
// MSVC array/scalar delete helper. Bit1 (0x2) selects the array form: the element count sits
// in the array cookie one word before `this`; each element is destroyed (the asm inlines the
// destructor body -- re-rolled here to the real ~CVideoFrameAnalyst call) from last to first,
// then the cookie block is freed when bit0 (0x1) is set. The scalar form destroys `this` and
// frees it when bit0 is set.
void* CVideoFrameAnalyst::_vector_deleting_destructor_(u32 uFlags)
{
    if ((uFlags & 2) != 0)
    {
        u32* lpCookie = reinterpret_cast<u32*>(this) - 1;    // MSVC array element-count cookie
        s32  liCount  = static_cast<s32>(*lpCookie);

        CVideoFrameAnalyst* lpElement = this + liCount;       // one past the last element
        for (s32 li = liCount - 1; li >= 0; --li)
        {
            --lpElement;
            lpElement->~CVideoFrameAnalyst();
        }

        if ((uFlags & 1) != 0)
        {
            XMemFree(lpCookie, KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        }
        return lpCookie;
    }

    this->~CVideoFrameAnalyst();
    if ((uFlags & 1) != 0)
    {
        XMemFree(this, KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
    }
    return this;
}

// ARTIST 0x82A47450: per-macroblock Y, U and V means.
int CVideoFrameAnalyst::computeBlockAvg(u8* pY, u8* pU, u8* pV)
{
    const int rows = static_cast<int>((muField4024 + 15u) >> 4);
    const int cols = static_cast<int>((muField4020 + 15u) >> 4);
    const u32 chromaStride = (muField4020 + 1u) >> 1;
    if (muField08)
    {
        XMemFree(reinterpret_cast<void*>(muField08), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES);
        muField08 = 0;
    }
    const s32 area = static_cast<s32>(muArea);
    u8* result = static_cast<u8*>(XMemAlloc(3 * (area >> 2), KU_VIDEOFRAMEANALYST_XMEM_ATTRIBUTES));
    muField08 = reinterpret_cast<u32>(result);
    u8* yMean = result;
    u8* uMean = result + (area >> 2);
    u8* vMean = result + (area >> 1);
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const int width = col == cols - 1 ? static_cast<int>(muField4020) - 16 * cols + 16 : 16;
            const int height = row == rows - 1 ? static_cast<int>(muField4024) - 16 * rows + 16 : 16;
            if (width == 16 && height == 16)
            {
                gpWMVFARoutines->calcMean16x16B(yMean, reinterpret_cast<int>(pY), 0, muField4020);
                gpWMVFARoutines->calcMean8x8B(uMean, reinterpret_cast<int>(pU), 0, chromaStride);
                result = gpWMVFARoutines->calcMean8x8B(vMean, reinterpret_cast<int>(pV), 0, chromaStride);
            }
            else { *yMean = *pY; *uMean = *pU; *vMean = *pV; }
            pY += width; pU += width >> 1; pV += width >> 1;
            ++yMean; ++uMean; ++vMean;
        }
        pU += 7 * chromaStride;
        pV += 7 * chromaStride;
        pY += 15 * muField4020;
    }
    return reinterpret_cast<int>(result);
}

// The scalar border and tail paths use the neighbors that stay inside the plane.
static u32 lPixelGradient(const u8* pixel, s32 stride, s32 x, s32 y, s32 height)
{
    u32 sum = 0;
    if (x > 0)
        sum += std::abs(pixel[0] - pixel[-1]);
    if (x < stride - 1)
        sum += std::abs(pixel[0] - pixel[1]);
    if (y > 0)
        sum += std::abs(pixel[0] - pixel[-stride]);
    if (y < height - 1)
        sum += std::abs(pixel[0] - pixel[stride]);
    return sum;
}

// ARTIST 0x82A47640
void CVideoFrameAnalyst::computeFrameTextureMap(u8* pSource)
{
    const s32 width = static_cast<s32>((muField4020 + 1) >> 1);
    const s32 height = static_cast<s32>((muField4024 + 1) >> 1);
    const u32 planeSize = width * height;
    u8* planes = static_cast<u8*>(std::malloc(3 * planeSize));
    if (!planes)
        return;

    // ARTIST advances past the final pair only for an even source width.
    u8* luma = planes;
    for (s32 row = 0; row < height; ++row)
    {
        for (s32 col = 0; col < width - 1; ++col)
        {
            const u8* nextRow = pSource + muField4020;
            *luma++ = static_cast<u8>((pSource[0] + pSource[1] + nextRow[0] + nextRow[1]) >> 2);
            pSource += 2;
        }
        const u8* nextRow = pSource + muField4020;
        *luma = static_cast<u8>((pSource[0] + pSource[1] + nextRow[0] + nextRow[1]) >> 2);
        if (2 * static_cast<u32>(width) == muField4020)
        {
            ++luma;
            pSource += 2;
        }
        pSource += muField4020;
    }
    std::memcpy(luma, pSource, planeSize);
    std::memcpy(luma + planeSize, pSource + planeSize, planeSize);

    u32* activity = static_cast<u32*>(std::malloc(planeSize * sizeof(u32)));
    if (!activity)
    {
        std::free(planes);
        return;
    }

    const s32 gradientCols = width >> 3;
    const s32 gradientCount = gradientCols * (height >> 3);
    u32* gradientDifference = static_cast<u32*>(lToPtr(muField0C));
    std::memset(gradientDifference, 0, gradientCount * sizeof(u32));

    const u8* cb = planes + planeSize;
    const u8* cr = cb + planeSize;
    const s32 gradientRun = (width - 2) & ~7;
    // ARTIST's virtual kernel overwrites word 0 and leaves words 1–7 as
    // prior stack contents. Only gradientDifference depends on those words.
    u32 lumaScratch[8];
    const volatile u32* lumaTerms = lumaScratch;
    for (s32 y = 0; y < height; ++y)
    {
        const s32 rowStart = y * width;
        const s32 gradientRow = (y >> 3) * gradientCols;
        s32 x = 0;
        while (x < width)
        {
            if (y > 0 && y < height - 1 && x == 1)
            {
                for (s32 run = 0; run < gradientRun; run += 8)
                {
                    const s32 pixel = rowStart + x;
                    gpWMVFARoutines->calcGradient8x1B(
                        lumaScratch, activity + pixel,
                        reinterpret_cast<int>(planes + pixel),
                        reinterpret_cast<int>(cb + pixel),
                        reinterpret_cast<int>(cr + pixel), 0, width);
                    u32& difference = gradientDifference[gradientRow + (run >> 3)];
                    for (s32 lane = 0; lane < 8; ++lane)
                        difference -= 2 * lumaTerms[lane];
                    for (s32 lane = 0; lane < 8; ++lane)
                        difference += activity[pixel + lane];
                    x += 8;
                }
            }

            const s32 pixel = rowStart + x;
            const u32 yGradient = lPixelGradient(planes + pixel, width, x, y, height);
            const u32 chromaGradient = lPixelGradient(cb + pixel, width, x, y, height)
                                     + lPixelGradient(cr + pixel, width, x, y, height);
            activity[pixel] = yGradient + chromaGradient;
            // The bottom-left sample uses the byte count passed to memset as its offset.
            const s32 gradientIndex = y == height - 1 && x == 0
                ? gradientCount : gradientRow + (x >> 3);
            gradientDifference[gradientIndex] -= yGradient;
            gradientDifference[gradientIndex] += chromaGradient;
            ++x;
        }
    }

    const s32 blockCols = (width + 3) >> 2;
    const s32 blockRows = (height + 3) >> 2;
    const s32 groupedCols = (blockCols - 1) & ~3;
    u32* blockActivity = static_cast<u32*>(lToPtr(muField04));
    u32* mean = blockActivity;
    for (s32 y = 0; y < blockRows; ++y)
    {
        s32 x = 0;
        const s32 rowStart = 4 * y * width;
        if (y < blockRows - 1)
        {
            for (; x < groupedCols; x += 4, mean += 4)
            {
                gpWMVFARoutines->calcMean4x4x4(
                    mean, reinterpret_cast<int>(activity), rowStart + 4 * x, width);
            }
        }
        for (; x < blockCols; ++x, ++mean)
        {
            const s32 pixel = rowStart + 4 * x;
            if (4 * y + 3 < height && 4 * x + 3 < width)
                gpWMVFARoutines->calcMean4x4(mean, reinterpret_cast<int>(activity), pixel, width);
            else
                *mean = activity[pixel];
        }
    }

    std::memset(maActivityHistogram, 0, sizeof(maActivityHistogram));
    const s32 blocks = blockCols * blockRows;
    for (s32 block = 0; block < blocks; ++block)
        ++maActivityHistogram[blockActivity[block]];

    u8* textureMap = static_cast<u8*>(lToPtr(muField00));
    for (s32 block = 0; block < blocks; ++block)
    {
        const u32 value = blockActivity[block];
        textureMap[block] = value <= 60 ? static_cast<u8>(value < 14) : 2;
    }

    std::free(activity);
    std::free(planes);
}
