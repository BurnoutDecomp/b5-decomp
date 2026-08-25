#ifndef BRN_INTERPOLATOR_H
#define BRN_INTERPOLATOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// BrnGui::Interpolator<T> / BrnGui::DeltaInterpolator - the small GUI easing helpers the custom
// HUD renderers (chiefly BoostBarRenderer) embed. DWARF home: BrnBoostBarRenderer.h:45 (the
// Interpolator template, h:45-112) and :117 (DeltaInterpolator, h:117-160); they live in this
// sibling header so the other renderers can reach them without pulling the whole boost-bar class.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF:
//   BrnGui::Interpolator<float>::GetCurrentValue @ 0x82448898
//   (the other methods are header inlines the X360 folds into every caller; shapes are the
//    DWARF's, bodies are the inlined forms the BoostBarRenderer ctor/Construct/Update attest)
//
// LAYOUT (DWARF member names; the @0x82448898 asm offsets):
//   Interpolator<f32>:  mStartValue @+0, mEndValue @+4, mfStartTime @+8, mfEndTime @+0xC.
//   DeltaInterpolator:  mfCurrentValue @+0, mfCurrentTime @+4, mfDeltaPerSec @+8,
//                       mfMinValue @+0xC, mfMaxValue @+0x10.
// An unset TIME key holds the -FLT_MAX sentinel; IsValid() is true only when BOTH time keys are
// set (the X360 GetCurrentValue reads the +8/+0xC pair against the sentinel).
//
// ⭐ FORMULA CORRECTED 2026-08-24 (boost-bar campaign). The previous recon read the tail
// `vmaddfp v0, v12, v0, v13` in the 4-operand order and produced the nonsense
// `(end-start)*start + t`. It is the VMX128 3-operand form (vD = vA*vB + vD -- the same
// operand-order trap the camera-blend campaign hit): v0(old) = mStartValue, v12 = end-start,
// v13 = the clamped t, so the result is the TEXTBOOK clamped lerp
//   mStartValue + clamp((t - startTime)/(endTime - startTime), 0, 1) * (mEndValue - mStartValue).

namespace BrnGui
{
// DWARF BrnBoostBarRenderer.h:45. Times ease a value pair: SetStart/SetEnd key the segment,
// GetCurrentValue maps a query time onto the saturated [0,1] fraction between the two time keys
// and lerps the value pair by it.
template <typename T>
class Interpolator
{
public:
    // The -FLT_MAX sentinel an unset time key holds.
    static const f32 KF_UnsetSentinel; // = -3.4028235e38f

    // h:47 -- both time keys unset (the BoostBarRenderer ctor stores the sentinel pair).
    Interpolator()
        : mStartValue(T())
        , mEndValue(T())
        , mfStartTime(KF_UnsetSentinel)
        , mfEndTime(KF_UnsetSentinel)
    {
    }

    // h:54 -- forget the segment (both time keys back to the sentinel).
    void Invalidate()
    {
        mfStartTime = KF_UnsetSentinel;
        mfEndTime   = KF_UnsetSentinel;
    }

    // h:60 -- true only when BOTH time keys are set (the X360 @0x82448898 head reads the
    // +8/+0xC TIME pair against the sentinel).
    bool IsValid() const
    {
        return mfStartTime != KF_UnsetSentinel && mfEndTime != KF_UnsetSentinel;
    }

    // h:65 -- a keyed segment that has not finished at lfTime.
    bool IsActive(f32 lfTime) const
    {
        return IsValid() && lfTime < mfEndTime;
    }

    // h:72 -- a keyed segment whose end has passed at lfTime.
    bool IsFinished(f32 lfTime) const
    {
        return IsValid() && lfTime >= mfEndTime;
    }

    // h:77 / h:83 -- key one end of the segment.
    void SetStart(T lStartValue, f32 lfStartTime)
    {
        mStartValue = lStartValue;
        mfStartTime = lfStartTime;
    }
    void SetEnd(T lEndValue, f32 lfEndTime)
    {
        mEndValue = lEndValue;
        mfEndTime = lfEndTime;
    }

    // h:100 / X360 @0x82448898 -- assert the segment is keyed, then the clamped lerp.
    T GetCurrentValue(f32 lfTime) const
    {
        CGS_ASSERT(IsValid(), "IsValid()");

        f32 lfFraction = (lfTime - mfStartTime) / (mfEndTime - mfStartTime);
        if (lfFraction < 0.0f) lfFraction = 0.0f;
        if (lfFraction > 1.0f) lfFraction = 1.0f;
        return mStartValue + (mEndValue - mStartValue) * lfFraction;
    }

    // DWARF member order (the @0x82448898 asm offsets).
    T   mStartValue;  // @+0x0
    T   mEndValue;    // @+0x4
    f32 mfStartTime;  // @+0x8
    f32 mfEndTime;    // @+0xC
};

template <typename T>
const f32 Interpolator<T>::KF_UnsetSentinel = -3.4028235e38f;

// DWARF BrnBoostBarRenderer.h:117. A rate-driven value: SetDelta keys a per-second rate at a
// time, GetCurrentValue advances the value by rate * elapsed and clamps it into [min, max].
// All methods are X360 header inlines; the member seed values are the BoostBarRenderer ctor's
// stores (cur/curTime/delta = 0, min = -FLT_MAX, max = +FLT_MAX) and Construct's SetRange(0, 1).
class DeltaInterpolator
{
public:
    // h:119 -- the ctor the BoostBarRenderer ctor inlines (zeros + the open range).
    DeltaInterpolator()
        : mfCurrentValue(0.0f)
        , mfCurrentTime(0.0f)
        , mfDeltaPerSec(0.0f)
        , mfMinValue(-3.4028235e38f)
        , mfMaxValue(3.4028235e38f)
    {
    }

    // h:128 -- clamp bounds for the advancing value.
    void SetRange(f32 lfMinValue, f32 lfMaxValue)
    {
        mfMinValue = lfMinValue;
        mfMaxValue = lfMaxValue;
    }

    // h:134 -- jump the value (and rebase the advance time).
    void SetCurrentValue(f32 lfValue, f32 lfTime)
    {
        mfCurrentValue = lfValue;
        mfCurrentTime  = lfTime;
    }

    // h:140 -- key a new per-second rate from lfTime (advancing to lfTime first so the old
    // rate applies up to the switch point -- the inlined X360 form GetCurrentValue then
    // continues from).
    void SetDelta(f32 lfDeltaPerSec, f32 lfTime)
    {
        GetCurrentValue(lfTime);
        mfDeltaPerSec = lfDeltaPerSec;
    }

    // h:146 -- advance the value by rate * elapsed, clamped into [min, max]; latch the time.
    f32 GetCurrentValue(f32 lfTime)
    {
        mfCurrentValue += mfDeltaPerSec * (lfTime - mfCurrentTime);
        if (mfCurrentValue < mfMinValue) mfCurrentValue = mfMinValue;
        if (mfCurrentValue > mfMaxValue) mfCurrentValue = mfMaxValue;
        mfCurrentTime = lfTime;
        return mfCurrentValue;
    }

    // DWARF member order (h:156-160).
    f32 mfCurrentValue; // @+0x00
    f32 mfCurrentTime;  // @+0x04
    f32 mfDeltaPerSec;  // @+0x08
    f32 mfMinValue;     // @+0x0C
    f32 mfMaxValue;     // @+0x10
};
}

#endif // BRN_INTERPOLATOR_H
