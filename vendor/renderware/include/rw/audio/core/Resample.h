#pragma once

// =====================================================================================
// rw::audio::core::Resample -- the sample-rate-conversion audio plug-in. A PlugIn graph
// node that resamples its input to the output sample rate via a 16.16 fixed-point phase
// increment derived from (outputRate / inputRate) * pitch, clamped to a 4x ratio.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset, grounded in the disassembly of:
//   CreateInstance         @0x82BA37F0
//   GetOutputSamples       @0x82B9A8D0
//   GetPlugInDescRunTime   @0x82B9A850
//   GetSize                @0x82B9F3D8
//   LinearInterpolate      @0x82B9A918   (phase E; raw-decoded, no dossier)
//   PreProcess             @0x82B9AC10
//   Process                @0x82B9F3E8   (phase E callback wave)
//   SetResampleIncrement   @0x82B9A860
//   `vector deleting destructor' @0x82BA1C68
//
// ⭐ VENDOR HEADER (phase E 2026-08-28): references/Feb-2007/BrnEntityModuleUnity/SDKs/
// Packages/rwaudiocore/2.11.00/include/rw/audio/core/plugins/resample.h is the authoritative
// naming source and MATCHES this layout member-for-member. The earlier "no leak source"
// note is retired, and every placeholder name it forced is replaced by the vendor's own
// (mfPitch -> mAttribute[ATTRIBUTE_SETPITCH], mfResampleRatio -> mActualResampleRatio,
// mfLastRatio -> mRequestedResampleRatio, mfOutputSampleRate -> mPreviousSampleRate,
// muIncrementFixed -> mIncr16_16, muFractionAccumulator -> mAcc16_16, muBufferOffset ->
// mHistoryBufferOffset, muBlockSampleCount -> mOutputSamplesRequested, mbConsumedOffset ->
// mCurrentHistorySamples, mbFilterTaps -> mTargetHistorySamples). It also supplies the
// vendor's own GetHistoryBuffer() accessor -- the `this + mHistoryBufferOffset` idiom the
// asm open-codes -- and the RESAMPLE_UNITY / MAX_* constants.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API. The PlugIn base
// sub-object is embedded (composition, not inheritance) as a PlugInBaseView so the data
// offsets are not shifted by a compiler-inserted vptr -- the same idiom the Iir2* filter
// shapes use (see Iir2Filters.h).
// =====================================================================================

#include "types.hpp" // f32, u32, u16, u8
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext / AudioFormat
#include "rw/audio/core/PlugIn.h"      // PlugIn::Attribute_t (the 8-byte attribute slot)
#include "rw/audio/core/Voice.h"       // VoiceStageConfig (GetSize's config argument)

#include <cstddef>  // offsetof / size_t (the host history-buffer placement)
#include <cstdint>  // uintptr_t (the GetHistoryBuffer idiom)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Resample -- a fixed header followed by a per-channel history buffer of
// MAX_RESAMPLE_HISTORY (6) floats == 24 bytes per channel. On the console the header is
// 0x50 (80) bytes and GetSize() == 24 * numChannels + 80; ON THE HOST THE HEADER IS LARGER
// (the base view's pointers widen), so both GetSize and the history placement are computed
// from the HOST layout -- see KU_HISTORY_OFFSET below. Layout grounded in CreateInstance
// @0x82BA37F0 and named by the vendor resample.h:
//   +0x00  mBase (PlugInBaseView) -- the installed v-table word (+0x00), the base
//                 attribute-table pointer (+0x0C, pointed at the attribute block), and the
//                 active channel count mbChannelCount (+0x21, the history sizing count).
//   +0x28  mAttribute[1]          -- ATTRIBUTE_SETPITCH (init 1.0), the live pitch multiplier
//   +0x30  mActualResampleRatio   (f32; the CLAMPED ratio, <= MAX_RESAMPLE_RATIO -- set by
//                                       SetResampleIncrement, applied to the ctx gain)
//   +0x34  mRequestedResampleRatio(f32, init -1.0; the UNclamped ratio, cached for change
//                                       detection -- an impossible negative forces the
//                                       first PreProcess to compute an increment)
//   +0x38  mPreviousSampleRate    (f32, init 48000.0; the cached INPUT rate the upstream
//                                       source published -- Process's handshake compares it)
//   +0x3C  mIncr16_16             (u32, init 0; the 16.16 fixed-point phase increment)
//   +0x40  mAcc16_16              (u32, init 0; the 16.16 phase accumulator, fraction only)
//   +0x44  mHistoryBufferOffset   (u16; byte offset from `this` to the history buffer)
//   +0x46  mOutputSamplesRequested(u16; the requested output sample count, per PreProcess)
//   +0x48  mCurrentHistorySamples (u8,  init 0; how many carried-over input samples are live)
//   +0x49  mTargetHistorySamples  (u8,  init 2; the interpolation kernel's tap count)
// The +0x24 / +0x4A.. spans are alignment padding preserved so the named members land in
// the observed order.
// -------------------------------------------------------------------------------------
class Resample
{
public:
    // Vendor enums/constants (resample.h).
    enum ResampleMethod
    {
        LINEAR_INTERPOLATION = 0,
        MAX_METHODS = 1
    };

    enum Attribute
    {
        ATTRIBUTE_SETPITCH = 0,
        ATTRIBUTE_MAX = 1
    };

    enum { KU_RESAMPLE_UNITY = 0x010000 };  // 1.0 in 16.16
    enum { KU_MAX_RESAMPLE_RATIO = 4 };
    // 2 interpolation taps + up to MAX_RESAMPLE_RATIO input samples consumed per output
    // sample == the six floats of per-channel history the instance carries.
    enum { KU_MAX_RESAMPLE_HISTORY = 2 + KU_MAX_RESAMPLE_RATIO };
    enum { KU_MAX_INPUTSIZE = 8192, KU_MAX_OUTPUTSIZE = 8192 };

    static int          CreateInstance(Resample *self);                     // @0x82BA37F0
    static u32          GetOutputSamples(Resample *self, int inputSamples); // @0x82B9A8D0
    static char       **GetPlugInDescRunTime();                             // @0x82B9A850
    static int          GetSize(const VoiceStageConfig *config);            // @0x82B9F3D8
    static int          PreProcess(Resample *self, AudioProcessContext *ctx,
                                   int a3, int outputSamples);              // @0x82B9AC10
    static int          Process(Resample *self, AudioProcessContext *ctx,
                                bool discontinuity);                        // @0x82B9F3E8
    static Resample    *SetResampleIncrement(Resample *self, f32 ratio);    // @0x82B9A860
    static void        *VectorDeletingDestructor(Resample *self, char flags); // @0x82BA1C68

    // @0x82B9A918 -- the 2-tap linear interpolation kernel (raw-decoded from the XEX; the
    // IDA export has no dossier for it). Signature is the vendor's, and it matches the
    // register contract at Process's call site exactly.
    void LinearInterpolate(u32 frames, const f32 *pSrc, f32 *pDst,
                           u32 *pAccWhole, u32 *pAccFrac, u32 inc);

    // The vendor's own accessor for the variable-length tail (resample.h): the asm
    // open-codes this same `this + mHistoryBufferOffset` addition.
    f32 *GetHistoryBuffer()
    {
        return reinterpret_cast<f32 *>(reinterpret_cast<uintptr_t>(this) + mHistoryBufferOffset);
    }

    PlugInBaseView mBase;                 // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    PlugIn::Attribute_t mAttribute[ATTRIBUTE_MAX]; // +0x28 .. +0x2F
    f32        mActualResampleRatio;      // +0x30
    f32        mRequestedResampleRatio;   // +0x34
    f32        mPreviousSampleRate;       // +0x38
    u32        mIncr16_16;                // +0x3C
    u32        mAcc16_16;                 // +0x40
    u16        mHistoryBufferOffset;      // +0x44
    u16        mOutputSamplesRequested;   // +0x46
    u8         mCurrentHistorySamples;    // +0x48
    u8         mTargetHistorySamples;     // +0x49
    char       mPad4A[0x50 - 0x4A];       // +0x4A .. +0x4F -- the console header's tail pad
};

// HOST PLACEMENT OF THE VARIABLE-LENGTH HISTORY TAIL (phase E 2026-08-28).
// The console computes the history offset as `((this + 0x57) & ~7) - this` == 0x50 for its
// 16-aligned instance, i.e. align_up(consoleSizeof(Resample), 8). That 0x50 is a CONSOLE
// LITERAL: the host object is bigger (five widened base-view pointers), so reusing it would
// place the history buffer INSIDE the object and let the resampler scribble its own members
// -- exactly the class of bug the phase-D probe crash was. The host therefore aligns up its
// OWN sizeof, and GetSize/CreateInstance both derive from this one expression.
enum { KU_RESAMPLE_HISTORY_BYTES_PER_CHANNEL = 4 * Resample::KU_MAX_RESAMPLE_HISTORY }; // 24
inline u32 ResampleHistoryOffset()
{
    return static_cast<u32>((sizeof(Resample) + 7u) & ~static_cast<size_t>(7u));
}

} // namespace core
} // namespace audio
} // namespace rw
