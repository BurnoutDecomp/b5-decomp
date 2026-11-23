#pragma once
#include "types.hpp"
#include "rw/audio/core/IFilter.h"
#include "rw/audio/core/DelayLine.h"

// ARTIST 82B6C3D8/82B6C6B0 initialize the IFilter base and trailing gains.
// PDB declarations confirm the base. Function pointers must widen with IFilter
// on PC; four console-width integers overlap the gains when dispatch is installed.
namespace rw { namespace audio { namespace core {
void AllPassFilterFunc(s32 count, f32 g1, f32 g2, f32* base, f32* work,
                       f32* feedforwardA, f32* feedforwardB, IFilter::FilterOutputMode mode);
f32 CombFilterFunc(s32 count, f32 g1, f32 g2, f32 g3, f32 g4, f32 state,
                   f32* base, f32* work, f32* accum, f32* feedforward, IFilter::FilterOutputMode mode);
class AllPassFilter : public IFilter
{
public:
    static AllPassFilter* AllPassFilter_ctor(AllPassFilter* self);
    static void AllPassFilterApplyFunc(IFilter* filter, s32 count, FilterOutputMode mode,
                                      s32 channel, LocationParams* ctx);
    static void AllPassFilterResetFunc(IFilter* filter);
    static AllPassFilter* SetGains(AllPassFilter* self, f32 g1, f32 g2);
    f32 mfGain1, mfGain2;
};
class CombFilter : public IFilter
{
public:
    CombFilter();
    static void CombFilterApplyFunc(IFilter* filter, s32 count, FilterOutputMode mode,
                                   s32 channel, LocationParams* ctx);
    static void CombFilterResetFunc(IFilter* filter);
    static CombFilter* SetGains(CombFilter* self, f32 g1, f32 g2, f32 g3, f32 g4);
    f32 mfGain1, mfGain2, mfGain3, mfGain4, mfState;
};
}}}
