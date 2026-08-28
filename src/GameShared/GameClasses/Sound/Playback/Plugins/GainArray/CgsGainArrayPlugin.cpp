// =====================================================================================
// CgsSound::Playback::Plugins::GainArray bodies -- the game-side per-channel gain-array
// plug-in ("JGA0").
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// store/branch. Decode report progress/scratch_dossiers/streammod_gainarray_decode_codex.md
// (TARGET 2). See CgsGainArrayPlugin.h for the layout.
//   GetSize        @0x82689E08 -- console 0x70; host sizeof (the stage carve)
//   CreateInstance @0x826C3A10 -- store-for-store
//   Process        @0x8268CDB0 -- store-for-store / branch-for-branch
//   `scalar deleting destructor' @0x826AA9E0
//
// OFFSET-HACK RETIREMENT (phase E 2026-08-28). The previous revision of this TU carried a
// TU-local reconstruction from the era before the engine types existed: a hand-rolled
// `struct GainArray` with word-indexed padding members, a forward-modelled local
// `MixBuffer`, and -- the load-bearing one -- Process reaching "an opaque content blob" by
// the raw byte offsets +0x3000C / +0x30010. Those are not an opaque blob: the callback's
// r4 IS the rw::audio::core::Mixer, and those two offsets are its mpSrcBuffer / mpDstBuffer
// slots. Everything is now by name against the real PlugIn / Mixer / SampleBuffer types,
// exactly as the TU's own FLAG note asked for once those homes landed.
// =====================================================================================

#include "GameShared/GameClasses/Sound/Playback/Plugins/GainArray/CgsGainArrayPlugin.h"

#include "rw/audio/core/Mixer.h"      // Mixer (the process context) + SampleBuffer
#include "rw/audio/core/MixKernels.h" // CopyWithGainRamp

namespace CgsSound
{
namespace Playback
{
namespace Plugins
{

using rw::audio::core::Mixer;
using rw::audio::core::SampleBuffer;
using rw::audio::core::PlugInDescRunTime;

// Fixed per-block ramp-step scale (flt_820ADC00, f31 in Process). RECOVERED 2026-08-25
// from the decrypted XEX rodata (file_off 0x3000 + 0xADC00: BE bytes 3C 80 00 00) ==
// 0.015625f == 1/64 -- the per-sample ramp fraction, i.e. GAIN_DECLICK_FRAME_SIZE. (An
// earlier revision silently stubbed this to 0.0f, which made every gain ramp a no-op.)
static const f32 KF_GAIN_RAMP_STEP = 0.015625f;   // flt_820ADC00 (1/64)
static const f32 KF_UNITY = 1.0f;                 // flt_82001C98
enum { KI_FRAME_SAMPLES = 256 };                  // MIXER_FRAME_SIZE (r7 at every kernel call)

// off_82F2E664 -- the "GainArray" runtime descriptor, REAL. Its 52 bytes were recovered
// from the XEX by the descriptor wave (progress/scratch_dossiers/plugindesc_layout_codex.md)
// and re-read by the GainArray decode: 'JGA0', type 4, 0 constructor params, 6 attributes,
// 0 events. Metadata FLAG'd null per the descriptor-wave convention.
static PlugInDescRunTime g_GainArrayDesc = {
    "GainArray",
    reinterpret_cast<void *>(&GainArray::GetSize),        // @0x82689E08
    reinterpret_cast<void *>(&GainArray::CreateInstance), // @0x826C3A10
    0,
    reinterpret_cast<void *>(&GainArray::Process),        // @0x8268CDB0
    0, 0, 0, 0,
    0,
    0x4A474130u,       // 'JGA0'
    4, 0, 6, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime -- return &off_82F2E664. (The console has no standalone getter for
// the three custom descriptors: GenericRwacFactory::GenericRwacFactory references their
// record addresses directly at 0x826C19A4..0x826C19B0. This accessor is the host's
// equivalent, matching the shape every stock plug-in uses.)
// -------------------------------------------------------------------------------------
char **GainArray::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_GainArrayDesc);
}

// -------------------------------------------------------------------------------------
// GetSize @0x82689E08 -- li r3, 0x70 ; blr
// -------------------------------------------------------------------------------------
int GainArray::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit): the console immediate under-allocates the
    // widened host object -- GetSize is the stage factory's allocation stride, so return
    // host sizeof.
    return static_cast<int>(sizeof(GainArray));   // X360: li r3, 0x70 (112)
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x826C3A10 -- point the base attribute table at the six target gains and
// open every channel at unity, current AND target, so the first frame ramps nowhere.
//
// The console's null test guards only its vtable store (which on the host is construction);
// every following access dereferences self, so null is not a supported input.
// -------------------------------------------------------------------------------------
int GainArray::CreateInstance(GainArray *self)
{
    // stw r9(self+0x28), 0xC(r3) -- by name, never a truncated console address.
    self->mpAttribute = self->maTargetGain;

    // One loop, two strides: the targets step by 8 (the Attribute_t stride) and the current
    // gains by 4, six iterations.
    for (int liChannel = 0; liChannel < KI_CHANNELS; ++liChannel)
    {
        self->maCurrentGain[liChannel] = KF_UNITY;             // stfs 0(r10), r10 += 4
        self->maTargetGain[liChannel].mfValue = KF_UNITY;      // stfs 0(r9),  r9  += 8
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// Process @0x8268CDB0 -- ramp every channel from its current gain toward its target.
//
// r5 is the first-pass flag: on it the current gain SNAPS to the target before the ramp is
// computed, so the block plays at the target level with a zero step instead of sliding in
// from a stale value. Each channel is copied src -> dst through the ramping kernel, and the
// slots are ping-ponged at the end.
// -------------------------------------------------------------------------------------
int GainArray::Process(GainArray *self, Mixer *ctx, bool firstPass)
{
    SampleBuffer *lpSrc = ctx->mpSrcBuffer;   // r28 (ctx+0x3000C -- NOT an opaque blob)
    SampleBuffer *lpDst = ctx->mpDstBuffer;   // r27 (ctx+0x30010)

    const u32 luCount = self->mOutputChannels;   // lbz 0x21(r3)
    for (u32 luChannel = 0; luChannel < luCount; ++luChannel)
    {
        f32 &lrTarget = self->maTargetGain[luChannel].mfValue;
        f32 &lrCurrent = self->maCurrentGain[luChannel];

        if (firstPass)
            lrCurrent = lrTarget;

        const f32 lfStep = (lrTarget - lrCurrent) * KF_GAIN_RAMP_STEP;

        rw::audio::core::CopyWithGainRamp(
            lpDst->mpSamples + lpDst->muStride * luChannel,   // r3 = destination
            lpSrc->mpSamples + lpSrc->muStride * luChannel,   // r4 = source
            lrCurrent,                                         // f1 = start gain
            lfStep,                                            // f2 = per-block step
            KI_FRAME_SAMPLES);                                 // r7 = 256

        // The ramp has landed; the target becomes the new current level.
        lrCurrent = lrTarget;
    }

    // Ping-pong the slots so the freshly written destination is the published source.
    SampleBuffer *lpTemp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = lpTemp;
    return 1;
}

} // namespace Plugins
} // namespace Playback
} // namespace CgsSound
