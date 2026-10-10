// =====================================================================================
// CWMVideoPerceptionModel -- perceptual analysis model for the WMV video-object encoder.
// See CWMVideoPerceptionModel.h for the layout map.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no debug-info declaration exists for this TU.
//
// The frame analysts hold their analysis buffers as 32-bit pointer values (see
// CVideoFrameAnalyst.h): muField00 is the per-8x8 texture map (bytes), muField04 the
// per-8x8 activity map (u32), muField08 the per-MB block averages (bytes). Each MB owns a
// 2x2 group of 8x8 samples: row stride 2 * miWidthInBlocks, two samples per MB column.
// =====================================================================================
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "types.hpp"

#include "CWMVideoPerceptionModel.h"
#include "CVideoAnalyst.h"
#include "CVideoFrameAnalyst.h"
#include "CWMVideoObjectEncoder.h"
#include "CWMVFARoutines.h"

// ---- raw Xbox 360 XDK heap API (platform externs, as the sibling rwmovie TUs) --------
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// XMemAlloc/XMemFree attribute word for this codec's analysis pools (lis 0x248C / ori 0x8000).
static const u32 KU_PERCEPTIONMODEL_XMEM_ATTRIBUTES = 0x248C8000u;

// ctor defaults.
static const s32 KI_PERCEPTIONMODEL_FLAGS_DEFAULT = 1;

// Texture-activity threshold range (decideApplyDquan clamps the probed band to it) and the
// split between the low and middle delta tables.
static const u32 KU_TEXTURE_THRESHOLD_MIN = 14;
static const u32 KU_TEXTURE_THRESHOLD_MID = 20;
static const u32 KU_TEXTURE_THRESHOLD_MAX = 30;

// An 8x8 activity sample counts as texture when it lies in [MIN, LIMIT).
static const u32 KU_TEXTURE_ACTIVITY_MIN   = 30;
static const u32 KU_TEXTURE_ACTIVITY_LIMIT = 60;
// IsMBDominatedByTexture: texture samples needed for a texture-dominated MB.
static const s32 KI_TEXTURE_SAMPLES_FOR_MB = 3;

// decideApplyIDquanBlocks: an activity sample at or below this is flat.
static const u32 KU_FLAT_ACTIVITY_MAX = 7;
// decideApplyIDquanBlocks results.
static const int KI_IDQUAN_NONE         = 0;
static const int KI_IDQUAN_LOW_ACTIVITY = 4;
static const int KI_IDQUAN_FLAT         = 8;
// Weight of the temporal block-average variation limit (10 per reference pair, halved).
static const u32 KU_IDQUAN_SAD_WEIGHT = 10;

// decideApplyDquan: the picture quantiser range the decision runs for, [MIN, LIMIT).
static const s32 KI_DQUAN_PQUANT_MIN   = 3;
static const s32 KI_DQUAN_PQUANT_LIMIT = 22;
// IDquant map byte: bit0 = apply, bit7 = the MB is flat.
static const u8 KU_IDQUAN_MAP_APPLY = 0x01;
static const u8 KU_IDQUAN_MAP_FLAT  = 0x80;
// Applied-MB share (percent of all MBs) the map must stay within, and the non-flat
// share above which the low-threshold case switches to the middle table.
static const u32 KU_DQUAN_APPLIED_MIN_PERCENT  = 3;
static const u32 KU_DQUAN_APPLIED_MAX_PERCENT  = 90;
static const u32 KU_DQUAN_NONFLAT_MID_PERCENT  = 30;

// ShiftPixels: luma overlap margin, widened when the encoder's +0x7B34 mode is >= 4.
static const s32 KI_SHIFT_PIXELS_DEFAULT = 2;
static const s32 KI_SHIFT_PIXELS_WIDE    = 4;
static const s32 KI_SHIFT_PIXELS_WIDE_MODE = 4;

// Differential-quantiser target per picture quantiser (indexed by miPQuant, 3..21 used).
// Dumped from the image: three consecutive 32-byte rows.
static const u8 KAU_DQUAN_TARGET_LOW[32] = {
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
    4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
};
static const u8 KAU_DQUAN_TARGET_MID[32] = {
    1, 1, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
};
static const u8 KAU_DQUAN_TARGET_HIGH[32] = {
    1, 1, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5,
    5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 10, 11,
};

// The shared analysis-routine instance (declared in CWMVFARoutines.h).
CWMVFARoutines* gpWMVFARoutines = nullptr;

// The frame analysts store their buffers as 32-bit pointer values; convert on the boundary.
static inline const u8* lpAnalystBytes(u32 luBuffer)
{
    return reinterpret_cast<const u8*>(static_cast<uintptr_t>(luBuffer));
}
static inline const u32* lpAnalystWords(u32 luBuffer)
{
    return reinterpret_cast<const u32*>(static_cast<uintptr_t>(luBuffer));
}

// One plane of ShiftPixels: move the image down/right by iShift in place (rows from the
// bottom up), fill each row's iShift left columns with its first moved sample, then copy
// row iShift over the top iShift rows.
static void ShiftPlane(u8* pPlane, u32 uWidth, u32 uHeight, s32 iShift)
{
    u8* lpRow = pPlane + (uHeight - 1) * uWidth;
    for (s32 liCount = static_cast<s32>(uHeight - iShift); liCount > 0; --liCount)
    {
        std::memcpy(lpRow + iShift, lpRow - iShift * uWidth, uWidth - iShift);
        std::memset(lpRow, lpRow[iShift], iShift);
        lpRow -= uWidth;
    }

    const u8* lpSourceRow = pPlane + iShift * uWidth;
    for (s32 liRow = 0; liRow < iShift; ++liRow)
    {
        std::memcpy(pPlane + liRow * uWidth, lpSourceRow, uWidth);
    }
}

CWMVideoPerceptionModel::CWMVideoPerceptionModel(CWMVideoObjectEncoder* pVideoInfo)
{
    // Inherited analyst state (the console inlines the base construction).
    mpFrameAnalysts = nullptr;
    miNumAnalysts   = 0;
    mpTemporal      = nullptr;
    miTemporalCount = 0;
    miMaxRefFrames  = 0;
    miCurIndex      = -1;
    miSetIndex      = -1;
    miFreeCount     = 0;

    mpVideoInfo        = pVideoInfo;
    mpIDquanMap        = nullptr;
    miField4C          = 0;
    miTextureThreshold = static_cast<s32>(KU_TEXTURE_THRESHOLD_MIN);
    miField44          = KI_PERCEPTIONMODEL_FLAGS_DEFAULT;

    if (!gpWMVFARoutines)
    {
        void* lpMemory = XMemAlloc(sizeof(CWMVFARoutines), KU_PERCEPTIONMODEL_XMEM_ATTRIBUTES);
        gpWMVFARoutines = lpMemory ? new (lpMemory) CWMVFARoutines() : nullptr;
    }
}

CWMVideoPerceptionModel::~CWMVideoPerceptionModel()
{
    if (mpIDquanMap)
    {
        XMemFree(mpIDquanMap, KU_PERCEPTIONMODEL_XMEM_ATTRIBUTES);
        mpIDquanMap = nullptr;
    }
    if (gpWMVFARoutines)
    {
        XMemFree(gpWMVFARoutines, KU_PERCEPTIONMODEL_XMEM_ATTRIBUTES);
        gpWMVFARoutines = nullptr;
    }
}

int CWMVideoPerceptionModel::IsTextureBlock(int iMBRow, int iMBCol, int iSubBlock)
{
    CVideoFrameAnalyst* lpAnalyst = getCurFrameAnalyst();
    if (!lpAnalyst)
    {
        return 0;
    }

    const u8* lpTextureMap = lpAnalystBytes(lpAnalyst->muField00);
    const u32 luStride     = mpVideoInfo->muMBWidth;
    const int liSubRow     = iSubBlock >> 1;
    const int liSubCol     = iSubBlock & 1;

    if (!mpVideoInfo->miField7B38 || !mpVideoInfo->miField6D54)
    {
        return lpTextureMap[2 * ((2 * iMBRow + liSubRow) * luStride + iMBCol) + liSubCol];
    }

    // Field pair: the MB row covers two picture rows; take the larger of the two fields.
    int liRow = 2 * iMBRow;
    const int liRowCount = static_cast<int>(static_cast<u32>(miHeight) >> 4);
    if (liRow >= liRowCount)
    {
        liRow = liRowCount - 1;
    }

    const int liFirstRow  = 2 * liRow + liSubRow;
    const int liSecondRow = liFirstRow + 2;
    const u8 luFirst  = lpTextureMap[2 * (luStride * liFirstRow + iMBCol) + liSubCol];
    const u8 luSecond = lpTextureMap[2 * (luStride * liSecondRow + iMBCol) + liSubCol];
    const int liPickedRow = (luFirst > luSecond) ? liFirstRow : liSecondRow;
    return lpTextureMap[2 * (luStride * liPickedRow + iMBCol) + liSubCol];
}

int CWMVideoPerceptionModel::IsMBDominatedByTexture(int iMBRow, int iMBCol, int* piOutLowActivityCount,
                                                    int* piOutTextureCount)
{
    s32 liLowActivity = 0;
    s32 liTexture     = 0;

    CVideoFrameAnalyst* lpAnalyst = getCurFrameAnalyst();
    if (!lpAnalyst)
    {
        return 0;
    }

    int liPasses = 1;
    if (mpVideoInfo->miField7B38 && mpVideoInfo->miField6D54)
    {
        iMBRow *= 2;
        liPasses = 2;
        const int liRowCount = static_cast<int>(static_cast<u32>(miHeight) >> 4);
        if (iMBRow >= liRowCount)
        {
            iMBRow = liRowCount - 1;
        }
    }

    const u32  luStride     = mpVideoInfo->muMBWidth;
    const u32* lpActivity   = lpAnalystWords(lpAnalyst->muField04);
    const u32  luRowOffset  = 2 * static_cast<u32>(miWidthInBlocks);
    const u32  luThreshold  = static_cast<u32>(miTextureThreshold);

    for (int liPass = 0; liPass < liPasses; ++liPass)
    {
        // The console accumulates the pass index into the row.
        iMBRow += liPass;

        const u32 luTop    = 2 * (2 * luStride * iMBRow + iMBCol);
        const u32 luBottom = luTop + luRowOffset;
        const u32 luSamples[4] = { lpActivity[luTop], lpActivity[luTop + 1],
                                   lpActivity[luBottom], lpActivity[luBottom + 1] };

        for (int liSample = 0; liSample < 4; ++liSample)
        {
            if (luSamples[liSample] >= KU_TEXTURE_ACTIVITY_MIN && luSamples[liSample] < KU_TEXTURE_ACTIVITY_LIMIT)
            {
                ++liTexture;
            }
        }

        if (piOutLowActivityCount)
        {
            for (int liSample = 0; liSample < 4; ++liSample)
            {
                liLowActivity += (luThreshold >= luSamples[liSample]) ? 1 : 0;
            }
        }
    }

    if (piOutLowActivityCount)
    {
        if (liPasses == 1)
        {
            *piOutLowActivityCount = liLowActivity;
        }
        else
        {
            liTexture >>= 1;
            *piOutLowActivityCount = liLowActivity >> 1;
        }
    }
    if (piOutTextureCount)
    {
        *piOutTextureCount = liTexture;
    }
    return (liTexture >= KI_TEXTURE_SAMPLES_FOR_MB) ? 1 : 0;
}

void CWMVideoPerceptionModel::ShiftPixels(u8* pY, u8* pU, u8* pV)
{
    if (!pY || !pU || !pV)
    {
        return;
    }

    const u32 luWidth  = static_cast<u32>(miWidth);
    const u32 luHeight = static_cast<u32>(miHeight);

    s32 liShift = KI_SHIFT_PIXELS_DEFAULT;
    if (mpVideoInfo && mpVideoInfo->miField7B34 >= KI_SHIFT_PIXELS_WIDE_MODE)
    {
        liShift = KI_SHIFT_PIXELS_WIDE;
    }

    ShiftPlane(pY, luWidth, luHeight, liShift);
    ShiftPlane(pU, luWidth >> 1, luHeight >> 1, liShift >> 1);
    ShiftPlane(pV, luWidth >> 1, luHeight >> 1, liShift >> 1);
}

// The console build emits a single tail-call that passes `this` and all four plane arguments
// straight through to the inherited analyser.
CVideoFrameAnalyst* CWMVideoPerceptionModel::analysisFrame(u8* pY, u8* pU, u8* pV, u8* pSourceId)
{
    return CVideoAnalyst::analysisFrame(pY, pU, pV, pSourceId);
}

int CWMVideoPerceptionModel::decideApplyIDquanBlocks(int iMBRow, int iMBCol, int bCountFlat)
{
    u32 luLowActivity = 0;
    u32 luFlat        = 0;

    // The reference frames: the temporal list when it is populated (it holds analyst
    // pointers as 32-bit values, from index 0); otherwise miMaxRefFrames entries of the
    // pointer list rooted at &mpFrameAnalysts, starting at the current-frame index
    // (computed as getCurFrameAnalyst does, without its final range check). The console
    // reads that second list exactly so; only its entry 0 is an analyst pointer.
    const bool lbTemporal = (miTemporalCount != 0);
    u32 luNumRef;
    s32 liStart;
    if (lbTemporal)
    {
        luNumRef = miTemporalCount;
        liStart  = 0;
    }
    else
    {
        luNumRef = static_cast<u32>(miMaxRefFrames);
        if (miCurIndex < 0)
        {
            liStart = -1;
        }
        else
        {
            liStart = miSetIndex;
            if (liStart < 0)
            {
                liStart = miCurIndex - miMaxRefFrames + 1;
                if (liStart < 0)
                {
                    liStart += miNumAnalysts;
                }
            }
        }
    }
    auto lReference = [this, lbTemporal](s32 iIndex) -> CVideoFrameAnalyst*
    {
        if (lbTemporal)
        {
            return reinterpret_cast<CVideoFrameAnalyst*>(
                static_cast<uintptr_t>(static_cast<u32>(mpTemporal[iIndex])));
        }
        return (&mpFrameAnalysts)[iIndex];
    };

    const u32 luSadLimit = (KU_IDQUAN_SAD_WEIGHT * luNumRef * (luNumRef - 1)) / 2;

    int liPasses = 1;
    if (mpVideoInfo->miField7B38 && mpVideoInfo->miField6D54)
    {
        iMBRow *= 2;
        liPasses = 2;
        const int liRowCount = static_cast<int>(static_cast<u32>(miHeight) >> 4);
        if (iMBRow >= liRowCount)
        {
            iMBRow = liRowCount - 1;
        }
    }

    const u32 luStride   = mpVideoInfo->muMBWidth;
    const u32 luAvgIndex = luStride * iMBRow + iMBCol;

    for (u32 luRef = 0; luRef < luNumRef; ++luRef)
    {
        s32 liIndex = liStart + static_cast<s32>(luRef);
        if (static_cast<u32>(liIndex) >= static_cast<u32>(miNumAnalysts))
        {
            liIndex -= miNumAnalysts;
        }

        const u32* lpActivity  = lpAnalystWords(lReference(liIndex)->muField04);
        const u32  luRowOffset = 2 * static_cast<u32>(miWidthInBlocks);
        const u32  luThreshold = static_cast<u32>(miTextureThreshold);

        for (int liPass = 0; liPass < liPasses; ++liPass)
        {
            // The console accumulates the pass index into the row (across references too).
            iMBRow += liPass;

            const u32 luTop    = 2 * (2 * luStride * iMBRow + iMBCol);
            const u32 luBottom = luTop + luRowOffset;
            const u32 luSamples[4] = { lpActivity[luTop], lpActivity[luTop + 1],
                                       lpActivity[luBottom], lpActivity[luBottom + 1] };

            for (int liSample = 0; liSample < 4; ++liSample)
            {
                luLowActivity += (luThreshold >= luSamples[liSample]) ? 1 : 0;
            }
            if (bCountFlat)
            {
                for (int liSample = 0; liSample < 4; ++liSample)
                {
                    luFlat += (luSamples[liSample] <= KU_FLAT_ACTIVITY_MAX) ? 1 : 0;
                }
            }
        }
    }

    if (liPasses == 2)
    {
        luLowActivity >>= 1;
        luFlat >>= 1;
    }

    const int liDecision = (luFlat >= 4 * luNumRef) ? KI_IDQUAN_FLAT : KI_IDQUAN_LOW_ACTIVITY;
    if (luLowActivity < 4 * luNumRef)
    {
        return KI_IDQUAN_NONE;
    }
    if (luNumRef <= 1)
    {
        return liDecision;
    }

    // The block average must be temporally stable: weighted variation between successive
    // references (older pairs weigh less).
    u32 luSad   = 0;
    s32 liIndex = liStart;
    for (u32 luPair = 0; luPair < luNumRef - 1; ++luPair)
    {
        s32 liNext = liIndex + 1;
        if (liNext >= miNumAnalysts)
        {
            liNext -= miNumAnalysts;
        }

        const u8* lpAverages     = lpAnalystBytes(lReference(liIndex)->muField08);
        const u8* lpNextAverages = lpAnalystBytes(lReference(liNext)->muField08);
        const int liDelta = static_cast<int>(lpNextAverages[luAvgIndex]) - static_cast<int>(lpAverages[luAvgIndex]);
        luSad += static_cast<u32>(std::abs(liDelta)) * (luNumRef - luPair - 1);
        liIndex = liNext;
    }

    return (luSad < luSadLimit) ? liDecision : KI_IDQUAN_NONE;
}

int CWMVideoPerceptionModel::decideApplyDquan(int* piOutDeltaQP)
{
    u32 luApplied = 0;
    u32 luFlat    = 0;

    CWMVideoObjectEncoder* lpVideoInfo = mpVideoInfo;
    if (lpVideoInfo->miPQuant < KI_DQUAN_PQUANT_MIN || lpVideoInfo->miPQuant >= KI_DQUAN_PQUANT_LIMIT)
    {
        return 0;
    }

    if (!mpIDquanMap)
    {
        miIDquanStride = (lpVideoInfo->muMBWidth + 7) >> 3;
        u32 luRows = lpVideoInfo->muMBHeight;
        if (lpVideoInfo->miField7B38)
        {
            luRows <<= 1;
        }
        mpIDquanMap = static_cast<u8*>(XMemAlloc(miIDquanStride * luRows, KU_PERCEPTIONMODEL_XMEM_ATTRIBUTES));
        if (!mpIDquanMap)
        {
            return 0;
        }
    }
    std::memset(mpIDquanMap, 0, mpVideoInfo->muMBHeight * miIDquanStride);

    // Texture threshold: the default, or half the frame's high-texture band clamped.
    miTextureThreshold = static_cast<s32>(KU_TEXTURE_THRESHOLD_MIN);
    if (miField44 & 1)
    {
        CVideoFrameAnalyst* lpAnalyst = getCurFrameAnalyst();
        s32 liBand;
        if (lpAnalyst && lpAnalyst->detectHighTextureBand(&liBand))
        {
            u32 luThreshold = static_cast<u32>(liBand) >> 1;
            if (luThreshold <= KU_TEXTURE_THRESHOLD_MIN)
            {
                luThreshold = KU_TEXTURE_THRESHOLD_MIN;
            }
            else if (luThreshold > KU_TEXTURE_THRESHOLD_MAX)
            {
                luThreshold = KU_TEXTURE_THRESHOLD_MAX;
            }
            miTextureThreshold = static_cast<s32>(luThreshold);
        }
    }

    for (u32 luRow = 0; luRow < mpVideoInfo->muMBHeight; ++luRow)
    {
        for (u32 luCol = 0; luCol < mpVideoInfo->muMBWidth; ++luCol)
        {
            const int liDecision = decideApplyIDquanBlocks(static_cast<int>(luRow), static_cast<int>(luCol), 1);
            if (liDecision != KI_IDQUAN_NONE)
            {
                ++luApplied;
                luFlat += (liDecision == KI_IDQUAN_FLAT) ? 1 : 0;
                u8& luCell = mpIDquanMap[miIDquanStride * luRow + luCol];
                luCell = KU_IDQUAN_MAP_APPLY;
                if (liDecision == KI_IDQUAN_FLAT)
                {
                    luCell |= KU_IDQUAN_MAP_FLAT;
                }
            }
        }
    }

    // Drop isolated decisions (no applied 4-neighbour) away from the picture border.
    for (u32 luRow = 1; luRow < mpVideoInfo->muMBHeight - 1; ++luRow)
    {
        for (u32 luCol = 1; luCol < mpVideoInfo->muMBWidth - 1; ++luCol)
        {
            u8* lpCell = &mpIDquanMap[miIDquanStride * luRow + luCol];
            if (*lpCell && !lpCell[-1] && !lpCell[1]
                && !mpIDquanMap[miIDquanStride * (luRow - 1) + luCol]
                && !mpIDquanMap[miIDquanStride * (luRow + 1) + luCol])
            {
                --luApplied;
                *lpCell = 0;
            }
        }
    }

    const u32 luArea = mpVideoInfo->muMBHeight * mpVideoInfo->muMBWidth;
    if (100 * luApplied < KU_DQUAN_APPLIED_MIN_PERCENT * luArea
        || 100 * luApplied > KU_DQUAN_APPLIED_MAX_PERCENT * luArea)
    {
        return 0;
    }

    const u8* lpTargets;
    const u32 luThreshold = static_cast<u32>(miTextureThreshold);
    if (luThreshold <= KU_TEXTURE_THRESHOLD_MIN)
    {
        lpTargets = (100 * (luApplied - luFlat) > KU_DQUAN_NONFLAT_MID_PERCENT * luArea)
                        ? KAU_DQUAN_TARGET_MID
                        : KAU_DQUAN_TARGET_LOW;
    }
    else if (luThreshold <= KU_TEXTURE_THRESHOLD_MID)
    {
        lpTargets = KAU_DQUAN_TARGET_MID;
    }
    else
    {
        lpTargets = KAU_DQUAN_TARGET_HIGH;
    }

    *piOutDeltaQP = lpTargets[mpVideoInfo->miPQuant];
    *piOutDeltaQP -= mpVideoInfo->miPQuant;
    return 1;
}

// return mpIDquanMap[miIDquanStride * iRow + iCol], read as an unsigned byte.
int CWMVideoPerceptionModel::isApplyIDquanBlocks(int iRow, int iCol)
{
    return mpIDquanMap[miIDquanStride * iRow + iCol];
}
