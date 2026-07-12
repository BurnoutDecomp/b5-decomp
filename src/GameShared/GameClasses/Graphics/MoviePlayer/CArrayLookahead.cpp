// ============================================================================
// CArrayLookahead.cpp  --  circular look-ahead ring of analysed video frames
//   Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative).
//
//   Implemented here (all 9 ledger functions):
//       CArrayLookahead::CArrayLookahead               @ 0x82A04BB8
//       CArrayLookahead::~CArrayLookahead              @ 0x82A055B8
//       CArrayLookahead::Initialize                    @ 0x82A04DE8
//       CArrayLookahead::ResetElement                  @ 0x82A04C68
//       CArrayLookahead::AddArrayElement               @ 0x82A04D30
//       CArrayLookahead::SetDissolveDumpFile           @ 0x82A05258
//       CArrayLookahead::Filter1stOrderDiffLumaVarMean @ 0x82A05048
//       CArrayLookahead::DetectDissolve                @ 0x82A05648
//       CArrayLookahead::DetectEventInArray            @ 0x82A05710
//
//   The last two drive the embedded CSeqDissolveDetector; its
//   Reset/Detect/ComputeLocalFeature engine bodies live in a separate TU and are
//   only DECLARED (in CSeqDissolveDetector.h) -- declarations are all that the
//   per-TU `cl /c` compile gate needs.
// ============================================================================

#include "GameShared/GameClasses/Graphics/MoviePlayer/CArrayLookahead.h"

namespace
{
// Look-ahead IIR filter weights (rodata: flt_820AFCDC / flt_8210EF68 /
// flt_82001DA0). The heavier weight lands on the *neighbour* sample; see the
// exact operand order recovered from the fmadds instructions below.
const f32 KF_WEIGHT_HI = 0.67000002f;  // flt_820AFCDC
const f32 KF_WEIGHT_LO = 0.33000001f;  // flt_8210EF68
const f32 KF_HALF       = 0.5f;        // flt_82001DA0

// Dissolve run cap seeded when detection is enabled (Initialize: li 0x64).
const s32 KI_MAX_DISSOLVE_LENGTH = 100;

// Luma high-moment ratio guard + threshold used by the scene-cut classifier.
// Both are attested by the Hex-Rays pseudocode (which decodes the rodata float
// literals directly): flt_82002138 -> 0.0099999998, dbl_8210EF60 -> 1.2.
const f32 KF_HIGHMOMENT_EPSILON  = 0.0099999998f;  // flt_82002138
const f64 KF_HIGHMOMENT_RATIO    = 1.2;            // dbl_8210EF60 (double compare)

// Per-slot scene-event codes (SElement::miEventType) written by the classifier.
const s32 KI_EVENT_CUT_WEAK      = 2;  // weak cut candidate
const s32 KI_EVENT_CUT           = 3;  // confirmed cut / dissolve boundary
const s32 KI_EVENT_DISSOLVE_RUN  = 4;  // frame spanned by a detected dissolve
} // namespace

// @ 0x82A04BB8
// All fields zero except miDissolveEnabled (=1). The embedded detector's
// all-zero construction is folded inline in the asm; reconstructed as
// value-initialisation of mDissolveDetector.
CArrayLookahead::CArrayLookahead()
    : mpArray(nullptr)
    , muCapacity(0)
    , miImageWidth(0)
    , miImageHeight(0)
    , miImageSize(0)
    , muReadIndex(0)
    , muWriteIndex(0)
    , muFrameCount(0)
    , miDissolveEnabled(1)
    , mDissolveDetector()
    , mpDissolveDumpFile(nullptr)
{
}

// @ 0x82A055B8
// Free every slot's histogram, then the ring array itself.
CArrayLookahead::~CArrayLookahead()
{
    for (u32 luIndex = 0; luIndex < muCapacity; ++luIndex)
    {
        CHistogram* lpHistogram = mpArray[luIndex].mpHistogram;
        if (lpHistogram != nullptr)
        {
            XMemFree(lpHistogram, KU_LOOKAHEAD_XMEM_ATTRIBUTES);
            mpArray[luIndex].mpHistogram = nullptr;
        }
    }

    if (mpArray != nullptr)
    {
        XMemFree(mpArray, KU_LOOKAHEAD_XMEM_ATTRIBUTES);
        mpArray = nullptr;
    }
}

// @ 0x82A04DE8
s32 CArrayLookahead::Initialize(s32 liLookahead, s32 liWidth, s32 liHeight)
{
    // Capacity = requested window + 4 look-ahead slots, floored at 5.
    u32 luCapacity = static_cast<u32>(liLookahead) + 4u;
    u32 luAllocSize;
    if (luCapacity >= 5u)
    {
        // 40 bytes/slot; guard against the u32 size overflowing (0x6666666*40).
        luAllocSize = (luCapacity > 0x6666666u) ? 0xFFFFFFFFu : 40u * luCapacity;
    }
    else
    {
        luCapacity = 5u;
        luAllocSize = 200u;  // 40 * 5
    }

    mpArray = static_cast<SElement*>(XMemAlloc(luAllocSize, KU_LOOKAHEAD_XMEM_ATTRIBUTES));
    if (mpArray == nullptr)
    {
        return -100;
    }

    for (u32 luIndex = 0; luIndex < luCapacity; ++luIndex)
    {
        CHistogram* lpHistogram =
            static_cast<CHistogram*>(XMemAlloc(sizeof(CHistogram), KU_LOOKAHEAD_XMEM_ATTRIBUTES));
        if (lpHistogram == nullptr)
        {
            return -100;
        }
        lpHistogram->mfHighMoment    = 0.0f;
        lpHistogram->miValid         = 0;
        lpHistogram->mfVariance      = 0.0f;
        lpHistogram->muPad10         = 0;
        lpHistogram->mfVarianceDelta = 0.0f;
        lpHistogram->mfMean          = 0.0f;
        mpArray[luIndex].mpHistogram = lpHistogram;
    }

    muCapacity    = luCapacity;
    miImageWidth  = liWidth;
    miImageHeight = liHeight;
    miImageSize   = liWidth * liHeight;
    if (miDissolveEnabled)
    {
        mDissolveDetector.miMaxDissolveLength = KI_MAX_DISSOLVE_LENGTH;
    }
    return 0;
}

// @ 0x82A04C68
// Zero the per-frame analysis fields (and reset the histogram) of one slot.
// The image-data and histogram pointers are preserved.
void CArrayLookahead::ResetElement(u32 luIndex)
{
    if (mpArray == nullptr)
    {
        return;
    }
    if (luIndex >= muCapacity)
    {
        return;
    }

    SElement& lrElement = mpArray[luIndex];

    CHistogram* lpHistogram = lrElement.mpHistogram;
    lpHistogram->mfHighMoment    = 0.0f;
    lpHistogram->miValid         = 0;
    lpHistogram->mfVariance      = 0.0f;
    lpHistogram->muPad10         = 0;
    lpHistogram->mfVarianceDelta = 0.0f;
    lpHistogram->mfMean          = 0.0f;

    lrElement.miHistChanged0 = 0;
    lrElement.miLumaChanged  = 0;
    lrElement.miHistChanged1 = 0;
    lrElement.miReserved14   = 0;
    lrElement.miHistChanged2 = 0;
    lrElement.miReserved1C   = 0;
    lrElement.miEventType    = 0;
    lrElement.miProcessed    = 0;
}

// @ 0x82A04D30
s32 CArrayLookahead::AddArrayElement(const u8* lpImageData)
{
    if (mpArray == nullptr)
    {
        return -100;
    }

    // Ring is "full" when the tail sits three slots ahead of the head (the
    // look-ahead head-room the detector needs).
    if (muReadIndex == (muWriteIndex + 3u) % muCapacity)
    {
        return -100;
    }

    // Clear the slot we are about to overwrite (r4 still holds the write index
    // at the call site -- the pseudocode's 1-arg call is a decompiler artifact).
    ResetElement(muWriteIndex);

    mpArray[muWriteIndex].mpImageData = lpImageData;
    muWriteIndex = (muWriteIndex + 1u) % muCapacity;
    return 0;
}

// @ 0x82A05258
// NOTE (faithful to asm): the handle argument is only *tested*, never stored.
// The dump file is cleared unless it is already open and a non-null handle was
// passed (in which case the existing handle is kept unchanged).
void CArrayLookahead::SetDissolveDumpFile(void* lpFile)
{
    if (lpFile != nullptr && mpDissolveDumpFile != nullptr)
    {
        return;
    }
    mpDissolveDumpFile = nullptr;
}

// @ 0x82A05048
// First-order temporal IIR smoothing across the look-ahead window. Operand
// order (which sample carries the 0.67 weight) is taken from the fmadds
// instructions, NOT the Hex-Rays pseudocode (which transposes the weights).
void CArrayLookahead::Filter1stOrderDiffLumaVarMean()
{
    // "Two-back" wrapped slot index; only read once muFrameCount >= 3, which
    // implies the >= 2 block below has already computed it.
    u32 luTwoBack = 0;

    if (muFrameCount >= 2u)
    {
        u32 luOne = muReadIndex;
        if (luOne == 0u)
        {
            luOne = muCapacity;
        }
        const u32 luPrev = luOne - 1u;
        luTwoBack = (luOne == 1u) ? (muCapacity - 1u) : (luOne - 2u);

        CHistogram* lpPrev = mpArray[luPrev].mpHistogram;
        CHistogram* lpTwoBack = mpArray[luTwoBack].mpHistogram;

        lpTwoBack->mfVariance = lpPrev->mfVariance * KF_WEIGHT_HI + lpTwoBack->mfVariance * KF_WEIGHT_LO;
        lpPrev->mfVariance    = (lpPrev->mfVariance + lpTwoBack->mfVariance) * KF_HALF;

        lpTwoBack->mfMean = lpPrev->mfMean * KF_WEIGHT_HI + lpTwoBack->mfMean * KF_WEIGHT_LO;
        lpPrev->mfMean    = (lpPrev->mfMean + lpTwoBack->mfMean) * KF_HALF;
    }

    if (muFrameCount >= 3u)
    {
        const u32 luNewer = luTwoBack;
        if (luTwoBack == 0u)
        {
            luTwoBack = muCapacity;
        }
        --luTwoBack;

        CHistogram* lpOlder = mpArray[luTwoBack].mpHistogram;
        CHistogram* lpNewer = mpArray[luNewer].mpHistogram;

        // First-order frame-to-frame differences stored on the older slot.
        lpOlder->mfVarianceDelta = lpNewer->mfVariance - lpOlder->mfVariance;
        lpOlder->mfMeanDelta     = lpNewer->mfMean - lpOlder->mfMean;
    }

    if (muFrameCount >= 4u)
    {
        const u32 luNewer = luTwoBack;
        if (luTwoBack == 0u)
        {
            luTwoBack = muCapacity;
        }

        CHistogram* lpNewer = mpArray[luNewer].mpHistogram;
        CHistogram* lpOlder = mpArray[luTwoBack - 1u].mpHistogram;

        // IIR-smooth the difference channels the same way as the raw channels.
        lpOlder->mfVarianceDelta = lpNewer->mfVarianceDelta * KF_WEIGHT_HI + lpOlder->mfVarianceDelta * KF_WEIGHT_LO;
        lpNewer->mfVarianceDelta = (lpOlder->mfVarianceDelta + lpNewer->mfVarianceDelta) * KF_HALF;

        lpOlder->mfMeanDelta = lpNewer->mfMeanDelta * KF_WEIGHT_HI + lpOlder->mfMeanDelta * KF_WEIGHT_LO;
        lpNewer->mfMeanDelta = (lpNewer->mfMeanDelta + lpOlder->mfMeanDelta) * KF_HALF;
    }
}

// @ 0x82A05648
// Run the dissolve detector against the slot three frames behind the read head
// (a look-back that only makes sense once >= 3 frames have been analysed). If
// that slot already carries the confirmed-cut marker the detector is reset;
// otherwise it is fed the current (and, past the third frame, the previous)
// histogram variance/mean deltas. All three detector calls tail-return `this`
// in the asm; the value is discarded by the sole caller, so this is void.
void CArrayLookahead::DetectDissolve()
{
    if (muFrameCount < 3u)
    {
        return;
    }

    // Index three frames back from the slot just behind the read head, wrapped
    // into the ring.
    u32 luIndex = muReadIndex;
    if (luIndex == 0u)
    {
        luIndex = muCapacity;
    }
    luIndex -= 1u;
    if (luIndex < 3u)
    {
        luIndex += muCapacity;
    }
    const u32 luTarget = luIndex - 3u;

    SElement& lrTarget = mpArray[luTarget];
    if (lrTarget.miEventType == KI_EVENT_CUT)
    {
        mDissolveDetector.Reset();
        return;
    }

    const CHistogram* lpCurHist = lrTarget.mpHistogram;

    if (muFrameCount == 3u)
    {
        // Third frame: no valid predecessor yet, so the "previous" deltas are 0.
        mDissolveDetector.Detect(0.0f, lpCurHist->mfVarianceDelta,
                                 0.0f, lpCurHist->mfMeanDelta);
        return;
    }

    // Feed the previous slot's deltas alongside the target slot's.
    u32 luPrev = luTarget;
    if (luPrev == 0u)
    {
        luPrev = muCapacity;
    }
    const CHistogram* lpPrevHist = mpArray[luPrev - 1u].mpHistogram;

    mDissolveDetector.Detect(lpPrevHist->mfVarianceDelta, lpCurHist->mfVarianceDelta,
                             lpPrevHist->mfMeanDelta, lpCurHist->mfMeanDelta);
}

// @ 0x82A05710
// Drain every frame pending between the read (tail) and write (head) heads.
// For each, build its histogram, compute up to three back-reference change
// flags, advance the read head, run the IIR filter and dissolve detector, then
// classify scene cuts / dissolves on a two-frame delay. Returns nothing (the
// asm leaves r3 undefined at the tail-return).
void CArrayLookahead::DetectEventInArray()
{
    const u32 luWrite = muWriteIndex;
    const u32 luRead  = muReadIndex;
    if (luWrite == luRead)
    {
        return;
    }

    // Number of frames queued between tail and head (wrapped).
    u32 luPending = (luWrite < luRead) ? (muCapacity - luRead + luWrite)
                                       : (luWrite - luRead);
    if (luPending == 0u)
    {
        return;
    }

    // Wrapped indices of the 1/2/3-frames-back slots. Each is (re)assigned in a
    // frame-count-guarded block below before it is read; a full pass (>= 4
    // frames) always sets all three. (The X360 asm seeds them from an
    // uninitialised stack slot, relying on the same guards.)
    u32       luPrev1 = 0u;
    u32       luPrev2 = 0u;
    u32       luPrev3 = 0u;
    SElement* lpPrev1 = nullptr;
    SElement* lpPrev2 = nullptr;
    SElement* lpPrev3 = nullptr;

    do
    {
        const u32 luCur = muReadIndex;
        SElement& lrCur = mpArray[luCur];

        // (a) Build the freshly-arrived frame's histogram. The asm passes
        // (image, miImageHeight, miImageWidth) into CalcHistogram's
        // (image, width, height) slots -- reproduced here register-for-register.
        lrCur.mpHistogram->CalcHistogram(lrCur.mpImageData,
                                         static_cast<u32>(miImageHeight),
                                         static_cast<u32>(miImageWidth));

        // (b) One frame back: luma high-moment ratio + primary hist-change.
        if (muFrameCount > 0u)
        {
            luPrev1 = (luCur != 0u) ? (luCur - 1u) : (muCapacity - 1u);
            lpPrev1 = &mpArray[luPrev1];

            const CHistogram* lpCurHist  = lrCur.mpHistogram;
            const CHistogram* lpPrevHist = lpPrev1->mpHistogram;

            s32 liLumaChanged = 0;
            if (lpPrevHist != nullptr
                && lpCurHist->miValid != 0
                && lpPrevHist->miValid != 0)
            {
                const f32 lfRatio = lpCurHist->mfHighMoment
                    / (lpPrevHist->mfHighMoment + KF_HIGHMOMENT_EPSILON);
                if (lfRatio > KF_HIGHMOMENT_RATIO)
                {
                    liLumaChanged = 1;
                }
            }
            lrCur.miLumaChanged = liLumaChanged;

            f32 lfSecondaryDist = 0.0f;
            f32 lfPrimaryDist   = 0.0f;
            lrCur.miHistChanged0 = lrCur.mpHistogram->isHistogramChanged(
                lpPrev1->mpHistogram, static_cast<u32>(miImageSize),
                &lfSecondaryDist, &lfPrimaryDist);
        }

        // (c) Two frames back: secondary hist-change.
        if (muFrameCount > 1u)
        {
            luPrev2 = (luPrev1 != 0u) ? (luPrev1 - 1u) : (muCapacity - 1u);
            lpPrev2 = &mpArray[luPrev2];

            f32 lfSecondaryDist = 0.0f;
            f32 lfPrimaryDist   = 0.0f;
            lrCur.miHistChanged1 = lrCur.mpHistogram->isHistogramChanged(
                lpPrev2->mpHistogram, static_cast<u32>(miImageSize),
                &lfSecondaryDist, &lfPrimaryDist);
        }

        // (d) Three frames back: tertiary hist-change.
        if (muFrameCount > 2u)
        {
            luPrev3 = (luPrev2 != 0u) ? (luPrev2 - 1u) : (muCapacity - 1u);
            lpPrev3 = &mpArray[luPrev3];

            f32 lfSecondaryDist = 0.0f;
            f32 lfPrimaryDist   = 0.0f;
            lrCur.miHistChanged2 = lrCur.mpHistogram->isHistogramChanged(
                lpPrev3->mpHistogram, static_cast<u32>(miImageSize),
                &lfSecondaryDist, &lfPrimaryDist);
        }

        lrCur.miProcessed = 1;

        // Advance: one more frame analysed; read head follows the running count.
        ++muFrameCount;
        muReadIndex = muFrameCount % muCapacity;

        if (miDissolveEnabled != 0)
        {
            Filter1stOrderDiffLumaVarMean();
        }

        if (muFrameCount >= 4u)
        {
            if (miDissolveEnabled != 0)
            {
                DetectDissolve();
                if (mDissolveDetector.miDissolveDetected != 0)
                {
                    // Back-annotate the frames spanned by the detected dissolve
                    // (clamped to the detected length and the ring headroom).
                    s32 liRun = static_cast<s32>(muFrameCount) - 3;
                    if (mDissolveDetector.miDissolveLength < liRun)
                    {
                        liRun = mDissolveDetector.miDissolveLength;
                    }
                    if (liRun >= static_cast<s32>(muCapacity) - 3)
                    {
                        liRun = static_cast<s32>(muCapacity) - 3;
                    }

                    if (liRun > 0)
                    {
                        u32 luMark = luPrev3;
                        s32 liRemaining = liRun;
                        do
                        {
                            mpArray[luMark].miEventType = KI_EVENT_DISSOLVE_RUN;
                            if (luMark == 0u)
                            {
                                luMark = muCapacity;
                            }
                            --liRemaining;
                            --luMark;
                        }
                        while (liRemaining != 0);
                    }

                    if (mDissolveDetector.miDissolveLength > 0)
                    {
                        mDissolveDetector.ComputeLocalFeature();
                    }

                    // Reset the detector's working state for the next dissolve;
                    // miMaxDissolveLength (+0x08) is deliberately preserved.
                    mDissolveDetector.miDissolveDetected = 0;
                    mDissolveDetector.miDissolveLength   = 0;
                    mDissolveDetector.mfReset0C          = 0.0f;
                    mDissolveDetector.mfReset10          = 0.0f;
                    mDissolveDetector.miReset14          = 0;
                    mDissolveDetector.miReset18          = 0;
                    mDissolveDetector.miReset1C          = 0;
                    mDissolveDetector.miReset20          = 0;
                    mDissolveDetector.miReset24          = 0;
                }
            }

            // Scene-cut / dissolve classifier over the two-frames-back slot.
            if (lpPrev2->miHistChanged0 != 0)
            {
                bool lbConfirmCut = false;
                if (lpPrev2->miLumaChanged != 0)
                {
                    if (lrCur.miHistChanged2 == 0)
                    {
                        lbConfirmCut = true;
                    }
                    else
                    {
                        // Reverse-direction luma ratio (two-back / current).
                        const CHistogram* lpCurHist  = lrCur.mpHistogram;
                        const CHistogram* lpPrevHist = lpPrev2->mpHistogram;
                        if (lpCurHist != nullptr
                            && lpPrevHist->miValid != 0
                            && lpCurHist->miValid != 0)
                        {
                            const f32 lfRatio = lpPrevHist->mfHighMoment
                                / (lpCurHist->mfHighMoment + KF_HIGHMOMENT_EPSILON);
                            if (lfRatio > KF_HIGHMOMENT_RATIO)
                            {
                                lbConfirmCut = true;
                            }
                        }
                    }
                }

                if (lbConfirmCut)
                {
                    lpPrev2->miEventType = KI_EVENT_CUT;
                    if (lpPrev1->miReserved14 != 0)
                    {
                        lpPrev1->miEventType = KI_EVENT_CUT;
                        lrCur.miHistChanged0 = 0;
                        lrCur.miEventType    = 0;
                    }
                    else
                    {
                        lpPrev1->miHistChanged0 = 0;
                        lpPrev1->miEventType    = 0;
                    }
                }
                else if (lpPrev3->miEventType == 0
                    && lpPrev1->miHistChanged1 != 0
                    && lrCur.miHistChanged2 != 0
                    && lpPrev1->miHistChanged0 == 0
                    && lrCur.miHistChanged0 == 0)
                {
                    lpPrev2->miEventType = KI_EVENT_CUT_WEAK;
                }
            }
        }

        --luPending;
    }
    while (luPending != 0);
}
