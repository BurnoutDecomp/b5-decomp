// ============================================================================
// CHistogram.cpp  --  video/scene histogram + distance metrics
//   CHistogram::CalcHistogram      @ 0x82A04330
//   CHistogram::CalcHistDist       @ 0x82A048B0
//   CHistogram::isHistogramChanged @ 0x82A04A60
//   Called by CArrayLookahead::DetectEventInArray (scene-cut / event detection).
//   Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative).
// ============================================================================

#include "GameShared/GameClasses/Graphics/MoviePlayer/CHistogram.h"

void CHistogram::CalcHistogram(const u8* lpauImage, u32 luWidth, u32 luHeight)
{
    if (lpauImage == nullptr)
    {
        return;
    }

    const u32 luNumPixels = luWidth * luHeight;
    const u32 luQuarter = luNumPixels >> 2;

    for (u32 luBin = 0; luBin < KU_NUM_BINS; ++luBin)
    {
        maHistogram[luBin] = 0;
        maHistogramSecondary[luBin] = 0;
    }

    // Primary histogram: every other pixel on every other row of the first plane.
    const u8* lpauRow = lpauImage;
    for (u32 luRow = 0; luRow < luHeight; luRow += 2)
    {
        for (u32 luCol = 0; luCol < luWidth; luCol += 2)
        {
            ++maHistogram[lpauRow[luCol]];
        }
        lpauRow += luWidth * 2;
    }

    // Secondary histogram: quarter-sized plane immediately following the first.
    const u8* lpauSecond = lpauImage + luNumPixels;
    for (u32 luIndex = 0; luIndex < luQuarter; ++luIndex)
    {
        ++maHistogramSecondary[lpauSecond[luIndex]];
    }

    // First and second moments of the primary histogram over all 256 bins.
    f32 lfFirstMoment = 0.0f;
    f32 lfSecondMoment = 0.0f;
    f32 lfHighMoment = 0.0f;
    for (u32 luBin = 0; luBin < KU_NUM_BINS; ++luBin)
    {
        const f32 lfCount = static_cast<f32>(maHistogram[luBin]);
        const f32 lfBin = static_cast<f32>(luBin);
        lfFirstMoment += lfBin * lfCount;
        lfSecondMoment += (lfBin * lfBin) * lfCount;
        if (luBin >= 100)
        {
            lfHighMoment += lfBin * lfCount;
        }
    }
    mfHighMoment = lfHighMoment;

    miValid = 1;

    const f32 lfInvCount = KF_ONE / static_cast<f32>(static_cast<s32>(luQuarter));
    const f32 lfMean = lfInvCount * lfFirstMoment;
    mfMean = lfMean;
    mfVariance = lfInvCount * lfSecondMoment - lfMean * lfMean;
}

f32 CHistogram::CalcHistDist(const s32* lpaOther, const s32* lpaThis, u32 luCount, s32 liMode)
{
    // lpaOther/lpaThis point at two 256-entry histograms; a1 (this) is unused.
    s32 liAccum = 0;

    if (liMode == 1)
    {
        // Sum of absolute per-bin differences over 256 bins.
        for (u32 luBin = 0; luBin < 256; ++luBin)
        {
            liAccum += Abs(lpaOther[luBin] - lpaThis[luBin]);
        }
        return static_cast<f32>(liAccum) / static_cast<f32>(luCount);
    }

    if (liMode == 2)
    {
        // Histogram intersection: sum of per-bin minima over 256 bins.
        for (u32 luBin = 0; luBin < 256; ++luBin)
        {
            const s32 liOther = lpaOther[luBin];
            const s32 liThis = lpaThis[luBin];
            liAccum += (liOther < liThis) ? liOther : liThis;
        }
        // (count - intersection) / count, scaled by KF_HIST_INTERSECTION_SCALE.
        return (KF_HIST_INTERSECTION_SCALE * static_cast<f32>(static_cast<s32>(luCount) - liAccum))
             / static_cast<f32>(luCount);
    }

    return KF_HIST_DIST_DEFAULT;
}

bool CHistogram::isHistogramChanged(const CHistogram* lpOther, u32 luImageSize, f32* lpfSecondaryDist, f32* lpfPrimaryDist)
{
    const u32 luCount = luImageSize >> 2;

    const f32 lfSecondary = CalcHistDist(lpOther->maHistogramSecondary, maHistogramSecondary, luCount, 1);
    *lpfSecondaryDist = lfSecondary;

    const f32 lfPrimary = CalcHistDist(lpOther->maHistogram, maHistogram, luCount, 1);
    *lpfPrimaryDist = lfPrimary;

    return lfSecondary > KF_HIST_CHANGED_HIGH
        || lfPrimary > KF_HIST_CHANGED_HIGH
        || (lfSecondary > KF_HIST_CHANGED_LOW && lfPrimary > KF_HIST_CHANGED_LOW);
}
