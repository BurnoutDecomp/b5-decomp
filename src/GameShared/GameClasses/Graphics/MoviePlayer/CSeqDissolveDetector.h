#pragma once

#include <cstddef>  // offsetof
#include "types.hpp"

class CSeqDissolveDetector
{
public:
    enum { KI_FEATURE_HISTORY = 21 };

    CSeqDissolveDetector* Reset();
    CSeqDissolveDetector* Detect(f32 lfPrevVarDelta, f32 lfCurVarDelta,
                                 f32 lfPrevMeanDelta, f32 lfCurMeanDelta);
    CSeqDissolveDetector* ComputeLocalFeature();

    s32 miDissolveDetected;
    s32 miDissolveLength;
    s32 miMaxDissolveLength;
    f32 mfMaxRecoveryDelta;
    f32 mfMinVarianceDelta;
    s32 miNonIncreasingCount;
    s32 miConsecutiveNonIncreasingCount;
    s32 miMeanSignChanges;
    s32 miFallingSamples;
    s32 miRisingSamples;

    f32 mafFeatureHistory[KI_FEATURE_HISTORY];
    s32 miFeatureSampleCount;
    f32 mfLocalFeature;
    s32 miFeatureIndex;

private:
    friend void _CSeqDissolveDetector_AssertLayout();
};

inline void _CSeqDissolveDetector_AssertLayout()
{
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miDissolveDetected) == 0x00), "CSeqDissolveDetector::miDissolveDetected");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miDissolveLength) == 0x04), "CSeqDissolveDetector::miDissolveLength");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miMaxDissolveLength) == 0x08), "CSeqDissolveDetector::miMaxDissolveLength");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, mfMaxRecoveryDelta) == 0x0C), "CSeqDissolveDetector::mfMaxRecoveryDelta");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, mfMinVarianceDelta) == 0x10), "CSeqDissolveDetector::mfMinVarianceDelta");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miNonIncreasingCount) == 0x14), "CSeqDissolveDetector::miNonIncreasingCount");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miConsecutiveNonIncreasingCount) == 0x18), "CSeqDissolveDetector::miConsecutiveNonIncreasingCount");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miMeanSignChanges) == 0x1C), "CSeqDissolveDetector::miMeanSignChanges");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miFallingSamples) == 0x20), "CSeqDissolveDetector::miFallingSamples");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miRisingSamples) == 0x24), "CSeqDissolveDetector::miRisingSamples");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, mafFeatureHistory) == 0x28), "dissolve feature history");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miFeatureSampleCount) == 0x7C), "dissolve sample count");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, mfLocalFeature) == 0x80), "dissolve local feature");
    CGS_STATIC_ASSERT((offsetof(CSeqDissolveDetector, miFeatureIndex) == 0x84), "dissolve feature index");
    CGS_STATIC_ASSERT((sizeof(CSeqDissolveDetector) == 0x88), "CSeqDissolveDetector size");
}
