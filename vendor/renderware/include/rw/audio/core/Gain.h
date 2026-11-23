#pragma once

// =====================================================================================
// rw::audio::core::Gain -- the per-voice volume audio plug-in. A PlugIn processing-graph
// node that applies a single scalar gain to every channel of its input frame, de-clicking
// gain changes with a 64-sample (GAIN_DECLICK_FRAME_SIZE) linear ramp toward the target
// gain via rw::audio::core::CopyWithGainRamp.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. The X360 audio PDB records the callback
// declaration and the 56-byte instance. The layout below follows ARTIST's disassembly of:
//   CreateInstance         @0x82BA2BD0
//   GetPlugInDescRunTime   @0x82B97350
//   Process                @0x82B97600
//   `vector deleting destructor' @0x82BA1710
//
// Lowercase rw::audio:: namespaces match the third-party middleware API. The PlugIn base
// sub-object is embedded (composition, not inheritance) as a PlugInBaseView so the data
// offsets are not shifted by a compiler-inserted vptr -- the same idiom the Iir2* filter
// shapes and Resample use (see Iir2Filters.h / Resample.h).
// =====================================================================================

#include "types.hpp" // f32, u32, u8
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView, Mixer
#include "rw/audio/core/PlugIn.h"      // PlugIn::Attribute_t

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Gain -- layout grounded in CreateInstance @0x82BA2BD0 and Process @0x82B97600:
//   +0x00  mBase (PlugInBaseView) -- the installed v-table word (+0x00), the base
//                 attribute-table pointer (+0x0C, pointed at mAttribute), and the active
//                 channel count mbChannelCount (+0x21, the per-channel Process-loop bound).
//   +0x28  mAttribute[ATTRIBUTE_SETGAIN].f32         (f32, init 1.0; the live "gain" graph attribute the base
//                               attribute pointer references -- the ramp target)
//   +0x30  mPreviousGain  (f32, init 1.0; the gain the current frame started at; the
//                               ramp's start value, latched to mAttribute[ATTRIBUTE_SETGAIN].f32 after each frame)
// The attribute slot carries its original eight-byte alignment.
// -------------------------------------------------------------------------------------
class Gain
{
public:
    enum Attribute { ATTRIBUTE_SETGAIN = 0, ATTRIBUTE_MAX = 1 };

    static int    CreateInstance(Gain *self);                             // @0x82BA2BD0
    static char **GetPlugInDescRunTime();                                 // @0x82B97350
    static BufferStatus Process(PlugIn *pPlugIn, Mixer *pMixer,
                                bool discontinuity); // @0x82B97600
    static void  *VectorDeletingDestructor(Gain *self, char flags);       // @0x82BA1710

    PlugInBaseView mBase;                 // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    PlugIn::Attribute_t mAttribute[ATTRIBUTE_MAX]; // +0x28
    f32        mPreviousGain;             // +0x30

private:
    static PlugInDescRunTime sPlugInDescRunTime;
    static u32 GetSize(PlugInConfig *pConfig);
};

} // namespace core
} // namespace audio
} // namespace rw
