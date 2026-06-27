// =====================================================================================
// rw::audio::core::RawPuller2 -- the "raw pull" source plug-in (PCM-from-callback into
// the audio output ring) with a deferred Play/Stop command path.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative. No reference source and no DecFIGS DWARF exist for this type.
//
// The RawPuller2 instance fields are modelled by name in RawPuller2.h. Two foreign
// aggregates this TU only touches through fixed byte offsets are kept opaque and
// addressed raw (the documented opaque-foreign-field precedent):
//   * the System command ring  (base @ +0x20, byte cursor @ +0x10B8)  -- EventEvent
//   * the per-Process output node (a2; fields at byte offsets 0x3000C..0x3002C) -- Process
// Each store WIDTH below (stw / stfs / stb / std) is reproduced from the asm.
// =====================================================================================

#include "rw/audio/core/RawPuller2.h"

#include <cstring> // memcpy (the X360 XMemCpy)

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
int RawPuller2::CreateInstance(int self)
{
    if (self)
    {
        RawPuller2* lpSelf = reinterpret_cast<RawPuller2*>(self);
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
    return 56;
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
int RawPuller2::EventEvent(int self, int bStop, u32* lpEvent)
{
    System* lpSystem = reinterpret_cast<RawPuller2*>(self)->mpSystem; // v3 = *(self+4)
    char* lpRingBase = lpSystem->mpDeferredRingBase;                  // *(v3+0x20)

    if (bStop)
    {
        u32 luCursor = lpSystem->muDeferredRingCursor;            // *(v3+0x10B8)
        u32* lpCmd = reinterpret_cast<u32*>(lpRingBase + luCursor);
        lpSystem->muDeferredRingCursor = luCursor + 8;            // advance by 8
        lpCmd[0] = reinterpret_cast<u32>(&RawPuller2::StopHandler); // cmd[0] = StopHandler
        lpCmd[1] = static_cast<u32>(self);                          // cmd[1] = self
    }
    else
    {
        u32 luCursor = lpSystem->muDeferredRingCursor;            // *(v3+0x10B8)
        u32* lpCmd = reinterpret_cast<u32*>(lpRingBase + luCursor);
        lpSystem->muDeferredRingCursor = luCursor + 24;          // advance by 24
        lpCmd[0] = reinterpret_cast<u32>(&RawPuller2::PlayHandler); // cmd[0] = PlayHandler
        lpCmd[1] = static_cast<u32>(self);                          // cmd[1] = self
        lpCmd[2] = lpEvent[0];                                       // cmd[2] = event[0]
        lpCmd[3] = lpEvent[1];                                       // cmd[3] = event[1]
        lpCmd[4] = lpEvent[2];                                       // cmd[4] = event[2]
        lpCmd[5] = lpEvent[3];                                       // cmd[5] = event[3]
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
int RawPuller2::PlayHandler(int lpCommand)
{
    RawPuller2PlayCommand* lpCmd = reinterpret_cast<RawPuller2PlayCommand*>(lpCommand);
    RawPuller2* lpSelf = reinterpret_cast<RawPuller2*>(lpCmd->mTarget); // v1 = *(a1+4)

    const f32 lfRate = *reinterpret_cast<const f32*>(&lpCmd->mPayload1);   // *(a1+0xC)
    const f32 lfChannelsF = *reinterpret_cast<const f32*>(&lpCmd->mPayload0); // *(a1+8)
    const char lbNumChannels = static_cast<char>(static_cast<long long>(lfChannelsF));

    if (lpSelf->mPlaySampleRate != lfRate || lpSelf->mPlayNumChannels != lbNumChannels)
        lpSelf->mFormatChanged = 1; // *(v1+0x34) = 1

    lpSelf->mPlayNumChannels = lbNumChannels;       // *(v1+0x35) (stb)
    lpSelf->mPlaySampleRate        = lfRate;              // *(v1+0x2C) (stfs)
    lpSelf->mpContext     = reinterpret_cast<void*>(lpCmd->mPayload2);          // *(v1+0x28)
    lpSelf->mpPullCallback    = reinterpret_cast<RawPuller2FillFn>(lpCmd->mPayload3); // *(v1+0x24)
    return 24;
}

// -------------------------------------------------------------------------------------
// StopHandler @0x82B9A5B0 -- replay a queued Stop command:
//   *(*(a1+4) + 0x24) = 0   ; the puller's mpCallback = NULL (idle)
//   return 8
// -------------------------------------------------------------------------------------
int RawPuller2::StopHandler(int lpCommand)
{
    RawPuller2PlayCommand* lpCmd = reinterpret_cast<RawPuller2PlayCommand*>(lpCommand);
    reinterpret_cast<RawPuller2*>(lpCmd->mTarget)->mpPullCallback = 0; // *(*(a1+4)+0x24) = 0
    return 8;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9A5C8 -- *(self+0x30) = a4 ; return 0
//   (stows the requested per-pull frame count into muFrameCount.)
// -------------------------------------------------------------------------------------
int RawPuller2::PreProcess(int self, int /*a2*/, int /*a3*/, int a4)
{
    reinterpret_cast<RawPuller2*>(self)->mSamplesRequested = static_cast<u32>(a4); // *(self+0x30)
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
// The output node (a2) and the per-channel destination buffer node are foreign rwaudio
// aggregates this TU only reads at the documented byte offsets, so they are addressed raw.
// `scratch` is the 8327A600 mix-scratch staging buffer (file-static here). XMemCpy == memcpy.
// -------------------------------------------------------------------------------------
namespace
{
    // The X360 unk_8327A600 staging buffer the puller fills then scatters per channel.
    // Sized to the engine's max pull block (frames * channels * 4); a generous fixed
    // staging window matching the asm's 4-byte-per-sample stride.
    char skScratchBuffer[64 * 1024];

    // Per-channel destination buffer node read out of the output graph: base ptr @ +4,
    // 16-bit channel stride @ +0xE (the asm `lwz r9,4(r28)` / `lhz r10,0xE(r28)`).
    struct OutputChannelNode
    {
        char* mpReserved0; // +0x00
        char* mpBase;      // +0x04 -- channel-0 sample base
        char  mGap08[0xE - 0x08];
        u16   mu16Stride;  // +0x0E -- inter-channel stride in samples
    };

    // Output-node field byte offsets (a2 + N).
    enum
    {
        KU_OUT_BUF_A   = 0x3000C, // 196620
        KU_OUT_BUF_B   = 0x30010, // 196624
        KU_OUT_REQUEST = 0x30020, // 196640 -- requested frame count
        KU_OUT_RATE    = 0x30024, // 196644 -- pull rate
        KU_OUT_PEEK     = 0x30028, // 196648 -- the f32 handed to the callback
        KU_OUT_CHANNELS = 0x3002C  // 196652 -- channel count (byte)
    };
}

int RawPuller2::Process(int self, int a2)
{
    RawPuller2* lpSelf = reinterpret_cast<RawPuller2*>(self);
    if (!lpSelf->mpPullCallback)
        return 0;

    char* lpOut = reinterpret_cast<char*>(a2);
    const char lbNumChannels = lpSelf->mPlayNumChannels; // v4 = *(self+0x35)

    if (lpSelf->mFormatChanged)
    {
        *reinterpret_cast<char*>(lpOut + KU_OUT_CHANNELS) = lbNumChannels; // stb
        *reinterpret_cast<u32*>(lpOut + KU_OUT_REQUEST) = 0;               // stw
        *reinterpret_cast<f32*>(lpOut + KU_OUT_RATE) = lpSelf->mPlaySampleRate;     // stfs
        lpSelf->mFormatChanged = 0;
        return 1;
    }

    const f32 lfPeek = *reinterpret_cast<f32*>(lpOut + KU_OUT_PEEK);       // v6
    const u32 luBufB = *reinterpret_cast<u32*>(lpOut + KU_OUT_BUF_B);      // v7 (0x30010)
    const u32 luBufA = *reinterpret_cast<u32*>(lpOut + KU_OUT_BUF_A);      // v8 (0x3000C)

    *reinterpret_cast<char*>(lpOut + KU_OUT_CHANNELS) = lbNumChannels;     // stb
    *reinterpret_cast<u32*>(lpOut + KU_OUT_BUF_A) = luBufB;                // swap
    *reinterpret_cast<u32*>(lpOut + KU_OUT_BUF_B) = luBufA;                // swap
    *reinterpret_cast<u32*>(lpOut + KU_OUT_REQUEST) = lpSelf->mSamplesRequested; // stw
    *reinterpret_cast<f32*>(lpOut + KU_OUT_RATE) = lpSelf->mPlaySampleRate;         // stfs

    // r3 = muFrameCount, r4 = scratch, r6 = mpContext, f1 = lfPeek (the X360 bctrl regs).
    if (lpSelf->mpPullCallback(lpSelf->mSamplesRequested, skScratchBuffer, lpSelf->mpContext, lfPeek))
    {
        const OutputChannelNode* lpNode =
            *reinterpret_cast<OutputChannelNode* const*>(lpOut + KU_OUT_BUF_A); // v9 = *v5
        const u32 luFrames = lpSelf->mSamplesRequested;
        for (u32 lu = 0; lu < static_cast<u32>(static_cast<unsigned char>(lbNumChannels)); ++lu)
        {
            char* lpDst = lpNode->mpBase + 4u * lpNode->mu16Stride * lu;
            const char* lpSrc = skScratchBuffer + 4u * luFrames * lu;
            std::memcpy(lpDst, lpSrc, 4u * luFrames);
        }
        return 1;
    }
    return 0;
}

} // namespace core
} // namespace audio
} // namespace rw
