// =====================================================================================
// rw::audio::core::Route -- member bodies reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC). The asm is authoritative for every store; see Route.h for the
// reconstructed layout. No reference source and no DecFIGS DWARF exist for this type.
//
// Route is a PlugIn-family node that attaches a source into a SubMix through an embedded
// SubMixConnector subobject (this+0x24), with a deferred connect/release path through the
// owning System's command ring (base @ +0x20, byte cursor @ +0x10B8). Each store WIDTH
// below (stw / stfs / stb) is reproduced from the asm.
// =====================================================================================

#include "rw/audio/core/Route.h"
#include "rw/audio/core/MixKernels.h" // MixWithGain
#include "rw/audio/core/SubMix.h" // SubMix (the real class; moved out of SubMixConnector.h)

namespace rw
{
namespace audio
{
namespace core
{
// off_8217F524 -- the Route (derived) vtable. Modelled as a single file-static sentinel
// slot; CreateInstance stores its address and this TU never dispatches through it.
static void* skRouteVTableSlot = 0;
static void* const skpRouteVTable = &skRouteVTableSlot;

// off_820AA810 -- the PlugIn base vtable the scalar-deleting destructor reverts to during
// teardown. Modelled as a separate file-static sentinel slot.
// off_820AA810 is the SHARED gpBasePlugInVTableSentinel (PlugIn.h; wave 5 --
// the file-local slot this TU used to hold gave the same console object a
// second host address vs Send.cpp).
static void* const skpRouteBaseVTable = gpBasePlugInVTableSentinel;

// PlugIn::CreateInstance dispatches every descriptor through the common
// int(PlugIn*, void*) ABI. Route's console constructor ignores the context word.
static int RouteCreateInstanceThunk(PlugIn *self, void *)
{
    return Route::CreateInstance(reinterpret_cast<Route *>(self));
}

// off_82F8FBC8 -- the complete mutable Route descriptor. RegisterPlugInRunTime threads
// the record through mpNext and writes mbSeq, so this must be real writable descriptor
// storage rather than the former single read-only name pointer.
static PlugInDescRunTime g_RouteDesc = {
    "Route",
    reinterpret_cast<void *>(&Route::GetSize),             // @0x82B982F8
    reinterpret_cast<void *>(&RouteCreateInstanceThunk),   // @0x82BA3D40
    0,
    reinterpret_cast<void *>(&Route::Process),             // @0x82B9B268
    0, 0, 0, 0,
    0,
    0x526F7530u,       // 'Rou0'
    4, 0, 0, 1, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA3D40
//   if (a1) { *a1 = off_8217F524 ; *(a1+0x2C)=0 ; *(a1+0x30)=0 ; *(a1+0x34)=0 (stb) }
//   zero 6 dwords at a1+0x38 (the foldback gain array)  -- always, even when a1 == 0
//   return 1
// The +0x2C/+0x30/+0x34 stores clear the embedded connector's mField08 / mpSubMix /
// mbField10; the +0x38 loop clears maChannelGain[0..5].
// -------------------------------------------------------------------------------------
int Route::CreateInstance(Route *self)
{
    if (self)
    {
        self->mBase.mpVTable = skpRouteVTable;                 // *self = off_8217F524
        self->mSubMixConnector.mpSubMixBuffer = 0;             // stw 0, 0x2C(a1)
        self->mSubMixConnector.mpSubMix = 0;                   // stw 0, 0x30(a1)
        self->mSubMixConnector.mNumSubMixChannels = 0;         // stb 0, 0x34(a1)

        // The X360 zeroes the 6-dword gain array (a1+0x38) AFTER the beq, i.e.
        // unconditionally; reproducing that literally through a null a1 would be a
        // host null-store. The only real caller always passes a valid buffer, so the
        // (semantically identical) guarded form is used.
        for (int li = 0; li < 6; ++li)
            self->mDeClickValue[li] = 0.0f;                    // stw 0 -> +0x38 + 4*i
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B982F8 -- li r3, 0x54 ; blr
// -------------------------------------------------------------------------------------
int Route::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit, phase-D follow-up): the console
    // immediate under-allocates the widened host object -- GetSize is the stage
    // factory's allocation stride, so return host sizeof (the RawPuller2/Send/
    // SinePlayer precedent).
    return static_cast<int>(sizeof(Route));   // X360: li r3, 0x54 (84)
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9B258 -- lis/addi off_82F8FBC8 ; blr  (returns &descriptor)
// -------------------------------------------------------------------------------------
char** Route::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_RouteDesc);
}

// -------------------------------------------------------------------------------------
// Process @0x82B9B268 -- route selected source channels into the attached SubMix.
//   if (!connector.mNumSubMixChannels) return 1
//   dst = connector.mpSubMixBuffer + targetStart * 256
//   for (i = 0; i < mNumChannels; ++i) {
//       src = ctx->mpSrcBuffer->mpSamples
//           + (sourceStart + i) * ctx->mpSrcBuffer->muStride
//       MixWithGain(dst, src, 1.0f, 256)
//       mDeClickValue[targetStart + i] = src[255]
//       dst += 256
//   }
// -------------------------------------------------------------------------------------
int Route::Process(Route *self, AudioProcessContext *ctx, char /*resetRamp*/)
{
    if (!self->mSubMixConnector.mNumSubMixChannels)
        return 1;

    f32 *lpDestination = self->mSubMixConnector.mpSubMixBuffer
        + static_cast<u32>(self->mTargetStartChannel) * Mixer::KU_FRAME_SIZE;
    AudioChannelBuffer *lpSourceBuffer = ctx->mpSrcBuffer;

    for (u32 luChannel = 0; luChannel < self->mNumChannels; ++luChannel)
    {
        const u32 luSourceChannel = static_cast<u32>(self->mSourceStartChannel) + luChannel;
        const f32 *lpSource = lpSourceBuffer->mpSamples
            + static_cast<u32>(lpSourceBuffer->muStride) * luSourceChannel;

        MixWithGain(lpDestination, lpSource, 1.0f, Mixer::KU_FRAME_SIZE);
        self->mDeClickValue[static_cast<u32>(self->mTargetStartChannel) + luChannel]
            = lpSource[Mixer::KU_FRAME_SIZE - 1];
        lpDestination += Mixer::KU_FRAME_SIZE;
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// EventEvent @0x82BA3DC8 -- queue a connect command (ConnectByPointerHandler) into the
// owning System's deferred-command ring (24 bytes / 6 words).
//   v3 = *(result+4)                          ; mpSystem
//   cursor = *(v3+0x10B8) ; slot = *(v3+0x20) + cursor ; *(v3+0x10B8) = cursor + 24
//   slot[0] = ConnectByPointerHandler ; slot[1] = result
//   slot[2..5] = a3[0..3]                      ; the 4-word connect event payload
// -------------------------------------------------------------------------------------
Route* Route::EventEvent(Route* self, int /*a2*/, const RouteConnectEvent* pEvent)
{
    System* lpSystem = static_cast<System *>(self->mBase.mpSystem); // v3 = *(result+4)
    char* lpRingBase = lpSystem->mpDeferredRingBase;               // *(v3+0x20)

    u32 luCursor = lpSystem->muDeferredRingCursor;                 // *(v3+0x10B8)
    RouteConnectCommand* lpCmd =
        reinterpret_cast<RouteConnectCommand*>(lpRingBase + luCursor);
    // RECORD STRIDE (X360-literal trap): the asm's `addi r9,r9,0x18` IS the console
    // sizeof(RouteConnectCommand) ({int(*)(Command*), Route*, SubMix*, f32 x3} = 4+4+4+4+4+4).
    // On the host the three widened pointers make the same record 40 bytes, and
    // ExecuteCommands advances the ring by the handler's return -- ConnectByPointerHandler
    // below likewise returns sizeof(RouteConnectCommand) -- so the enqueue stride must be
    // the HOST sizeof, never the literal 24. The cursor is advanced before the fields are
    // written, exactly as the asm orders the stores.
    lpSystem->muDeferredRingCursor =
        luCursor + static_cast<u32>(sizeof(RouteConnectCommand)); // X360: addi r9,r9,0x18 @0x82BA3DE0

    lpCmd->handler = &Route::ConnectByPointerHandler; // cmd[0]
    lpCmd->pObject = self;                             // cmd[1]
    lpCmd->mpSubMix = pEvent->mpSubMix;                 // cmd[2] = event word 0
    lpCmd->mfGain0 = pEvent->mfGain0;                   // cmd[3] = event word 1
    lpCmd->mfGain1 = pEvent->mfGain1;                   // cmd[4] = event word 2
    lpCmd->mfGain2 = pEvent->mfGain2;                   // cmd[5] = event word 3
    return self;
}

// ARTIST 0x82BA1D10. Restore the base vtable before optional deallocation.
void* Route::ScalarDeletingDestructor(void* lpInstance, char lcFlags)
{
    Route* lpRoute = static_cast<Route*>(lpInstance);
    lpRoute->mBase.mpVTable = skpRouteBaseVTable;
    if (lcFlags & 1)
        ::operator delete(lpRoute);
    return lpRoute;
}

} // namespace core
} // namespace audio
} // namespace rw
