// =====================================================================================
// rw::audio::core::Gain bodies -- the per-voice volume audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 PDB entry exist for this type.
//   CreateInstance         @0x82BA2BD0 -- store-for-store
//   GetPlugInDescRunTime   @0x82B97350 -- returns the registered "Gain" descriptor
//   Process                @0x82B97600 -- branch-for-branch de-clicked gain ramp
//   `vector deleting destructor' @0x82BA1710 -- reinstalls base vtable, conditional free
// See Gain.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Gain.h"
#include "rw/audio/core/MixKernels.h" // CopyWithGainRamp

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F3D4 -- the Gain v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the deleting destructor reinstalls before any free. off_82F8CB70 -- the
// registered run-time descriptor record (its +0x00 label is the string "Gain"). These are
// opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
static void *const KGN_GainVTable = nullptr;    // off_8217F3D4
static void *const KGN_BasePlugInVTable = nullptr; // off_820AA810
static char       *g_GainDesc = nullptr;        // off_82F8CB70 (the "Gain" record)

// The full mixer frame is processed per Process call (li r7, 0x100 == MIXER_FRAME_SIZE),
// and the de-click ramp spans GAIN_DECLICK_FRAME_SIZE (64) samples, so the per-sample step
// scale is 1/64 == 0.015625 (flt_820ADC00).
enum { KI_MIXER_FRAME_SIZE = 256 };
static const f32 KF_GAIN_DECLICK_STEP = 0.015625f; // 1 / GAIN_DECLICK_FRAME_SIZE

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B97350 -- return the address of the registered descriptor
// record (its label is the string "Gain").
// -------------------------------------------------------------------------------------
char **Gain::GetPlugInDescRunTime()
{
    return &g_GainDesc; // &off_82F8CB70
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA2BD0 -- placement-init a Gain over `self`.
//   if (self) self->mBase.mpVTable = off_8217F3D4;
//   self->mfCurrentGain(+0x30) = 1.0;
//   self->mBase.mpAttributes(+0x0C) = &self->mfGain(+0x28);
//   self->mfGain(+0x28) = 1.0;
//   return 1;
// -------------------------------------------------------------------------------------
int Gain::CreateInstance(Gain *self)
{
    if (self)
        self->mBase.mpVTable = KGN_GainVTable; // off_8217F3D4

    self->mfCurrentGain = 1.0f;                // flt_82001C98 @ +0x30
    // Point the base attribute-table slot at the live gain attribute (self+0x28) so the
    // base SetAttribute writes land on mfGain.
    self->mBase.mpAttributes = &self->mfGain;  // @ +0x0C
    self->mfGain = 1.0f;                        // flt_82001C98 @ +0x28
    return 1;
}

// -------------------------------------------------------------------------------------
// Process @0x82B97600 -- apply the scalar gain to every channel of the input frame, ramping
// from mfCurrentGain toward mfGain over the 64-sample de-click window, then ping-pong the
// context's src/dst channel-buffer slots and latch the current gain to the target.
//   src = ctx->mpSrcBuffer(+0x3000C);  dst = ctx->mpDstBuffer(+0x30010);
//   if (resetRamp) mfCurrentGain = mfGain;   // restart the ramp at the target
//   step = (mfGain - mfCurrentGain) * (1/64);
//   for (c = 0; c < mBase.mbChannelCount; ++c)
//       CopyWithGainRamp(dst->mpSamples + dst->muStride*c,
//                        src->mpSamples + src->muStride*c, mfCurrentGain, step, 256);
//   swap(ctx->mpSrcBuffer, ctx->mpDstBuffer);
//   mfCurrentGain = mfGain;
//   return 1;
// -------------------------------------------------------------------------------------
int Gain::Process(Gain *self, AudioProcessContext *ctx, char resetRamp)
{
    AudioChannelBuffer *srcBuffer = ctx->mpSrcBuffer; // *(a2+0x3000C) = v6
    AudioChannelBuffer *dstBuffer = ctx->mpDstBuffer; // *(a2+0x30010) = v7

    if (resetRamp)
        self->mfCurrentGain = self->mfGain;

    const f32 step = (self->mfGain - self->mfCurrentGain) * KF_GAIN_DECLICK_STEP;

    const u32 channelCount = self->mBase.mbChannelCount; // lbz +0x21
    for (u32 channel = 0; channel < channelCount; ++channel)
    {
        f32 *pDst = dstBuffer->mpSamples + dstBuffer->muStride * channel;
        f32 *pSrc = srcBuffer->mpSamples + srcBuffer->muStride * channel;
        CopyWithGainRamp(pDst, pSrc, self->mfCurrentGain, step, KI_MIXER_FRAME_SIZE);
    }

    // Ping-pong the src/dst channel-buffer slots for the next stage.
    AudioChannelBuffer *swapTmp = ctx->mpSrcBuffer;
    ctx->mpSrcBuffer = ctx->mpDstBuffer;
    ctx->mpDstBuffer = swapTmp;

    self->mfCurrentGain = self->mfGain;
    return 1;
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82BA1710
//   self->mBase.mpVTable = off_820AA810; // reinstall the base PlugIn vtable
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Gain::VectorDeletingDestructor(Gain *self, char flags)
{
    self->mBase.mpVTable = KGN_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
