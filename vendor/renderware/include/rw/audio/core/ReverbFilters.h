#pragma once
#include "types.hpp"
#include "rw/audio/core/IFilter.h"
#include "rw/audio/core/DelayLine.h"

// ARTIST 82B6C3D8/82B6C6B0 initialize the IFilter base and trailing gains.
// PDB declarations confirm the base. Function pointers must widen with IFilter
// on PC; four console-width integers overlap the gains when dispatch is installed.
namespace rw { namespace audio { namespace core {
void AllPassFilterFunc(s32 count, f32 g1, f32 g2, f32* base, f32* work,
                       f32* feedforwardA, f32* feedforwardB, s32 lowPass);
f32 CombFilterFunc(s32 count, f32 g1, f32 g2, f32 g3, f32 g4, f32 state,
                   f32* base, f32* work, f32* accum, f32* feedforward, s32 lowPass);
class AllPassFilter : public IFilter
{
public:
    // ApplyFilter passes this exact six-pointer record. ARTIST +8/+0xC
    // are the crossfade tap/ramp POINTERS, not two integer flags.
    using Context = DelayLine::TapContext;
    static AllPassFilter* AllPassFilter_ctor(AllPassFilter* self);
    static void* AllPassFilterApplyFunc(AllPassFilter* self, s32 count, s32 src,
                                        s32 channel, Context* ctx);
    static AllPassFilter* AllPassFilterResetFunc(AllPassFilter* self);
    static AllPassFilter* SetGains(AllPassFilter* self, f32 g1, f32 g2);
    f32 mfGain1, mfGain2;
};
class CombFilter : public IFilter
{
public:
    using Context = DelayLine::TapContext;
    static CombFilter* CombFilter_ctor(CombFilter* self);
    static void* CombFilterApplyFunc(CombFilter* self, s32 count, s32 src,
                                     s32 channel, Context* ctx);
    static CombFilter* CombFilterResetFunc(CombFilter* self);
    static CombFilter* SetGains(CombFilter* self, f32 g1, f32 g2, f32 g3, f32 g4);
    f32 mfGain1, mfGain2, mfGain3, mfGain4, mfState;
};
}}}
