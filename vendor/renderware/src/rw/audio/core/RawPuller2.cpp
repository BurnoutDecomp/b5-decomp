// =====================================================================================
// rw::audio::core::RawPuller2 -- the "raw pull" source plug-in (PCM-from-callback into
// the audio output ring) with a deferred Play/Stop command path.
//
// The vendor rawpuller2.h supplies the planar pull callback and command field types.
// ARTIST supplies the command widths, output-buffer swap and per-channel copy.
//
// The RawPuller2 instance fields are modelled by name in RawPuller2.h. The System command
// ring (base @ +0x20, byte cursor @ +0x10B8) is reached through the committed System home
// in PlugIn.h; the per-Process node the asm walks at byte offsets 0x3000C..0x3002C is the
// shared rw::audio::core::AudioProcessContext (homed in Iir2Filters.h), so it is addressed
// by NAMED MEMBER -- Send::Process reads mpSrcBuffer at the same +0x3000C, and Rechannel /
// SinePlayer / the Iir2 shapes read the same slots. Each store WIDTH below
// (stw / stfs / stb) is reproduced from the asm.
// =====================================================================================

#include "rw/audio/core/RawPuller2.h"
#include "rw/audio/core/Iir2Filters.h" // AudioProcessContext / AudioChannelBuffer homes

#include <xtl.h>

namespace rw
{
namespace audio
{
namespace core
{
// off_8217F4E4 -- the RawPuller2 vtable. Modelled as a single file-static sentinel slot;
// the instance only stores its address (CreateInstance) and never dispatches through it
// in this TU.
static void* skRawPuller2VTableSlot = 0;
static void* const skpRawPuller2VTable = &skRawPuller2VTableSlot;

// -------------------------------------------------------------------------------------
// CreateInstance @0x82B3798
//   if (self) *self = off_8217F4E4
//   *(self+0x24) = 0   (stw)   mpCallback   = NULL
//   *(self+0x30) = 0   (stw)   muFrameCount = 0
//   *(self+0x34) = 1   (stb)   mbRestart    = 1
//   return 1
// -------------------------------------------------------------------------------------
int RawPuller2::CreateInstance(RawPuller2* self)
{
    if (self)
    {
        RawPuller2* lpSelf = self;
        lpSelf->mpVTable     = skpRawPuller2VTable; // *self = off_8217F4E4
        lpSelf->mpPullCallback   = 0;                   // +0x24
        lpSelf->mSamplesRequested = 0;                   // +0x30
        lpSelf->mFormatChanged    = 1;                   // +0x34 (stb)
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B97348 -- li r3, 0x38 ; blr
// -------------------------------------------------------------------------------------
int RawPuller2::GetSize()
{
    // X360-LITERAL TRAP: the console immediate is this object's CONSOLE footprint, but
    // GetSize is the plug-in factory's allocation stride -- it allocates GetSize() bytes and
    // constructs the object into them. On the host the object is larger (widened vptr and
    // pointers), so returning the console value under-allocates and corrupts what follows.
    // Return host sizeof; the console immediate stays in the comment above.
    return static_cast<int>(sizeof(RawPuller2));   // X360: li r3, 0x38
}

// -------------------------------------------------------------------------------------
// EventEvent @0x82B9F358 -- queue a Play or Stop command into the owning System's
// deferred-command ring.
//   v3 = *(self+4)                          ; mpSystem
//   if (bStop)  { Stop:  cursor += 8 ;  cmd[0]=StopHandler ; cmd[1]=self }
//   else        { Play:  cursor += 24;  cmd[0]=PlayHandler ; cmd[1]=self ; cmd[2..5]=event }
// The asm `cmpwi r4,0 / bne` takes the Stop branch when a2 != 0.
// (cursor is the System byte cursor @ +0x10B8; ring base is @ +0x20.)
// -------------------------------------------------------------------------------------
RawPuller2* RawPuller2::EventEvent(RawPuller2* self, int bStop, const RawPuller2PlayEvent* pEvent)
{
    System* lpSystem = self->mpSystem;                                // v3 = *(self+4)
    char* lpRingBase = lpSystem->mpDeferredRingBase;                  // *(v3+0x20)

    if (bStop)
    {
        u32 luCursor = lpSystem->muDeferredRingCursor;            // *(v3+0x10B8)
        Command* lpCmd =
            reinterpret_cast<Command*>(lpRingBase + luCursor);
        // RECORD STRIDE (X360-literal trap): the asm's `addi r10,r10,8` IS the console
        // sizeof(Command) ({int(*)(Command*), RawPuller2*} = 4+4). On the host
        // the two widened pointers make the same record 16 bytes, and ExecuteCommands
        // advances the ring by the handler's return -- StopHandler below likewise returns
        // sizeof(Command) -- so the enqueue stride must be the HOST sizeof,
        // never the literal 8. The cursor is advanced before the fields are written,
        // exactly as the asm orders the stores.
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(Command)); // X360: addi r10,r10,8 @0x82B9F3C4
        lpCmd->handler = &RawPuller2::StopHandler;              // cmd[0] = StopHandler
        lpCmd->pObject = self;                                   // cmd[1] = self
    }
    else
    {
        u32 luCursor = lpSystem->muDeferredRingCursor;            // *(v3+0x10B8)
        RawPuller2PlayCommand* lpCmd =
            reinterpret_cast<RawPuller2PlayCommand*>(lpRingBase + luCursor);
        // RECORD STRIDE (X360-literal trap): the asm's `addi r9,r9,0x18` IS the console
        // sizeof(RawPuller2PlayCommand) ({int(*)(Command*), RawPuller2*, f32, f32, void*,
        // RawPuller2FillFn} = 4+4+4+4+4+4). On the host the four widened pointers make the
        // same record 40 bytes, and ExecuteCommands advances the ring by the handler's
        // return -- PlayHandler below likewise returns sizeof(RawPuller2PlayCommand) -- so
        // the enqueue stride must be the HOST sizeof, never the literal 24. The cursor is
        // advanced before the fields are written, exactly as the asm orders the stores.
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(RawPuller2PlayCommand)); // X360: addi r9,r9,0x18 @0x82B9F378
        lpCmd->handler = &RawPuller2::PlayHandler;              // cmd[0] = PlayHandler
        lpCmd->pObject = self;                                   // cmd[1] = self
        lpCmd->mfNumChannels = pEvent->mfNumChannels;             // cmd[2] = event[0]
        lpCmd->mfSampleRate = pEvent->mfSampleRate;               // cmd[3] = event[1]
        lpCmd->mpContext = pEvent->mpContext;                     // cmd[4] = event[2]
        lpCmd->mpCallback = pEvent->mpCallback;                   // cmd[5] = event[3]
    }
    return self;
}

// -------------------------------------------------------------------------------------
// PlayHandler @0x82B9A540 -- replay a queued Play command (a1 = the command record):
//   v1 = *(a1+4)                                       ; the RawPuller2 instance
//   if (*(v1+0x2C){f32} != *(a1+0xC){f32}  ||  *(v1+0x35){u8} != (u8)*(a1+8){f32->int})
//       *(v1+0x34) = 1                                 ; mbRestart = 1
//   *(v1+0x35) = (u8)*(a1+8)                           ; mbNumChannels (float truncated)
//   *(v1+0x2C) = *(a1+0xC)   (stfs)                    ; mfRate
//   *(v1+0x28) = *(a1+0x10)  (stw)                     ; mpContext
//   *(v1+0x24) = *(a1+0x14)  (stw)                     ; mpCallback
//   return 24
// The two comparands at a1+8 / a1+0xC are floats (lfs); a1+8 is truncated to a byte
// channel count via fctidz (round-toward-zero double->int, low byte taken). a1+0x10 /
// a1+0x14 are the context and callback the Play event carried.
// -------------------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
// The original rwAudio plug-in object emits these scalar loops in its size form.
#pragma optimize("s", on)
#endif
int RawPuller2::PlayHandler(Command * cmd)
{
    RawPuller2PlayCommand* lpCmd = static_cast<RawPuller2PlayCommand*>(cmd);
    RawPuller2* lpSelf = static_cast<RawPuller2 *>(lpCmd->pObject); // v1 = *(a1+4)

    if (lpSelf->mPlaySampleRate != lpCmd->mfSampleRate ||
        lpSelf->mPlayNumChannels != static_cast<u8>(lpCmd->mfNumChannels))
    {
        lpSelf->mFormatChanged = 1;
    }

    lpSelf->mPlayNumChannels = static_cast<u8>(lpCmd->mfNumChannels);
    lpSelf->mPlaySampleRate = lpCmd->mfSampleRate;
    lpSelf->mpContext = lpCmd->mpContext;
    lpSelf->mpPullCallback = lpCmd->mpCallback;
    // RECORD STRIDE (X360-literal trap): the asm's `li r3,0x18` is the console
    // sizeof(RawPuller2PlayCommand); it must stay identical to the producer's advance in
    // EventEvent above, so both sides use the HOST sizeof (40 on x64).
    return static_cast<int>(sizeof(RawPuller2PlayCommand)); // X360: li r3,0x18 @0x82B9A580
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif

// -------------------------------------------------------------------------------------
// StopHandler @0x82B9A5B0 -- replay a queued Stop command:
//   *(*(a1+4) + 0x24) = 0   ; the puller's mpCallback = NULL (idle)
//   return 8
// -------------------------------------------------------------------------------------
int RawPuller2::StopHandler(Command * cmd)
{
    Command* lpCmd = cmd;
    static_cast<RawPuller2 *>(lpCmd->pObject)->mpPullCallback = 0; // *(*(a1+4)+0x24) = 0
    // RECORD STRIDE (X360-literal trap): the asm's `li r3,8` is the console
    // sizeof(Command); it must stay identical to the producer's advance in
    // EventEvent above, so both sides use the HOST sizeof (16 on x64).
    return static_cast<int>(sizeof(Command)); // X360: li r3,8 @0x82B9A5B8
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9A5C8 -- *(self+0x30) = a4 ; return 0
//   (stows the requested per-pull frame count into muFrameCount.)
// -------------------------------------------------------------------------------------
int RawPuller2::PreProcess(PlugIn* plugIn, Mixer*, bool, int outputSamples)
{
    reinterpret_cast<RawPuller2*>(plugIn)->mSamplesRequested = outputSamples;
    return 0;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9A5D8 -- pull a block from the callback into the output node (a2).
//
//   if (!mpCallback) return 0;
//   v4 = mbNumChannels;
//   if (mbRestart) {                            // re-prime path
//       *(a2+0x3002C){u8}  = mbNumChannels;
//       *(a2+0x30020){u32} = 0;
//       *(a2+0x30024){f32} = mfRate;
//       mbRestart = 0;
//       return 1;
//   }
//   // normal path: swap the two output cursors, set up the request, run the callback
//   f32 v6 = *(a2+0x30028){f32};
//   u32 v7 = *(a2+0x30010){u32};               // 196624
//   u32 v8 = *(a2+0x3000C){u32};               // 196620
//   *(a2+0x3002C){u8}  = mbNumChannels;
//   *(a2+0x3000C){u32} = v7;                    // swap 0x3000C <- 0x30010
//   *(a2+0x30010){u32} = v8;                    // swap 0x30010 <- 0x3000C
//   *(a2+0x30020){u32} = muFrameCount;
//   *(a2+0x30024){f32} = mfRate;
//   if (mpCallback(muFrameCount, scratch, v6)) {
//       outBuf = *(a2+0x3000C);                 // the (swapped) destination buffer node
//       for (i = 0; i < mbNumChannels; ++i)
//           XMemCpy(outBuf->base + 4*outBuf->stride16*i,  scratch + 4*muFrameCount*i,  4*muFrameCount);
//       return 1;
//   }
//   return 0;
//
// The node the asm reaches at a2+0x3000C..0x3002C is the shared AudioProcessContext and the
// per-channel buffer node is AudioChannelBuffer (base @+0x04, halfword stride @+0x0E), both
// homed in Iir2Filters.h -- so both are addressed by named member rather than by the console
// byte offsets. THIS IS LOAD-BEARING, not cosmetic: on the host the widened pointers push
// mpSrcBuffer/mpDstBuffer past their console offsets (mdStreamTime's 8-byte alignment alone
// moves mpSrcBuffer from +0x3000C to +0x30010), and the earlier raw-offset form additionally
// read/rewrote those two buffer POINTERS as u32, truncating them on x64.
// `scratch` is the 8327A600 mix-scratch staging buffer (file-static here). XMemCpy == memcpy.
// -------------------------------------------------------------------------------------
namespace
{
    // The X360 unk_8327A600 staging buffer the puller fills then scatters per channel.
    // The callback writes planar float samples before the per-channel scatter.
    float skScratchBuffer[64 * 1024 / sizeof(float)];
}

#if defined(_XBOX) && defined(_MSC_VER)
// The original rwAudio plug-in object emits these scalar loops in its size form.
#pragma optimize("s", on)
#endif
BufferStatus RawPuller2::Process(PlugIn* plugIn, Mixer* mixer, bool)
{
    RawPuller2* self = reinterpret_cast<RawPuller2*>(plugIn);
    if (!self->mpPullCallback)
        return BUFFERSTATUS_UNAVAILABLE;

    const unsigned channels = self->mPlayNumChannels;
    if (self->mFormatChanged)
    {
        mixer->mbChannelCount = static_cast<u8>(channels);
        mixer->mNumSamples = 0;
        mixer->mfSampleRate = self->mPlaySampleRate;
        self->mFormatChanged = 0;
    }
    else
    {
        const float pitch = mixer->mfResampleGain;
        mixer->mbChannelCount = static_cast<u8>(channels);
        mixer->SwapBuffers();
        mixer->mNumSamples = self->mSamplesRequested;
        mixer->mfSampleRate = self->mPlaySampleRate;
        if (!self->mpPullCallback(self->mSamplesRequested, skScratchBuffer, pitch, self->mpContext))
            return BUFFERSTATUS_UNAVAILABLE;

        SampleBuffer* output = mixer->mpSrcBuffer;
        for (unsigned channel = 0; channel < channels; ++channel)
        {
            const int samples = self->mSamplesRequested;
            XMemCpy(output->LockChannel(channel), skScratchBuffer + samples * channel,
                        samples * sizeof(float));
        }
    }
    return BUFFERSTATUS_AVAILABLE;
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif

} // namespace core
} // namespace audio
} // namespace rw
