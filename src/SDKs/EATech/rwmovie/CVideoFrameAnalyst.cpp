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
//   CVideoFrameAnalyst::computeBlockAvg                @0x82A47450  (NOT bodied -- blocked)
//   CVideoFrameAnalyst::computeFrameTextureMap         @0x82A47640  (NOT bodied -- blocked)
// The two compute* routines dispatch every block kernel through the global analysis-routine
// object `off_8322634C`, calling its VIRTUAL vtable slots 0/1/2/3 (`(*(*g + 4*N))(g, ...)`)
// as well as the non-virtual CWMVFARoutines::calcMean4x4. The committed CWMVFARoutines home
// (CWMVFARoutines.cpp) models that class as NON-virtual with no vtable and no such global,
// and two of its kernels are explicitly unverified -- so the slot-to-method mapping cannot
// be grounded here without guessing a vtable layout. Left out pending a real CWMVFARoutines
// vtable + `off_8322634C` recovery.
//
// The four leading words are heap-buffer pointers stored X360-width (32-bit), matching the
// asm `stw`/`lwz`; the record keeps X360 widths so its array stride stays byte-exact (see
// KU_VIDEOFRAMEANALYST_STRIDE and CVideoFrameAnalyst::_AssertLayout).
// =====================================================================================
#include <cstdint>

#include "types.hpp"

#include "CVideoFrameAnalyst.h"

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
// The asm calls XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD) directly (r3 =
// size/pAddress, r4 = attributes), with no `this` argument -- matching the sibling
// CVideoAnalyst / CIntImage / CQueue rwmovie TUs.
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

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

    // A single weighted bin term: (weight * count) truncated to 32 bits, then zero-extended.
    auto lWeightedBin = [](u32 luWeight, s32 liCount) -> u64
    {
        return static_cast<u64>(static_cast<u32>(luWeight * static_cast<u32>(liCount)));
    };

    // Seed the running total and the window with bins 1..9 (weights 1..9).
    u64 luTotal = 0;
    for (u32 luWeight = 1; luWeight <= 9; ++luWeight)
    {
        luTotal += lWeightedBin(luWeight, lpHistogram[luWeight]);
    }

    u64 luWindow   = luTotal;
    u64 luBest     = luTotal;
    s32 liBestPos  = 10;

    for (u32 luPos = 10; luPos < 3072; ++luPos)
    {
        u64 luAdd = lWeightedBin(luPos, lpHistogram[luPos]);
        luTotal  += luAdd;
        luWindow += luAdd - lWeightedBin(luPos - 10, lpHistogram[luPos - 10]);
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
