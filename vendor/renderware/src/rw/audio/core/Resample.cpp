// =====================================================================================
// rw::audio::core::Resample bodies -- the sample-rate-conversion audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 PDB entry exist for this type.
//   CreateInstance         @0x82BA37F0 -- store-for-store
//   GetOutputSamples       @0x82B9A8D0 -- branch-for-branch
//   GetPlugInDescRunTime   @0x82B9A850 -- returns the registered "Resample" descriptor
//   GetSize                @0x82B9F3D8 -- 24 * numChannels + 80
//   PreProcess             @0x82B9AC10 -- store-for-store / branch-for-branch
//   SetResampleIncrement   @0x82B9A860 -- store-for-store
//   `vector deleting destructor' @0x82BA1C68 -- reinstalls base vtable, conditional free
// See Resample.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Resample.h"

#include <cstring>  // std::memset (the X360 XMemSet)
#include <cstdint>  // uintptr_t

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F504 -- the Resample v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the deleting destructor reinstalls before any free. off_82F8F8E0 -- the
// registered run-time descriptor record (its +0x00 label is the string "Resample"). These
// are opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
static void *const KRS_ResampleVTable = nullptr;    // off_8217F504
static void *const KRS_BasePlugInVTable = nullptr;  // off_820AA810
static char       *g_ResampleDesc = nullptr;        // off_82F8F8E0 (the "Resample" record)

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9A850 -- return the address of the registered descriptor
// record (its label is the string "Resample").
// -------------------------------------------------------------------------------------
char **Resample::GetPlugInDescRunTime()
{
    return &g_ResampleDesc; // &off_82F8F8E0
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B9F3D8 -- the per-instance allocation size: a 0x50-byte header plus a
// 24-byte-per-channel history buffer. The channel count is the low byte of the stage
// config's +0x08 field (the same byte Voice passes as the CreateInstance flag).
// -------------------------------------------------------------------------------------
int Resample::GetSize(const VoiceStageConfig *config)
{
    return 24 * static_cast<u8>(config->mFlagAndField8) + 80;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA37F0 -- placement-init a Resample over `self`.
//   if (self) self->mBase.mpVTable = off_8217F504;
//   self->mBase.mpAttributes(+0x0C) = &self->mfPitch(+0x28);
//   zero the 24*numChannels history buffer at the first 8-aligned offset past the header;
//   record that offset at +0x44; seed the rate/ratio/accumulator/latency fields.
//   return 1;
// -------------------------------------------------------------------------------------
int Resample::CreateInstance(Resample *self)
{
    if (self)
        self->mBase.mpVTable = KRS_ResampleVTable; // off_8217F504

    // Point the base attribute-table slot at the live pitch attribute (self+0x28) so the
    // base SetAttribute writes land on mfPitch.
    self->mBase.mpAttributes = &self->mfPitch;

    // History buffer: 24 bytes per channel, placed at the first 8-aligned address past the
    // 0x50-byte header window (== self+0x50 for the 16-aligned instance). The X360 build is
    // 32-bit; the align-to-8 mask is applied to the pointer's integer value -- uintptr_t
    // keeps the semantics on the 64-bit gate host without truncating.
    const u32 historyBytes = 24u * self->mBase.mbChannelCount;
    const uintptr_t base = reinterpret_cast<uintptr_t>(self);
    const u16 bufferOffset =
        static_cast<u16>(((base + 0x57u) & ~static_cast<uintptr_t>(7u)) - base);
    self->muBufferOffset = bufferOffset;
    std::memset(reinterpret_cast<char *>(self) + bufferOffset, 0, historyBytes);

    self->mfOutputSampleRate = 48000.0f;   // flt_820AA808 @ +0x38
    self->muIncrementFixed = 0;            // @ +0x3C
    self->muFractionAccumulator = 0;       // @ +0x40
    self->mbConsumedOffset = 0;            // @ +0x48
    self->mfPitch = 1.0f;                  // flt_82001C98 @ +0x28
    self->mfLastRatio = -1.0f;             // flt_820037C8 @ +0x34
    self->mbFilterTaps = 2;                // @ +0x49
    return 1;
}

// -------------------------------------------------------------------------------------
// SetResampleIncrement @0x82B9A860 -- convert the resample ratio to a 16.16 fixed-point
// increment (round-to-nearest), clamp it (and the stored ratio) to 4x, and cache the raw
// ratio for change detection.
//   inc = round(ratio * 65536);
//   if (inc > 0x40000) { inc = 0x40000; self->mfResampleRatio = 4.0; }
//   else               { self->mfResampleRatio = ratio; }
//   self->mfLastRatio = ratio; self->muIncrementFixed = inc;
// -------------------------------------------------------------------------------------
Resample *Resample::SetResampleIncrement(Resample *self, f32 ratio)
{
    f32 scaled = ratio * 65536.0f;                                   // flt_8217F368
    scaled = (scaled < 0.0f) ? (scaled - 0.5f) : (scaled + 0.5f);    // round to nearest
    int increment = static_cast<int>(scaled);                        // fctiwz (truncate)

    if (increment > 0x40000)
    {
        increment = 0x40000;
        self->mfResampleRatio = 4.0f;   // flt_82004EF4 @ +0x30
    }
    else
    {
        self->mfResampleRatio = ratio;  // @ +0x30
    }
    self->mfLastRatio = ratio;                           // @ +0x34
    self->muIncrementFixed = static_cast<u32>(increment); // @ +0x3C
    return self;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9AC10 -- recompute the increment if the (outputRate/inputRate)*pitch
// ratio has changed, work out this block's produced sample count, latch the requested
// count, and scale the context's downstream gain by the clamped ratio.
// -------------------------------------------------------------------------------------
int Resample::PreProcess(Resample *self, AudioProcessContext *ctx, int /*a3*/,
                         int outputSamples)
{
    const f32 inputRate = ctx->mpFormat->mfSampleRate;             // *(ctx+0x30018)->+0x0C
    const f32 ratio = (self->mfOutputSampleRate / inputRate) * self->mfPitch;
    if (self->mfLastRatio != ratio)
        SetResampleIncrement(self, ratio);

    const u32 phase = self->muIncrementFixed * static_cast<u32>(outputSamples)
                    + self->muFractionAccumulator;
    int result = static_cast<int>(phase >> 16)
               - self->mbConsumedOffset + self->mbFilterTaps;
    if (result < 0)
        result = 0;

    self->muBlockSampleCount = static_cast<u16>(outputSamples);        // sth r6 @ +0x46
    ctx->mfResampleGain = self->mfResampleRatio * ctx->mfResampleGain; // *(ctx+0x30028) *= +0x30
    return result;
}

// -------------------------------------------------------------------------------------
// GetOutputSamples @0x82B9A8D0 -- given `inputSamples` available, how many output samples
// this stage yields. Returns 0 if the input can't cover the kernel latency, 0x2000 while
// the increment is still unset, else floor(((usable<<16) - fraction - 1) / increment).
// -------------------------------------------------------------------------------------
u32 Resample::GetOutputSamples(Resample *self, int inputSamples)
{
    const int usable = inputSamples - self->mbFilterTaps + 1;
    if (usable <= 0)
        return 0;

    const u32 increment = self->muIncrementFixed;
    if (increment == 0)
        return 0x2000;

    const u32 fraction = self->muFractionAccumulator;
    // asm `twllei increment, 0` is the compiler's div-by-zero trap; already guarded above.
    return ((static_cast<u32>(usable) << 16) - fraction - 1u) / increment;
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA1C68
//   self->mBase.mpVTable = off_820AA810; // reinstall the base PlugIn vtable
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Resample::VectorDeletingDestructor(Resample *self, char flags)
{
    self->mBase.mpVTable = KRS_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
