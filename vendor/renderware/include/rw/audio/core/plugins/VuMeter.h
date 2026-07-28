#pragma once

// =====================================================================================
// rw::audio::core::VuMeter -- the "VuMeter" per-channel level-metering plug-in.
//
// A PlugIn processing-graph node (sibling of Gain / Pan2D1 / the Iir2* filter shapes, the
// Limiter1 dynamics stage and the Delay/ReverbModel1 effects). Each frame it walks the src
// channel buffer, tracking a running per-channel peak, an all-time "hold" peak, and a
// windowed RMS (root-mean-square) over mWindowSize samples, then de-interleaves those
// per-channel readings into speaker-position DISPLAY slots by the active channel count
// (mono / stereo / quad / 5.1). A per-frame profiling TimerHandle (registered on the
// System's TimerManager, callback RwacTimerClient) decays the momentary display to silence
// on any tick where Process did not run.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. There is NO Feb-2007 leak source, NO
// DecFIGS DWARF, and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is
// grounded directly in the disassembly of the bodied member functions:
//   GetSize                @0x82B98360 -> 336 (0x150)
//   CreateInstance         @0x82BA0EA8
//   EventEvent             @0x82BA0F80  (queues a deferred ResetHandler command)
//   ResetFunc              @0x82B9D038  (clear the display + rms/peak/hold columns)
//   ResetHandler           @0x82B9D130  (deferred-command trampoline -> ResetFunc)
//   RwacTimerClient        @0x82B9D0C8  (per-frame decay / window-size guard)
//   ReleaseEvent           @0x82B9D0A8
//   UpdateRunningPeakandRMS@0x82BA0FB0  (the per-sample peak / RMS accumulation)
//   UpdateAttributes       @0x82B9D158  (de-interleave readings into speaker slots)
//   Process                @0x82BA11F0  (per-frame entry)
//   `scalar deleting destructor' @0x82B9EB58
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at +0x00..+0x23 -- the same idiom Gain / the Iir2* shapes / Delay use so the data offsets
// are not shifted by a compiler-inserted vptr. +0x00 is the installed v-table word;
// mBase.mpAttributes (+0x0C) is pointed at self+0x40 by PlugIn::Initialize<VuMeter>; the
// active channel count lives in mBase.mbChannelCount (+0x21) and drives both the metering
// loop bound and the display de-interleave.
//
// The embedded TimerHandle widens on the x64 PC target (it carries pointer members), so the
// +0xNN annotations below are the X360 (32-bit) offsets and are documentary only -- only the
// member ORDER is load-bearing, and every access is by name / by array index. GetSize returns
// the X360 constant 336, not sizeof(VuMeter).
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u16, u32

#include <cstddef> // offsetof (layout pins below)

#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioChannelBuffer / AudioProcessContext
#include "rw/audio/core/TimerHandle.h" // TimerHandle

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// VuMeter -- byte-exact X360 layout (X360 sizeof 0x150 == 336; GetSize is hard-coded).
//
// The metering state is held as columnar per-channel arrays (4-byte stride, indexed by
// channel): rms sum-of-squares accumulator, computed RMS, running window peak, last-window
// peak, and all-time hold peak. The three DISPLAY bands are 8-byte-strided speaker-position
// slots (the second word of each slot is unused by the X360 bodies) that UpdateAttributes
// fills from the columns by channel count.
// -------------------------------------------------------------------------------------
class VuMeter
{
public:
    // Six output speaker positions and six input metering channels (5.1 max).
    enum { KI_MAX_CHANNELS = 6 };

    // One 8-byte display slot: the X360 stores only the level float; the trailing word is
    // reserved (never written by any VuMeter body).
    struct MeterLevel
    {
        f32 mfLevel;    // +0x00 -- the displayed level (RMS / peak / hold)
        f32 mfReserved; // +0x04 -- unused by the X360 bodies (8-byte display stride)
    };

    // ---- factory / v-table surface ----
    static int      GetSize();                          // @0x82B98360 -> 336
    static int      CreateInstance(VuMeter *self);      // @0x82BA0EA8 (1 = ok, 0 = timer failed)
    static void    *ScalarDeletingDestructor(VuMeter *self, char flags); // @0x82B9EB58

    // ---- event / lifecycle handlers ----
    static VuMeter *EventEvent(VuMeter *self);          // @0x82BA0F80 (queue deferred reset)
    static int      ReleaseEvent(VuMeter *self);        // @0x82B9D0A8

    // ---- reset ----
    static VuMeter *ResetFunc(VuMeter *self);           // @0x82B9D038
    // Returns the command record's own size -- the ring advance System::ExecuteCommands
    // applies. The X360 `li r3,8` is the CONSOLE sizeof; the host record is 16, so the body
    // returns sizeof(VuMeterResetCommand), never the literal.
    static int      ResetHandler(void *cmd);            // @0x82B9D130

    // ---- per-frame processing ----
    static VuMeter *RwacTimerClient(VuMeter *self);     // @0x82B9D0C8 (timer callback)
    static VuMeter *UpdateRunningPeakandRMS(VuMeter *self, AudioChannelBuffer *pSrc,
                                            int numSamples); // @0x82BA0FB0
    static void     UpdateAttributes(VuMeter *self);    // @0x82B9D158
    static int      Process(VuMeter *self, AudioProcessContext *ctx); // @0x82BA11F0

    // ---- layout (X360 offsets documentary; access by name / index) ----
    PlugInBaseView mBase;                       // +0x00 .. +0x23 (mbChannelCount @+0x21)
    TimerHandle    mTimer;                       // +0x24 -- per-frame decay/reset timer
    // ---- speaker-position display bands (8-byte slots) ----
    MeterLevel     mRmsDisplay[KI_MAX_CHANNELS];  // +0x40 -- de-interleaved RMS readout
    MeterLevel     mPeakDisplay[KI_MAX_CHANNELS]; // +0x70 -- de-interleaved last-window peak
    MeterLevel     mHoldDisplay[KI_MAX_CHANNELS]; // +0xA0 -- de-interleaved hold peak
    // ---- per-channel metering columns (4-byte stride, indexed by channel) ----
    f32            mRmsAccum[KI_MAX_CHANNELS];     // +0xD0  -- window sum-of-squares accumulator
    f32            mRms[KI_MAX_CHANNELS];          // +0xE8  -- computed RMS (sqrt(accum/window))
    f32            mPeakCurrent[KI_MAX_CHANNELS];  // +0x100 -- running peak within the window
    f32            mPeak[KI_MAX_CHANNELS];         // +0x118 -- last completed window's peak
    f32            mPeakHold[KI_MAX_CHANNELS];     // +0x130 -- all-time hold peak
    // ---- window / state ----
    u16            mWindowSize;                    // +0x148 -- RMS window length (default 479)
    u16            mWindowPos;                     // +0x14A -- running sample position in window
    u8             mbUpdated;                      // +0x14C -- Process ran since last timer tick
    u8             mbTimerRegistered;              // +0x14D -- the decay timer is registered

private:
    // Never called; a member-function body is a complete-class context with private access, so
    // the offsetof pins compile here where a bare in-class static_assert would not.
    //
    // Only POINTER-WIDTH-INVARIANT facts are pinned. The absolute +0xNN column offsets shift on
    // this x64 gate (TimerHandle widens), but the RELATIVE deltas the asm actually indexes by do
    // not: UpdateRunningPeakandRMS @0x82BA0FB0 bases r11 at self+0xD0 (mRmsAccum) and then uses
    // the fixed displacements 0x00 / 0x18 / 0x30 / 0x48 / 0x60 for accum / rms / peakCurrent /
    // peak / peakHold, and UpdateAttributes @0x82B9D158 steps the three display bands 0x30 apart.
    static void _AssertLayout()
    {
        // Display bands: 6 slots x 8 bytes = 0x30 per band (asm 0x40 / 0x70 / 0xA0).
        static_assert(sizeof(MeterLevel) == 8, "MeterLevel must be the 8-byte display stride");
        static_assert(offsetof(VuMeter, mPeakDisplay) - offsetof(VuMeter, mRmsDisplay) == 0x30,
                      "display band stride (asm 0x40 -> 0x70)");
        static_assert(offsetof(VuMeter, mHoldDisplay) - offsetof(VuMeter, mPeakDisplay) == 0x30,
                      "display band stride (asm 0x70 -> 0xA0)");

        // Metering columns, all relative to the r11 base self+0xD0 (mRmsAccum).
        static_assert(offsetof(VuMeter, mRms) - offsetof(VuMeter, mRmsAccum) == 0x18,
                      "mRms is 0x18(r11) (asm 0xD0 -> 0xE8)");
        static_assert(offsetof(VuMeter, mPeakCurrent) - offsetof(VuMeter, mRmsAccum) == 0x30,
                      "mPeakCurrent is 0x30(r11) (asm 0xD0 -> 0x100)");
        static_assert(offsetof(VuMeter, mPeak) - offsetof(VuMeter, mRmsAccum) == 0x48,
                      "mPeak is 0x48(r11) (asm 0xD0 -> 0x118)");
        static_assert(offsetof(VuMeter, mPeakHold) - offsetof(VuMeter, mRmsAccum) == 0x60,
                      "mPeakHold is 0x60(r11) (asm 0xD0 -> 0x130)");

        // Window/state tail packing (asm 0x148 / 0x14A / 0x14C / 0x14D).
        static_assert(offsetof(VuMeter, mWindowPos) - offsetof(VuMeter, mWindowSize) == 2,
                      "mWindowPos follows mWindowSize");
        static_assert(offsetof(VuMeter, mbUpdated) - offsetof(VuMeter, mWindowSize) == 4,
                      "mbUpdated follows the two halfwords");
        static_assert(offsetof(VuMeter, mbTimerRegistered) - offsetof(VuMeter, mbUpdated) == 1,
                      "mbTimerRegistered follows mbUpdated");
    }
};

} // namespace core
} // namespace audio
} // namespace rw
