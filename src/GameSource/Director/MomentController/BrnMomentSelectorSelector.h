#pragma once

// =============================================================================
// BrnDirector::Selector<T, N>  --  a small fixed-capacity weighted-random picker.
//
// Reconstructed from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Director/MomentController/BrnMomentSelector.h,
// 'struct BrnDirector::Selector<uint32_t,10u>' @ BrnMomentSelector.h:50) and the X360
// bodies of its only instantiation. The IDA demangler collapsed the class name to the
// template fragment 'int,10>' (both Selector<uint32_t,10> and its embedded
// Array<uint32_t,10> mangle through the same suffix); the DWARF is authoritative for the
// real name, member layout, and method shapes.
//
// Used by BrnDirector::MomentSelector::SelectBestRandomMomentWithExclusion, which builds a
// local Selector<uint32_t,10>, AddElement()s each candidate moment's {weight, index}, then
// GetSelection(Random&) picks one weighted-randomly.
//
// LAYOUT (DWARF-authoritative, asm-confirmed by the +0x2C / +0x58 / +0x80 offsets across
// AddElement @0x82213D50, CalculateIntervals @0x822142D8, GetSelection @0x8222EE38):
//   +0x00  Array<u32,10> mOutputArray      (maElements[10] 40B + miCount @+0x28 -> this+0x28)
//   +0x2C  Array<f32,10> mOriginalWeights  (maElements[10] 40B + miCount @+0x28 -> this+0x54)
//   +0x58  Array<f32,9>  mIntervalEnds     (maElements[9]  36B + miCount @+0x24 -> this+0x7C)
//   +0x80  bool          mbNormalised
// The two f32 interval/weight arrays are mutable because GetSelection() (const) lazily
// (re)builds them via CalculateIntervals() (const).
//
// Bodies are kept INLINE in this header (mirrors how CgsArray.h bodies its generics): the
// sole <uint32_t,10> instantiation lives in CgsArrayU32_10.cpp (the mOutputArray member) plus
// the already-committed CgsArrayFloat10.cpp / CgsArrayFloat9.cpp (weights / intervals).
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"        // Array<T,N>
#include "GameShared/GameClasses/Numeric/CgsRandom.h"          // CgsNumeric::Random (GetSelection draw)
#include "rw/math/fpu/scalar_operation.h"                      // rw::math::fpu::IsZero (CalculateIntervals assert)

namespace BrnDirector
{

// DWARF BrnMomentSelector.h:50. Only ever instantiated at <uint32_t, 10u>.
template <typename T, u32 N>
class Selector
{
public:
    // DWARF BrnMomentSelector.h:54. Bring all three embedded arrays to their
    // empty-but-usable state and mark the interval cache stale.
    void Construct()
    {
        mOutputArray.Construct();
        mOriginalWeights.Construct();
        mIntervalEnds.Construct();
        mbNormalised = false;
    }

    // DWARF BrnMomentSelector.h:327  (X360 @ 0x82213D50). Add one candidate
    // {weight in (0,1], output value}: append the weight, append the value, invalidate cache.
    void AddElement(f32 lfWeight0To1, const T& lrElement)
    {
        CGS_ASSERT(lfWeight0To1 > 0.0f, "lfWeight0To1 > 0.0f");
        CGS_ASSERT(lfWeight0To1 <= 1.0f, "lfWeight0To1 <= 1.0f");
        mOriginalWeights.Append(lfWeight0To1);
        mOutputArray.Append(lrElement);
        mbNormalised = false;
    }

    // DWARF BrnMomentSelector.h:66  (X360 @ 0x8222EE38). Weighted-random pick over the
    // added elements: draw a uniform [0,1) value and map it through the cumulative
    // interval table.
    const T& GetSelection(CgsNumeric::Random& lrRandom) const
    {
        CGS_ASSERT(mOriginalWeights.GetLength() > 0, "mOriginalWeights.GetLength() > 0");

        if (!mbNormalised)
        {
            CalculateIntervals();
        }

        const f32 lfRandom = lrRandom.RandomFloat();

        for (u32 luIndex = 0; luIndex < mIntervalEnds.GetLength(); ++luIndex)
        {
            if (lfRandom < mIntervalEnds[luIndex])
            {
                return mOutputArray[luIndex];
            }
        }

        // Random fell in the final (fallthrough) bucket: return the last output value.
        return mOutputArray[mOutputArray.GetLength() - 1];
    }

    // DWARF BrnMomentSelector.h:340. Number of candidates added so far.
    u32 GetLength() const { return mOutputArray.GetLength(); }

private:
    // DWARF BrnMomentSelector.h:74  (X360 @ 0x822142D8). Normalise the accumulated weights
    // into a running cumulative-distribution table (mIntervalEnds). Lazily invoked by
    // GetSelection when mbNormalised is false. const because it only mutates the mutable
    // interval cache + mbNormalised flag.
    void CalculateIntervals() const
    {
        CGS_ASSERT(mOriginalWeights.GetLength() > 0, "mOriginalWeights.GetLength() > 0");

        f32 lfWeightsSum = 0.0f;
        for (u32 luIndex = 0; luIndex < mOriginalWeights.GetLength(); ++luIndex)
        {
            lfWeightsSum += mOriginalWeights[luIndex];
        }

        CGS_ASSERT(!rw::math::fpu::IsZero(lfWeightsSum), "!rw::math::fpu::IsZero(lfWeightsSum)");

        const f32 lfInverseSum = 1.0f / lfWeightsSum;
        f32 lfRunningTotal = 0.0f;
        mIntervalEnds.Clear();
        // Only N-1 interval ends are needed for N buckets (the final bucket is fallthrough).
        for (u32 luIndex = 0; luIndex < mOriginalWeights.GetLength() - 1; ++luIndex)
        {
            lfRunningTotal += mOriginalWeights[luIndex] * lfInverseSum;
            mIntervalEnds.Append(lfRunningTotal);
        }

        mbNormalised = true;
    }

    // DWARF BrnMomentSelector.h:76 / :77 / :84 / :85.
    Array<T,   N>             mOutputArray;      // +0x00
    mutable Array<f32, N>     mOriginalWeights;  // +0x2C (rebuilt under const GetSelection)
    mutable Array<f32, N - 1> mIntervalEnds;     // +0x58 (cumulative-distribution cache)
    mutable bool              mbNormalised;      // +0x80 (cache-valid flag)
};

} // namespace BrnDirector
