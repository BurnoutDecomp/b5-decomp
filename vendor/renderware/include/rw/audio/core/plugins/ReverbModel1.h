#pragma once

// =====================================================================================
// rw::audio::core::ReverbModel1 -- the "ReverbModel1" reverb plug-in.
//
// A PlugIn processing-graph node (sibling of Gain / Pan2D / the Iir2* filter shapes) that
// builds a Schroeder-style reverb out of the reverb building-block filters in
// ReverbFilters.h: a bank of six feedback CombFilter sections (each driven by its own
// DelayLine) summed and then coloured by up to three AllPassFilter sections (each driven
// by its own DelayLine). A per-frame profiling TimerHandle (registered with the System's
// TimerManager, callback RwacTimerClient) reconfigures the network whenever the reverb-time
// / room-size / damping attributes or the sample rate change.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. There is NO Feb-2007 leak source, NO
// DecFIGS DWARF, and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is
// grounded directly in the disassembly of the bodied member functions:
//   GetPlugInDescRunTime  @0x82B9AD98 -> the registered "ReverbModel1" descriptor
//   GetSize               @0x82B9ADA8 -> 1088 (0x440)
//   ReleaseEvent          @0x82B9ADB0
//   CalculateCombDistances@0x82B9AE30
//   CalculateCombDelays   @0x82B9AEE0
//   CalculateG1Values     @0x82B9AFD0
//   CalculateG2Values     @0x82B9B1F8
//   ReverbModel1 (ctor)   @0x82B9E938
//   GetPpuTicksEvent      @0x82B9E9D8
//   ~ReverbModel1         @0x82B9E9E0
//   `vector deleting destructor' @0x82B9EA48
//   UpdateLatencyAndDecay @0x82B9F6A8
//   Process               @0x82B9F7D0
//   ConfigureModel        @0x82BA3A10
//   RwacTimerClient       @0x82BA5720
//   CreateInstance        @0x82BA67F8
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at +0x00..+0x23 -- the same idiom Gain / Pan2D / the Iir2* filter shapes use so the data
// offsets are not shifted by a compiler-inserted vptr. +0x00 is the installed v-table word;
// mBase.mpAttributes (+0x0C) is pointed at the first input attribute (mfDecayTime @+0x28) so
// the base SetAttribute writes land there; the reverb "mode" (1 / 2 / 4 -- the channel/preset
// selector) lives in mBase.mbChannelCount (+0x21).
//
// The embedded sub-filters / delay-lines / timer widen on the x64 PC target (they carry
// pointer members), so the +0xNN annotations below are the X360 (32-bit) offsets and are
// documentary only -- only the member ORDER is load-bearing, and every access is by name /
// element (the same rule the DelayLine / TimerManager / Iir2Filters homes state). GetSize
// returns the X360 constant 1088, not sizeof(ReverbModel1).
// =====================================================================================

#include "types.hpp" // f32, f64, s32, u8, u32

#include "rw/audio/core/Iir2Filters.h"    // PlugInBaseView / AudioProcessContext
#include "rw/audio/core/ReverbFilters.h"  // AllPassFilter / CombFilter
#include "rw/audio/core/DelayLine.h"      // DelayLine
#include "rw/audio/core/TimerHandle.h"    // TimerHandle

namespace rw
{
namespace audio
{
namespace core
{

class System; // owning rwaudio sub-system (PlugIn.h) -- used only through the .cpp

// -------------------------------------------------------------------------------------
// ReverbModel1 -- byte-exact X360 layout (X360 sizeof 0x440 == 1088; GetSize is hard-coded).
//
// Six feedback comb sections (mComb / mCombDelay) plus up to three all-pass sections
// (mAllPass / mAllPassDelay). Per section the model caches distance (mfCombDistances),
// integer delay length (miCombDelays) and the two loop gains (mfCombG1 / mfCombG2); the
// all-pass network caches its gains / delay lengths at +0x41C.. and its live count /
// configured flags in the trailing bytes.
// -------------------------------------------------------------------------------------
class ReverbModel1
{
public:
    // ---- factory / v-table surface ----
    static int           GetSize();                                         // @0x82B9ADA8 -> 1088
    static char        **GetPlugInDescRunTime();                            // @0x82B9AD98
    static u32           GetPpuTicksEvent(ReverbModel1 *self);              // @0x82B9E9D8
    static ReverbModel1 *ReverbModel1_ctor(ReverbModel1 *self);            // @0x82B9E938
    static ReverbModel1 *Dtor(ReverbModel1 *self);                         // @0x82B9E9E0 (~ReverbModel1)
    static void         *VectorDeletingDestructor(ReverbModel1 *self,
                                                  char flags);              // @0x82B9EA48
    static int           CreateInstance(ReverbModel1 *self);                // @0x82BA67F8

    // ---- reverb network maths / configuration ----
    static int  CalculateCombDistances(ReverbModel1 *self, f32 *pRoomSize,
                                       f32 *pCombDistances);                // @0x82B9AE30
    static int  CalculateCombDelays(ReverbModel1 *self, f64 sampleRate,
                                    const f32 *pCombDistances,
                                    s32 *pCombDelays);                       // @0x82B9AEE0
    static int  CalculateG1Values(ReverbModel1 *self, f32 *pCombG1,
                                  f64 sampleRate);                          // @0x82B9AFD0
    static int  CalculateG2Values(ReverbModel1 *self, f32 *pCombG2);        // @0x82B9B1F8
    static int  ConfigureModel(ReverbModel1 *self, System *pSystem);        // @0x82BA3A10
    static void UpdateLatencyAndDecay(ReverbModel1 *self);                  // @0x82B9F6A8
    static int  RwacTimerClient(ReverbModel1 *self);                        // @0x82BA5720
    static int  ReleaseEvent(ReverbModel1 *self);                          // @0x82B9ADB0
    static int  Process(ReverbModel1 *self, AudioProcessContext *ctx);     // @0x82B9F7D0

    // ---- layout (X360 offsets documentary; access by name/element) ----
    PlugInBaseView mBase;                 // +0x00 .. +0x23 (mbChannelCount @+0x21 = reverb mode)
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    // ---- input attributes (8-byte-stride attribute table; base = &mfDecayTime) ----
    f32        mfDecayTime;               // +0x28 -- reverb time (attribute 0)
    char       mPad2C[0x30 - 0x2C];       // +0x2C
    f32        mfRoomSize;                // +0x30 -- room size / dimension (attribute 1)
    char       mPad34[0x38 - 0x34];       // +0x34
    f32        mfDamping;                 // +0x38 -- HF damping (attribute 2)
    char       mPad3C[0x40 - 0x3C];       // +0x3C
    // ---- filter banks ----
    AllPassFilter mAllPass[3];            // +0x40 -- all-pass colouration sections
    DelayLine     mAllPassDelay[3];       // +0x88 -- their delay lines
    TimerHandle   mTimer;                 // +0x13C -- profiling / reconfigure timer (mCpuTicks @+0x14C)
    // ---- cached "last applied" state (recompute decision) ----
    f32        mfLastSampleRate;          // +0x154
    f32        mfLastDecayTime;           // +0x158
    f32        mfLastRoomSize;            // +0x15C
    f32        mfLastDamping;             // +0x160
    // ---- per-comb network state (6 sections) ----
    f32        mfCombDistances[6];        // +0x164 -- reflection distances (metres)
    s32        miCombDelays[6];           // +0x17C -- delay lengths (samples; ascending)
    char       mPad194[0x1AC - 0x194];    // +0x194 -- reserved (untouched by the bodied members)
    f32        mfCombG1[6];               // +0x1AC -- comb decay gains
    f32        mfCombG2[6];               // +0x1C4 -- comb damping gains
    CombFilter mComb[6];                  // +0x1DC -- comb sections
    DelayLine  mCombDelay[6];             // +0x2B4 -- their delay lines
    // ---- all-pass network config ----
    f32        mfAllPassGain[3];          // +0x41C -- per-section all-pass gains
    s32        miAllPassDelaySamples[3];  // +0x428 -- per-section delay lengths (samples)
    f32        mfAllPassMixGain;          // +0x434 -- shared all-pass mix gain (2/(n-1))
    u8         mbAllPassConfigured;       // +0x438 -- the all-pass network has been sized
    u8         mbAllPassCount;            // +0x439 -- live all-pass section count (0..3)
    u8         mbTimerAdded;              // +0x43A -- RwacTimerClient timer is registered
    char       mPad43B[0x440 - 0x43B];    // +0x43B (X360 sizeof 0x440 == 1088; GetSize is hard-coded)
};

} // namespace core
} // namespace audio
} // namespace rw
