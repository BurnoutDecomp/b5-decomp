// ============================================================================
// CArrayLookahead.cpp  --  circular look-ahead ring of analysed video frames
//   Reconstructed from BURNOUT_X360_ARTIST.XEX (asm authoritative).
//
//   Implemented here (7 of 9 ledger functions):
//       CArrayLookahead::CArrayLookahead               @ 0x82A04BB8
//       CArrayLookahead::~CArrayLookahead              @ 0x82A055B8
//       CArrayLookahead::Initialize                    @ 0x82A04DE8
//       CArrayLookahead::ResetElement                  @ 0x82A04C68
//       CArrayLookahead::AddArrayElement               @ 0x82A04D30
//       CArrayLookahead::SetDissolveDumpFile           @ 0x82A05258
//       CArrayLookahead::Filter1stOrderDiffLumaVarMean @ 0x82A05048
//
//   BLOCKED (not implemented here) -- both drive the embedded
//   CSeqDissolveDetector, whose Reset/Detect/ComputeLocalFeature engine lives in
//   a separate, not-yet-reconstructed TU (see CSeqDissolveDetector.h):
//       CArrayLookahead::DetectDissolve                @ 0x82A05648
//       CArrayLookahead::DetectEventInArray            @ 0x82A05710
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
