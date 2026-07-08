// =====================================================================================
// rw::audio::core::Rechannel bodies -- the channel-remap audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 PDB entry exist for this type.
//   CreateInstance         @0x82BA37D0 -- installs the derived vtable
//   GetPlugInDescRunTime   @0x82B9A718 -- returns the registered "Rechannel" descriptor
//   GetSize                @0x82B982E0 -- li r3, 0x24 ; blr
//   PreProcess             @0x82B97F98 -- mr r3, r6 ; blr (pass the output sample count through)
//   Process                @0x82B9A728 -- branch-for-branch channel-count remap
//   `scalar deleting destructor' @0x82BA1C20 -- reinstalls base vtable, conditional free
// See Rechannel.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/Rechannel.h"
#include "rw/audio/core/MixKernels.h" // ReChannelGainWrite

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F4F4 -- the Rechannel (derived) vtable installed by CreateInstance. off_820AA810
// -- the PlugIn base vtable the scalar-deleting destructor reverts to during teardown.
// off_82F8F884 -- the registered run-time descriptor record (its +0x00 label is the string
// "Rechannel"). These are opaque data symbols in the XEX (no exported contents); modelled
// as honest placeholder storage so the bodies below link without fabricating their
// contents -- the same idiom Gain.cpp / Send.cpp use.
static void *const KRC_RechannelVTable = nullptr;   // off_8217F4F4
static void *const KRC_BasePlugInVTable = nullptr;  // off_820AA810
static char       *g_RechannelDesc = const_cast<char *>("Rechannel"); // off_82F8F884 (label "Rechannel")

// The whole instance is the embedded PlugIn base view (X360 GetSize() == 36 == 0x24; the
// host sizeof is larger because the base view's pointers widen to 8 bytes -- see Send.h's
// same note, so no host sizeof pin is asserted). Six is the rwaudio MAX_CHANNELS; the
// source pointer array is clamped to it (see Process).
enum { KI_RECHANNEL_MAX_CHANNELS = 6 };

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA37D0 -- install the derived vtable over `self` (no other init).
//   if (self) self->mBase.mpVTable = off_8217F4F4;
//   return 1;
// -------------------------------------------------------------------------------------
int Rechannel::CreateInstance(Rechannel *self)
{
    if (self)
        self->mBase.mpVTable = KRC_RechannelVTable; // off_8217F4F4
    return 1;
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9A718 -- return the address of the registered descriptor
// record (its label is the string "Rechannel").
// -------------------------------------------------------------------------------------
char **Rechannel::GetPlugInDescRunTime()
{
    return &g_RechannelDesc; // &off_82F8F884
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B982E0 -- li r3, 0x24 ; blr
// -------------------------------------------------------------------------------------
int Rechannel::GetSize()
{
    return 36;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B97F98 -- mr r3, r6 ; blr. Pass the requested output sample count (a4/r6)
// straight through: a channel remap neither adds nor drops samples.
// -------------------------------------------------------------------------------------
int Rechannel::PreProcess(Rechannel * /*self*/, AudioProcessContext * /*ctx*/, int /*a3*/,
                          int outputSamples)
{
    return outputSamples;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9A728 -- force the input frame to the plug-in's output channel count.
//   n      = ctx->mNumSamples(+0x30020)                                  ; lwzx (frame samples)
//   inCh   = ctx->mbChannelCount(+0x3002C)                              ; lbz  (incoming count)
//   if (!n) self->mBase.mbFlag20(+0x20) = inCh                          ; latch while idle
//   outCh  = self->mBase.mbChannelCount(+0x21)                          ; lbz  (target count)
//   if (inCh != outCh) {
//       if (n) {
//           src = ctx->mpSrcBuffer(+0x3000C) ; dst = ctx->mpDstBuffer(+0x30010)
//           srcCnt = min(inCh, 6)
//           for (c<srcCnt) srcPtr[c] = src->mpSamples + src->muStride*c   ; v20 (8 slots)
//           for (c<outCh)  dstPtr[c] = dst->mpSamples + dst->muStride*c   ; v21 (20 slots)
//           ReChannelGainWrite(dstPtr, srcPtr, numDst=outCh, numSrc=inCh, numSamples=n, 1.0)
//       }
//       swap(ctx->mpSrcBuffer, ctx->mpDstBuffer)                         ; ping-pong
//       ctx->mbChannelCount(+0x3002C) = self->mBase.mbChannelCount(+0x21)
//   }
//   return 1;
// The source-pointer loop count is clamped to 6 (MAX_CHANNELS) while the numSrc argument
// passed to the kernel is the raw incoming count -- exactly as the asm builds v20 with the
// clamped bound (v9) but passes the unclamped byte (v6). The swap + channel-count write run
// whenever the count changed, even for a silent (n == 0) frame.
// -------------------------------------------------------------------------------------
int Rechannel::Process(Rechannel *self, AudioProcessContext *ctx)
{
    const u32 luNumSamples = ctx->mNumSamples;          // lwzx +0x30020
    const u8  luIncomingChannels = ctx->mbChannelCount; // lbz  +0x3002C

    // A silent frame carries no samples to remap; just track the incoming channel count.
    if (!luNumSamples)
        self->mBase.mbFlag20 = luIncomingChannels;      // stb +0x20

    const u8 luOutputChannels = self->mBase.mbChannelCount; // lbz +0x21
    if (luIncomingChannels != luOutputChannels)
    {
        if (luNumSamples)
        {
            AudioChannelBuffer *lpSrcBuffer = ctx->mpSrcBuffer; // *(ctx+0x3000C) = v8
            AudioChannelBuffer *lpDstBuffer = ctx->mpDstBuffer; // *(ctx+0x30010) = v10

            f32 *lpSrcChannel[8];  // v20 -- per-source-channel sample pointers
            f32 *lpDstChannel[20]; // v21 -- per-destination-channel sample pointers

            // Source pointers: incoming channel count, clamped to MAX_CHANNELS (6).
            u32 luSrcCount = luIncomingChannels;
            if (luSrcCount > KI_RECHANNEL_MAX_CHANNELS)
                luSrcCount = KI_RECHANNEL_MAX_CHANNELS;
            for (u32 lc = 0; lc < luSrcCount; ++lc)
                lpSrcChannel[lc] = lpSrcBuffer->mpSamples + lpSrcBuffer->muStride * lc;

            // Destination pointers: the plug-in's output channel count.
            for (u32 lc = 0; lc < luOutputChannels; ++lc)
                lpDstChannel[lc] = lpDstBuffer->mpSamples + lpDstBuffer->muStride * lc;

            // numDst = output channels, numSrc = incoming channels (unclamped), gain = 1.0.
            ReChannelGainWrite(lpDstChannel, lpSrcChannel, luOutputChannels,
                               luIncomingChannels, static_cast<s32>(luNumSamples), 1.0f);
        }

        // Ping-pong the src/dst channel-buffer slots so the remapped buffer becomes active,
        // then record the plug-in's output channel count as the context's new active count.
        AudioChannelBuffer *lpSwapTmp = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = ctx->mpDstBuffer;
        ctx->mpDstBuffer = lpSwapTmp;
        ctx->mbChannelCount = self->mBase.mbChannelCount; // stb self+0x21 -> ctx+0x3002C
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA1C20
//   self->mBase.mpVTable = off_820AA810; // reinstall the base PlugIn vtable
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Rechannel::ScalarDeletingDestructor(Rechannel *self, char flags)
{
    self->mBase.mpVTable = KRC_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
