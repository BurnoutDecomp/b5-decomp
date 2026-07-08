#pragma once

// =====================================================================================
// rw::audio::core::Resample -- the sample-rate-conversion audio plug-in. A PlugIn graph
// node that resamples its input to the output sample rate via a 16.16 fixed-point phase
// increment derived from (outputRate / inputRate) * pitch, clamped to a 4x ratio.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF
// exist for this TU, and it is absent from the ProStreet08 rwaudio PDB, so each offset
// below is grounded directly in the disassembly of:
//   CreateInstance         @0x82BA37F0
//   GetOutputSamples       @0x82B9A8D0
//   GetPlugInDescRunTime   @0x82B9A850
//   GetSize                @0x82B9F3D8
//   PreProcess             @0x82B9AC10
//   SetResampleIncrement   @0x82B9A860
//   `vector deleting destructor' @0x82BA1C68
//
// Lowercase rw::audio:: namespaces match the third-party middleware API. The PlugIn base
// sub-object is embedded (composition, not inheritance) as a PlugInBaseView so the data
// offsets are not shifted by a compiler-inserted vptr -- the same idiom the Iir2* filter
// shapes use (see Iir2Filters.h).
// =====================================================================================

#include "types.hpp" // f32, u32, u16, u8
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext / AudioFormat
#include "rw/audio/core/Voice.h"       // VoiceStageConfig (GetSize's config argument)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Resample -- base instance footprint is 0x50 (80) bytes, followed by a per-channel
// history buffer of 24 bytes per channel (GetSize() == 24 * numChannels + 80). Layout
// grounded in CreateInstance @0x82BA37F0:
//   +0x00  mBase (PlugInBaseView) -- the installed v-table word (+0x00), the base
//                 attribute-table pointer (+0x0C, pointed at &mfPitch), and the active
//                 channel count mbChannelCount (+0x21, the history-buffer sizing count).
//   +0x28  mfPitch                (f32, init 1.0; the live "pitch"/rate attribute the base
//                                       attribute pointer references)
//   +0x30  mfResampleRatio        (f32; the clamped resample ratio, <= 4.0 -- set by
//                                       SetResampleIncrement, applied to the ctx gain)
//   +0x34  mfLastRatio            (f32, init -1.0; cached ratio for change detection)
//   +0x38  mfOutputSampleRate     (f32, init 48000.0)
//   +0x3C  muIncrementFixed       (u32, init 0; 16.16 fixed-point sample increment)
//   +0x40  muFractionAccumulator  (u32, init 0; 16.16 phase accumulator)
//   +0x44  muBufferOffset         (u16; byte offset from `this` to the history buffer)
//   +0x46  muBlockSampleCount     (u16; the requested output sample count, per PreProcess)
//   +0x48  mbConsumedOffset       (u8,  init 0; read-cursor offset subtracted in PreProcess)
//   +0x49  mbFilterTaps           (u8,  init 2; interpolation kernel latency in samples)
// The +0x22..+0x27 / +0x2C..+0x2F / +0x4A..+0x4F spans are alignment padding preserved so
// the named members land at the observed offsets and the fixed header stays 0x50 bytes.
// -------------------------------------------------------------------------------------
class Resample
{
public:
    static int          CreateInstance(Resample *self);                     // @0x82BA37F0
    static u32          GetOutputSamples(Resample *self, int inputSamples); // @0x82B9A8D0
    static char       **GetPlugInDescRunTime();                             // @0x82B9A850
    static int          GetSize(const VoiceStageConfig *config);            // @0x82B9F3D8
    static int          PreProcess(Resample *self, AudioProcessContext *ctx,
                                   int a3, int outputSamples);              // @0x82B9AC10
    static Resample    *SetResampleIncrement(Resample *self, f32 ratio);    // @0x82B9A860
    static void        *VectorDeletingDestructor(Resample *self, char flags); // @0x82BA1C68

    PlugInBaseView mBase;                 // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    f32        mfPitch;                   // +0x28
    char       mPad2C[0x30 - 0x2C];       // +0x2C .. +0x2F
    f32        mfResampleRatio;           // +0x30
    f32        mfLastRatio;               // +0x34
    f32        mfOutputSampleRate;        // +0x38
    u32        muIncrementFixed;          // +0x3C
    u32        muFractionAccumulator;     // +0x40
    u16        muBufferOffset;            // +0x44
    u16        muBlockSampleCount;        // +0x46
    u8         mbConsumedOffset;          // +0x48
    u8         mbFilterTaps;              // +0x49
    char       mPad4A[0x50 - 0x4A];       // +0x4A .. +0x4F -- pad to the 0x50 header size
};

} // namespace core
} // namespace audio
} // namespace rw
