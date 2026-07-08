// =====================================================================================
// CVideoAnalyst -- frame-analysis manager for the RTCMV/WMV video-object encoder.
// See CVideoAnalyst.h for the layout map and the X360 addresses of every method.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
// =====================================================================================
#include <cstdint>

#include "types.hpp"

#include "CVideoAnalyst.h"
#include "CVideoFrameAnalyst.h"

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
// The asm calls the XDK XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD) directly
// (r3 = size/pAddress, r4 = attributes), with NO `this` argument -- matching the sibling
// CIntImage / CQueue / CLocalHuffman rwmovie TUs.
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// XMemAlloc/XMemFree attribute word for this codec's analysis pools (lis 0x248C / ori 0x8000).
static const u32 KU_VIDEOANALYST_XMEM_ATTRIBUTES = 0x248C8000u;

// Overflow guards from the two allocation-size computations (the asm caps each product
// before adding to XMemAlloc; on overflow it passes 0xFFFFFFFF so the alloc fails).
static const u32 KU_FRAME_COUNT_LIMIT    = 0x3FD41u;     // uNumAnalysts ceiling
static const u32 KU_FRAME_BYTES_LIMIT    = 0xFFFFFFFBu;  // 16428*n ceiling (before +4)
static const u32 KU_TEMPORAL_COUNT_LIMIT = 0x3FFFFFFFu;  // miMaxRefFrames ceiling (*4)

// CVideoAnalyst::Clean @0x82A491D8.
int CVideoAnalyst::Clean()
{
    if (mpFrameAnalysts)
    {
        // The frame array was allocated with an MSVC array cookie; the X360 tears it down
        // through CVideoFrameAnalyst's vector deleting destructor (flags: free | array).
        mpFrameAnalysts->_vector_deleting_destructor_(3);
        mpFrameAnalysts = nullptr;
    }

    // Vestigial return value: the asm returns r3 == the (pre-free) temporal pointer.
    int liResult = static_cast<int>(reinterpret_cast<intptr_t>(mpTemporal));
    if (mpTemporal)
    {
        XMemFree(mpTemporal, KU_VIDEOANALYST_XMEM_ATTRIBUTES);
        mpTemporal = nullptr;
    }

    mpFrameAnalysts = nullptr;
    miNumAnalysts   = 0;
    miTemporalCount = 0;
    miMaxRefFrames  = 0;
    miFreeCount     = 0;
    miCurIndex      = -1;
    miSetIndex      = -1;
    return liResult;
}

// CVideoAnalyst::Init @0x82A49260.
int CVideoAnalyst::Init(s32 iWidth, s32 iHeight, u32 uNumAnalysts, u32 uMaxRef)
{
    Clean();

    if (iWidth > 0)
    {
        miWidth  = iWidth;
        miHeight = iHeight;
        // 4 * blocks-wide * blocks-high, both rounded up to a 16-pixel macroblock grid.
        s32 liArea = 4 * ((iWidth + 15) >> 4) * ((iHeight + 15) >> 4);
        miWidthInBlocks      = iWidth >> 4;
        miMacroBlockArea     = liArea;
        miMacroBlockAreaHalf = liArea >> 1;
    }

    // ---- frame-analyst pool: XMemAlloc(16428*n + 4) with a leading count cookie -------
    u32 luFrameBytes;
    if (uNumAnalysts > KU_FRAME_COUNT_LIMIT ||
        KU_VIDEOFRAMEANALYST_STRIDE * uNumAnalysts > KU_FRAME_BYTES_LIMIT)
    {
        luFrameBytes = 0xFFFFFFFFu;   // force the allocation to fail
    }
    else
    {
        luFrameBytes = KU_VIDEOFRAMEANALYST_STRIDE * uNumAnalysts + 4;
    }

    u32* lpuAlloc = static_cast<u32*>(XMemAlloc(luFrameBytes, KU_VIDEOANALYST_XMEM_ATTRIBUTES));
    CVideoFrameAnalyst* lpFrames = nullptr;
    if (lpuAlloc)
    {
        *lpuAlloc = uNumAnalysts;   // array cookie: element count (read back by delete[])
        lpFrames = reinterpret_cast<CVideoFrameAnalyst*>(lpuAlloc + 1);

        // Per-element construction (the X360 inlined the default constructor here): clear
        // the four leading words and the two trailing words, source id = -1, free flag = 0.
        for (u32 li = 0; li < uNumAnalysts; ++li)
        {
            CVideoFrameAnalyst& lrFrame = lpFrames[li];
            lrFrame.muField00   = 0;
            lrFrame.muField04   = 0;
            lrFrame.muField08   = 0;
            lrFrame.muField0C   = 0;
            lrFrame.muField4020 = 0;
            lrFrame.muField4024 = 0;
            lrFrame.miAvailable = 0;
            lrFrame.muSourceId  = 0xFFFFFFFFu;
        }
    }

    mpFrameAnalysts = lpFrames;
    miNumAnalysts   = static_cast<s32>(uNumAnalysts);

    // ---- temporal index buffer: min(maxRef, numAnalysts) ints (or numAnalysts) --------
    u32 luMaxRef = (uMaxRef == 0 || uMaxRef >= uNumAnalysts) ? uNumAnalysts : uMaxRef;
    miMaxRefFrames = static_cast<s32>(luMaxRef);

    u32 luTemporalBytes = 4 * luMaxRef;
    if (luMaxRef > KU_TEMPORAL_COUNT_LIMIT)
    {
        luTemporalBytes = 0xFFFFFFFFu;
    }
    void* lpTemporal = XMemAlloc(luTemporalBytes, KU_VIDEOANALYST_XMEM_ATTRIBUTES);
    mpTemporal = static_cast<s32*>(lpTemporal);

    int liResult = static_cast<int>(reinterpret_cast<intptr_t>(lpTemporal));
    if (miNumAnalysts)
    {
        for (s32 li = 0; li < miNumAnalysts; ++li)
        {
            liResult = mpFrameAnalysts[li].Init(static_cast<u32>(miWidth), miHeight);
        }
    }
    return liResult;
}

// CVideoAnalyst::analysisFrame @0x82A49450.
CVideoFrameAnalyst* CVideoAnalyst::analysisFrame(u8* pY, u8* pU, u8* pV, u8* pSourceId)
{
    if (!mpFrameAnalysts)
    {
        return nullptr;
    }

    if (miFreeCount <= 0)
    {
        // No analyst flagged free: advance the current one round-robin. The X360 guards
        // the divide with twllei (traps if miNumAnalysts == 0) -- the modulo has the same
        // divide-by-zero, so the semantics match.
        miCurIndex = static_cast<s32>((static_cast<u32>(miCurIndex) + 1) %
                                      static_cast<u32>(miNumAnalysts));
    }
    else
    {
        s32 liIndex = 0;
        if (miNumAnalysts != 0)
        {
            // Find the first analyst flagged free for reuse (miAvailable > 0).
            while (mpFrameAnalysts[liIndex].miAvailable <= 0)
            {
                if (++liIndex >= miNumAnalysts)
                {
                    break;
                }
            }
            if (liIndex < miNumAnalysts)
            {
                miCurIndex = liIndex;
                miFreeCount -= 1;
                mpFrameAnalysts[liIndex].miAvailable = 0;
            }
        }
        if (liIndex == miNumAnalysts)
        {
            return nullptr;   // pool exhausted / nothing free
        }
    }

    CVideoFrameAnalyst* lpFrame = &mpFrameAnalysts[miCurIndex];
    lpFrame->computeFrameTextureMap(pY);

    if (!pSourceId)
    {
        pSourceId = pY;
    }
    lpFrame->muSourceId = static_cast<u32>(reinterpret_cast<uintptr_t>(pSourceId));

    if (miNumAnalysts)
    {
        if (!pU || !pV)
        {
            // Default the chroma planes to the U/V packed after the luma plane.
            u32 luPlane = static_cast<u32>(miHeight) * static_cast<u32>(miWidth);
            pU = &pY[luPlane];
            pV = &pY[luPlane + (luPlane >> 2)];
        }
        lpFrame->computeBlockAvg(pY, pU, pV);
    }
    return lpFrame;
}

// CVideoAnalyst::getCurFrameAnalyst @0x82A47238.
CVideoFrameAnalyst* CVideoAnalyst::getCurFrameAnalyst()
{
    s32 liCur = miCurIndex;
    if (liCur < 0)
    {
        return nullptr;
    }

    s32 liIndex = miSetIndex;
    if (liIndex < 0)
    {
        liIndex = liCur - miMaxRefFrames + 1;
        if (liIndex < 0)
        {
            liIndex += miNumAnalysts;
            if (liIndex < 0)
            {
                return nullptr;
            }
        }
    }
    return &mpFrameAnalysts[liIndex];
}

// CVideoAnalyst::setCurFrameAnalyst @0x82A47290.
CVideoFrameAnalyst* CVideoAnalyst::setCurFrameAnalyst(u8* pSourceId)
{
    if (miNumAnalysts <= 0)
    {
        return nullptr;
    }

    u32 luSourceId = static_cast<u32>(reinterpret_cast<uintptr_t>(pSourceId));
    s32 liIndex = 0;
    while (mpFrameAnalysts[liIndex].muSourceId != luSourceId)
    {
        if (++liIndex >= miNumAnalysts)
        {
            return nullptr;
        }
    }

    miSetIndex = liIndex;
    return &mpFrameAnalysts[liIndex];
}

// CVideoAnalyst::setFreeAnalyst @0x82A493E8.
CVideoAnalyst* CVideoAnalyst::setFreeAnalyst(u8* pSourceId)
{
    if (miNumAnalysts > 0)
    {
        u32 luSourceId = static_cast<u32>(reinterpret_cast<uintptr_t>(pSourceId));
        s32 liIndex = 0;
        while (mpFrameAnalysts[liIndex].muSourceId != luSourceId)
        {
            if (++liIndex >= miNumAnalysts)
            {
                return this;
            }
        }

        CVideoFrameAnalyst* lpFrame = &mpFrameAnalysts[liIndex];
        if (lpFrame)
        {
            lpFrame->miAvailable = 1;
            ++miFreeCount;
        }
    }
    return this;
}

// CVideoAnalyst::setTemporal @0x82A472E0.
CVideoAnalyst* CVideoAnalyst::setTemporal(s32 iIndex, s32 iValue)
{
    if (iIndex == 0)
    {
        miTemporalCount = 0;
    }

    u32 luCount = miTemporalCount;
    if (static_cast<u32>(iIndex + 1) > luCount)
    {
        luCount = static_cast<u32>(iIndex + 1);
    }

    s32* lpTemporal = mpTemporal;
    miTemporalCount = luCount;
    lpTemporal[iIndex] = iValue;
    return this;
}

// CVideoAnalyst::~CVideoAnalyst @0x82A49908.
CVideoAnalyst::~CVideoAnalyst()
{
    // The X360 dtor stores the vtable pointer at +0x00 first; the compiler emits that here.
    if (mpFrameAnalysts)
    {
        mpFrameAnalysts->_vector_deleting_destructor_(3);
        mpFrameAnalysts = nullptr;
    }
    if (mpTemporal)
    {
        XMemFree(mpTemporal, KU_VIDEOANALYST_XMEM_ATTRIBUTES);
        mpTemporal = nullptr;
    }
}
