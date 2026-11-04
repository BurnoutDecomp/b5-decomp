#include "GameShared/GameClasses/Graphics/MoviePlayer/CSeqDissolveDetector.h"

#include <cmath>

namespace
{
    const double KD_RECOVERY_THRESHOLD = 22.8;
    const double KD_FALL_THRESHOLD = -22.8;
    const f32 KF_VARIANCE_STEP = 2.0f;
    const f32 KF_REJECT_RATE = 150.0f;
}

// ARTIST 0x82A04AF8
CSeqDissolveDetector* CSeqDissolveDetector::ComputeLocalFeature()
{
    mfLocalFeature = mafFeatureHistory[(miFeatureIndex + 1) % KI_FEATURE_HISTORY];
    for (s32 liOffset = 2; liOffset <= 15; ++liOffset)
    {
        const f32 lfFeature = mafFeatureHistory[(miFeatureIndex + liOffset) % KI_FEATURE_HISTORY];
        if (mfLocalFeature < lfFeature)
            mfLocalFeature = lfFeature;
    }
    return this;
}

// ARTIST 0x82A05278
CSeqDissolveDetector* CSeqDissolveDetector::Reset()
{
    if (miDissolveLength > 0)
        ComputeLocalFeature();

    mfMaxRecoveryDelta = 0.0f;
    miDissolveDetected = 0;
    mfMinVarianceDelta = 0.0f;
    miDissolveLength = 0;
    miNonIncreasingCount = 0;
    miConsecutiveNonIncreasingCount = 0;
    miMeanSignChanges = 0;
    miFallingSamples = 0;
    miRisingSamples = 0;
    return this;
}

// ARTIST 0x82A052D8
CSeqDissolveDetector* CSeqDissolveDetector::Detect(f32 lfPrevVarDelta, f32 lfCurVarDelta,
                                                  f32 lfPrevMeanDelta, f32 lfCurMeanDelta)
{
    if (miMaxDissolveLength <= 0)
        return this;

    const s32 liSample = miFeatureSampleCount;
    miFeatureSampleCount = static_cast<s32>(static_cast<u32>(liSample) + 1u);
    miFeatureIndex = liSample % KI_FEATURE_HISTORY;
    mafFeatureHistory[miFeatureIndex] = std::fabs(lfCurVarDelta);
    if (miDissolveLength == 0)
        ComputeLocalFeature();

    if (miDissolveLength == 0)
    {
        if (lfCurVarDelta < KD_FALL_THRESHOLD)
        {
            mfMinVarianceDelta = lfCurVarDelta;
            miDissolveLength = 1;
            miFallingSamples = 1;
        }
        return this;
    }

    ++miDissolveLength;
    if (lfCurVarDelta < mfMinVarianceDelta)
    {
        if (miDissolveLength == 2)
        {
            if (lfPrevVarDelta - lfCurVarDelta > KF_VARIANCE_STEP)
                ++miFallingSamples;
        }
        else
        {
            miFallingSamples = 1;
        }

        mfMinVarianceDelta = lfCurVarDelta;
        miDissolveLength = 1;
        miNonIncreasingCount = 0;
        miConsecutiveNonIncreasingCount = 0;
        miMeanSignChanges = 0;
        miRisingSamples = 0;
        mfMaxRecoveryDelta = 0.0f;
    }
    else
    {
        if ((lfPrevMeanDelta > 0.0f && lfCurMeanDelta < 0.0f) ||
            (lfPrevMeanDelta < 0.0f && lfCurMeanDelta > 0.0f))
            ++miMeanSignChanges;
        if (miMeanSignChanges > 0)
            return Reset();

        if (!(lfCurVarDelta > lfPrevVarDelta))
        {
            ++miNonIncreasingCount;
            ++miConsecutiveNonIncreasingCount;
        }
        else
        {
            if (miNonIncreasingCount > 3)
                return Reset();
            if (mfMaxRecoveryDelta < lfCurVarDelta)
                mfMaxRecoveryDelta = lfCurVarDelta;
            miConsecutiveNonIncreasingCount = 0;
            if (lfCurVarDelta - lfPrevVarDelta > KF_VARIANCE_STEP)
                ++miRisingSamples;
        }
    }

    const f32 lfFall = -mfMinVarianceDelta;
    f32 lfFeature = mfMaxRecoveryDelta;
    const bool lbRejectFall = lfFeature < lfFall && miFallingSamples < 4 &&
        -(mfMinVarianceDelta / static_cast<f32>(miFallingSamples)) > KF_REJECT_RATE;
    const bool lbRejectRise = miRisingSamples < 4 &&
        (lfFeature - mfMinVarianceDelta) / static_cast<f32>(miRisingSamples) > KF_REJECT_RATE;

    if (miDissolveLength >= miMaxDissolveLength)
    {
        if (lfFeature < KD_RECOVERY_THRESHOLD)
            return Reset();
    }
    else
    {
        if (miConsecutiveNonIncreasingCount < 2)
            return this;
        if (miDissolveLength < 6 || !(lfFeature > KD_RECOVERY_THRESHOLD))
            return Reset();
    }

    if (lfFall < lfFeature)
        lfFeature = lfFall;
    if (lfFeature < mfLocalFeature || lbRejectFall || lbRejectRise)
        return Reset();

    miDissolveDetected = 1;
    return this;
}
