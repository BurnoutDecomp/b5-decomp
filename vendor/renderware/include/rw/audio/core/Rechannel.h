#pragma once

// =====================================================================================
// rw::audio::core::Rechannel -- the channel-remap audio plug-in. A PlugIn processing-graph
// node that forces its input frame to a fixed output channel count: when the incoming
// buffer's channel count differs from the plug-in's configured output count it remaps the
// source channels into the destination buffer through rw::audio::core::ReChannelGainWrite
// (the store-for-store channel-fold kernel the Send + Gain paths also drive), ping-pongs
// the context's src/dst channel-buffer slots, and records the new active channel count back
// into the context.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF exist
// for this TU, and it is absent from the ProStreet08 rwaudio PDB, so each offset below is
// grounded directly in the disassembly of:
//   CreateInstance         @0x82BA37D0
//   GetPlugInDescRunTime   @0x82B9A718
//   GetSize                @0x82B982E0
//   PreProcess             @0x82B97F98
//   Process                @0x82B9A728
//   `scalar deleting destructor' @0x82BA1C20
//
// Lowercase rw::audio:: namespaces match the third-party middleware API. The PlugIn base
// sub-object is embedded (composition, not inheritance) as a PlugInBaseView so the data
// offsets are not shifted by a compiler-inserted vptr -- the same idiom the Iir2* filter
// shapes, Gain and Resample use (see Iir2Filters.h / Gain.h / Resample.h).
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
// Rechannel -- GetSize() @0x82B982E0 returns 36 (0x24) == sizeof(PlugInBaseView), and the
// only members Process/CreateInstance touch are the embedded base view's installed vtable
// word (+0x00) and its two channel-count bytes:
//   mBase.mbFlag20      (+0x20) -- latched incoming source channel count (set when a frame
//                                  carries no samples, so the count is tracked while idle)
//   mBase.mbChannelCount(+0x21) -- the configured OUTPUT channel count the remap targets.
// The Rechannel therefore adds NO fields of its own beyond the base view.
// -------------------------------------------------------------------------------------
class Rechannel
{
public:
    static int    CreateInstance(Rechannel *self);                          // @0x82BA37D0
    static char **GetPlugInDescRunTime();                                   // @0x82B9A718
    static int    GetSize();                                                // @0x82B982E0
    static int    PreProcess(Rechannel *self, AudioProcessContext *ctx,
                             int a3, int outputSamples);                    // @0x82B97F98
    static int    Process(Rechannel *self, AudioProcessContext *ctx);       // @0x82B9A728
    static void  *ScalarDeletingDestructor(Rechannel *self, char flags);    // @0x82BA1C20

    PlugInBaseView mBase; // +0x00 .. +0x23 -- the whole instance (GetSize() == 36)
};

} // namespace core
} // namespace audio
} // namespace rw
