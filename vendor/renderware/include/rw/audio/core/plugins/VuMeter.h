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
// authoritative for member offsets and stores. The Xbox 360 audio PDB corroborates the
// 0x150-byte layout, including the 18-slot attribute table at +0x40:
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
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u16, u32

#include <cstddef> // offsetof (layout pins below)

#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioChannelBuffer / AudioProcessContext
#include "rw/audio/core/PlugIn.h" // PlugIn::Attribute_t
#include "rw/audio/core/TimerHandle.h" // TimerHandle

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// VuMeter -- X360 layout (sizeof 0x150 == 336).
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
    enum
    {
        KI_MAX_CHANNELS = 6,
        KI_RMS_DISPLAY = 0,
        KI_PEAK_DISPLAY = KI_MAX_CHANNELS,
        KI_HOLD_DISPLAY = 2 * KI_MAX_CHANNELS,
        KI_ATTRIBUTE_COUNT = 3 * KI_MAX_CHANNELS
    };

    // ---- factory / v-table surface ----
    static int      GetSize();                          // @0x82B98360 -> 336
    static int      CreateInstance(VuMeter *self);      // @0x82BA0EA8 (1 = ok, 0 = timer failed)
    static void    *ScalarDeletingDestructor(VuMeter *self, char flags); // @0x82B9EB58

    // ---- event / lifecycle handlers ----
    static VuMeter *EventEvent(VuMeter *self);          // @0x82BA0F80 (queue deferred reset)
    static void     ReleaseEvent(VuMeter *self);        // @0x82B9D0A8

    // ---- reset ----
    static VuMeter *ResetFunc(VuMeter *self);           // @0x82B9D038
    // Returns the command record's own size -- the ring advance System::ExecuteCommands
    // applies. The X360 `li r3,8` is the CONSOLE sizeof; the host record is 16, so the body
    // returns sizeof(VuMeterResetCommand), never the literal.
    static int      ResetHandler(void *cmd);            // @0x82B9D130

    // ---- per-frame processing ----
    static void     RwacTimerClient(void *context, f32); // @0x82B9D0C8 (timer callback)
    static VuMeter *UpdateRunningPeakandRMS(VuMeter *self, AudioChannelBuffer *pSrc,
                                            int numSamples); // @0x82BA0FB0
    static void     UpdateAttributes(VuMeter *self);    // @0x82B9D158
    static int      Process(VuMeter *self, AudioProcessContext *ctx); // @0x82BA11F0

    // ---- layout (X360 offsets documentary; access by name / index) ----
    PlugInBaseView mBase;                       // +0x00 .. +0x23 (mbChannelCount @+0x21)
    TimerHandle    mTimer;                       // +0x24 -- per-frame decay/reset timer
    // RMS at +0x40, peak at +0x70, and hold at +0xA0 (0x82BA0ECC / 0x82BA0EE4).
    PlugIn::Attribute_t mAttribute[KI_ATTRIBUTE_COUNT];
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
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT(sizeof(PlugIn::Attribute_t) == 8, "native attribute slot stride");
        CGS_STATIC_ASSERT(__alignof(PlugIn::Attribute_t) == 8, "native attribute slot alignment");
        CGS_STATIC_ASSERT(offsetof(VuMeter, mAttribute) == 0x40, "native attribute table");
        CGS_STATIC_ASSERT(sizeof(VuMeter) == 0x150, "native meter extent");

        // Metering columns, all relative to the r11 base self+0xD0 (mRmsAccum).
        CGS_STATIC_ASSERT((offsetof(VuMeter, mRms) - offsetof(VuMeter, mRmsAccum) == 0x18),
                      "mRms is 0x18(r11) (asm 0xD0 -> 0xE8)");
        CGS_STATIC_ASSERT((offsetof(VuMeter, mPeakCurrent) - offsetof(VuMeter, mRmsAccum) == 0x30),
                      "mPeakCurrent is 0x30(r11) (asm 0xD0 -> 0x100)");
        CGS_STATIC_ASSERT((offsetof(VuMeter, mPeak) - offsetof(VuMeter, mRmsAccum) == 0x48),
                      "mPeak is 0x48(r11) (asm 0xD0 -> 0x118)");
        CGS_STATIC_ASSERT((offsetof(VuMeter, mPeakHold) - offsetof(VuMeter, mRmsAccum) == 0x60),
                      "mPeakHold is 0x60(r11) (asm 0xD0 -> 0x130)");

        // Window/state tail packing (asm 0x148 / 0x14A / 0x14C / 0x14D).
        CGS_STATIC_ASSERT((offsetof(VuMeter, mWindowPos) - offsetof(VuMeter, mWindowSize) == 2),
                      "mWindowPos follows mWindowSize");
        CGS_STATIC_ASSERT((offsetof(VuMeter, mbUpdated) - offsetof(VuMeter, mWindowSize) == 4),
                      "mbUpdated follows the two halfwords");
        CGS_STATIC_ASSERT((offsetof(VuMeter, mbTimerRegistered) - offsetof(VuMeter, mbUpdated) == 1),
                      "mbTimerRegistered follows mbUpdated");
    }
};

} // namespace core
} // namespace audio
} // namespace rw
