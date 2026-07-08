#pragma once

// =====================================================================================
// rw::audio::core::Gain -- the per-voice volume audio plug-in. A PlugIn processing-graph
// node that applies a single scalar gain to every channel of its input frame, de-clicking
// gain changes with a 64-sample (GAIN_DECLICK_FRAME_SIZE) linear ramp toward the target
// gain via rw::audio::core::CopyWithGainRamp.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF
// exist for this TU, and it is absent from the ProStreet08 rwaudio PDB, so each offset
// below is grounded directly in the disassembly of:
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
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext / AudioChannelBuffer

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Gain -- layout grounded in CreateInstance @0x82BA2BD0 and Process @0x82B97600:
//   +0x00  mBase (PlugInBaseView) -- the installed v-table word (+0x00), the base
//                 attribute-table pointer (+0x0C, pointed at &mfGain), and the active
//                 channel count mbChannelCount (+0x21, the per-channel Process-loop bound).
//   +0x28  mfGain         (f32, init 1.0; the live "gain" graph attribute the base
//                               attribute pointer references -- the ramp target)
//   +0x30  mfCurrentGain  (f32, init 1.0; the gain the current frame started at; the
//                               ramp's start value, latched to mfGain after each frame)
// The +0x24..+0x27 / +0x2C..+0x2F spans are alignment padding preserved so the named
// members land at the observed offsets.
// -------------------------------------------------------------------------------------
class Gain
{
public:
    static int    CreateInstance(Gain *self);                             // @0x82BA2BD0
    static char **GetPlugInDescRunTime();                                 // @0x82B97350
    static int    Process(Gain *self, AudioProcessContext *ctx,
                          char resetRamp);                                // @0x82B97600
    static void  *VectorDeletingDestructor(Gain *self, char flags);       // @0x82BA1710

    PlugInBaseView mBase;                 // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    f32        mfGain;                    // +0x28
    char       mPad2C[0x30 - 0x2C];       // +0x2C .. +0x2F
    f32        mfCurrentGain;             // +0x30
};

} // namespace core
} // namespace audio
} // namespace rw
