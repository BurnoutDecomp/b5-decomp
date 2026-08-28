// =====================================================================================
// rw::audio::core::SubMix bodies -- the named mix-bus plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch (decode report
// progress/scratch_dossiers/submix_decode_codex.md). The vendor submix.h supplies the
// authoritative names and matches the ARTIST layout field for field.
//   GetPlugInDescRunTime  @0x82B9C370 -- returns the registered "SubMix" descriptor
//   GetSize               @0x82B982F0 -- console 0x90; host sizeof
//   CreateInstance        @0x82BA4680 -- store-for-store
//   CreateInstanceHandler @0x82B9C380 -- the deferred registry push
//   Process               @0x82B9C480 -- branch-for-branch
//   ReleaseEvent          @0x82BA0C18 -- drain / unlink / free (vt[0]; the destructor here)
// See SubMix.h for the layout.
//
// This TU REPLACES SubMix_statics.cpp, whose banner described itself as temporary link
// closure: the two statics it defined are defined here now, and the list is no longer
// permanently empty because the real create handler pushes into it.
// =====================================================================================

#include "rw/audio/core/SubMix.h"
#include "rw/audio/core/Mixer.h"      // the process context + SampleBuffer
#include "rw/audio/core/MixKernels.h" // DeClick

#include <cstddef> // offsetof (the node-to-owner conversion)
#include <cstring> // std::memcpy / std::memset (the X360 XMemCpy / XMemSet)

namespace rw
{
namespace audio
{
namespace core
{

// The two by-name registry statics (ARTIST off_8327EE68 / dword_8327EE00). Both live in
// BSS on the console and are therefore zero-initialised -- there are no raw bytes in the
// XEX to quote for them, which is why the decode marks their initial values as the static
// initialization contract rather than a recovered word.
ListDStack SubMix::sSubMixList = {};
ListDNode *SubMix::spSubMixNextNode = 0;

// The allocation tag string the console passes to System::Alloc (rodata @0x8217B098,
// re-read byte-for-byte from the XEX at file_off 0x17E098).
static const char KSZ_SUBMIX_BUFFER_TAG[] = "rw::audio::core::SubMix::mpSubMixBuffer";

enum { KI_FRAME_SAMPLES = 256 };   // MIXER_FRAME_SIZE
enum { KI_BUFFER_ALIGN = 128 };

// off_82F902E0 -- the "SubMix" runtime descriptor, REAL (its 52 bytes were re-read from
// the XEX by the decode: 'Sub0', type 4, ONE constructor parameter, 0 attributes,
// 0 events). Metadata FLAG'd null per the descriptor-wave convention.
//
// The create thunk mirrors the Dac precedent: the console's first store is the vtable
// install, which on the host IS the placement construction of the derived object over the
// generic stage memory. It must happen before CreateInstance's own stores.
static int SubMixCreateInstanceThunk(SubMix *self, void *apConstructorParams)
{
    ::new (static_cast<void *>(self)) SubMix;   // *a1 = off_8217F554
    return SubMix::CreateInstance(
        self, static_cast<const SubMix::ConstructorParams *>(apConstructorParams));
}

static PlugInDescRunTime g_SubMixDesc = {
    "SubMix",
    reinterpret_cast<void *>(&SubMix::GetSize),              // @0x82B982F0
    reinterpret_cast<void *>(&SubMixCreateInstanceThunk),    // @0x82BA4680
    0,
    reinterpret_cast<void *>(&SubMix::Process),              // @0x82B9C480
    0, 0, 0, 0,
    0,
    0x53756230u,       // 'Sub0'
    4, 1, 0, 0, 0, 0,
    0
};

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9C370 -- return &off_82F902E0.
// -------------------------------------------------------------------------------------
char **SubMix::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&g_SubMixDesc);
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B982F0 -- li r3, 0x90 ; blr
// -------------------------------------------------------------------------------------
int SubMix::GetSize()
{
    // X360-LITERAL TRAP (the stage-carve audit): the console immediate includes 4-byte
    // pointers and under-allocates the derived host object -- GetSize is the stage
    // factory's allocation stride, so return host sizeof.
    return static_cast<int>(sizeof(SubMix));   // X360: li r3, 0x90 (144)
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA4680 -- allocate the bus's accumulation buffer, copy its name, and
// queue the deferred registry push.
//
// The registry insert is DEFERRED rather than done here because the list is walked by the
// audio thread; the command ring is the engine's serialisation point.
// -------------------------------------------------------------------------------------
int SubMix::CreateInstance(SubMix *self, const ConstructorParams *params)
{
    // (The console's vtable store and the null-head store happened in the caller's
    // placement construction; see SubMixCreateInstanceThunk. The console guards ONLY those
    // two on a null self -- every following access dereferences it -- so null is not a
    // supported input either way.)
    self->mSendList.phead = 0;

    // Unconditional, and BEFORE the params test.
    self->mSubMixAdded = 0;                                   // stb +0x8C

    // ⚠️ The name copy is UNBOUNDED: the console copies bytes through the NUL into a
    // 64-byte buffer with no length check, so a name of 64 characters or more overflows.
    // Reproduced faithfully -- adding a bound here would be a silent behavioural
    // divergence, and the only caller supplies short literal bus names.
    if (params)
    {
        const char *lpSrc = params->pName;
        char *lpDst = self->mName;
        while ((*lpDst++ = *lpSrc++) != '\0')
            ;
    }
    else
    {
        self->mName[0] = '\0';                                // the null-params arm
    }

    // The bus buffer: 256 frames per channel. The console forms this with `rotlwi 10` on
    // the zero-extended channel byte, which IS channels * 1024 -- expressed here as the
    // frame maths it actually is, not as a rotate.
    const u32 luBytes = static_cast<u32>(self->mOutputChannels)
                      * KI_FRAME_SAMPLES * sizeof(f32);
    System *lpSystem = self->mpSystemUseGetSystemAccessor;
    void *lpBuffer = System::Alloc(lpSystem, luBytes, KSZ_SUBMIX_BUFFER_TAG,
                                   KI_BUFFER_ALIGN, 0);

    // The store happens on BOTH paths -- the console writes r3 to +0x24 before testing it,
    // so a failed allocation explicitly leaves mpSubMixBuffer null rather than stale.
    self->mpSubMixBuffer = static_cast<f32 *>(lpBuffer);       // stw +0x24
    if (!lpBuffer)
        return 0;

    std::memset(lpBuffer, 0, luBytes);                         // XMemSet

    // Queue the deferred registry push.
    const u32 luCursor = lpSystem->muDeferredRingCursor;       // lwz +0x10B8
    SubMixCreateCommand *lpCommand =
        reinterpret_cast<SubMixCreateCommand *>(lpSystem->mpDeferredRingBase + luCursor);
    // RECORD STRIDE (X360-literal trap): the console `cursor += 8` is ITS sizeof; the host
    // record is larger because both fields widen, and ExecuteCommands advances by the
    // handler's RETURN -- which returns the same host sizeof. No capacity check exists in
    // this body; faithfully reproduced.
    lpSystem->muDeferredRingCursor =
        luCursor + static_cast<u32>(sizeof(SubMixCreateCommand));
    lpCommand->mpHandler = &SubMix::CreateInstanceHandler;     // stw +0x00 (@0x82B9C380)
    lpCommand->mpTarget = self;                                // stw +0x04

    // These clears happen ONLY on the successful path (after the allocation test).
    self->mDeClickRequired = 0;                                // stb +0x8D
    for (int liChannel = 0; liChannel < KI_MAX_CHANNELS; ++liChannel)
        self->mDeClickValueTotal[liChannel] = 0.0f;            // six word zeros +0x34..+0x48

    return 1;
}

// -------------------------------------------------------------------------------------
// CreateInstanceHandler @0x82B9C380 -- the deferred replay: push this bus onto the head of
// the global by-name registry and mark it added.
//
// The return value is the RING-CURSOR ADVANCE, not a success code (System::ExecuteCommands
// advances the record pointer by whatever this returns).
// -------------------------------------------------------------------------------------
int SubMix::CreateInstanceHandler(void *apCommand)
{
    SubMixCreateCommand *lpCommand = static_cast<SubMixCreateCommand *>(apCommand);
    SubMix *lpSelf = lpCommand->mpTarget;

    ListDNode *lpNode = &lpSelf->mSubMixListNode;
    lpNode->pnext = sSubMixList.phead;          // stw head -> node+0x00
    lpNode->pprev = 0;                          // stw 0    -> node+0x04
    if (sSubMixList.phead)
        sSubMixList.phead->pprev = lpNode;      // repair the old head's back-link
    sSubMixList.phead = lpNode;                 // publish the new head
    lpSelf->mSubMixAdded = 1;                   // stb +0x8C

    // The console `li r3, 8` is its own record stride; producer and handler must agree, so
    // both use the HOST sizeof.
    return static_cast<int>(sizeof(SubMixCreateCommand));
}

// -------------------------------------------------------------------------------------
// EnumerateSubMixReset / EnumerateSubMix -- the by-name registry walk.
//
// Both were INLINED into Send::ConnectByNameHandler @0x82B9FF80 by the X360 build, so
// neither has a standalone ARTIST address; their bodies ARE the decoded inline ranges
// there (@0x82B9FFB4-B8 for the reset, @0x82B9FFFC-0x82BA0004 for the step). They are
// bodied here because the state they walk is SubMix-owned, and Send_wL_01.cpp keeps its
// own open-coded copy exactly where the console inlined it.
// -------------------------------------------------------------------------------------
void SubMix::EnumerateSubMixReset()
{
    spSubMixNextNode = sSubMixList.phead;
}

SubMix *SubMix::EnumerateSubMix()
{
    ListDNode *lpNode = spSubMixNextNode;
    if (!lpNode)
        return 0;
    // Node-to-owner. The console's literal -0x2C is offsetof on the host, never the
    // constant -- SubMix's leading members widen on LLP64.
    SubMix *lpSubMix = reinterpret_cast<SubMix *>(
        reinterpret_cast<char *>(lpNode) - offsetof(SubMix, mSubMixListNode));
    spSubMixNextNode = lpNode->pnext;
    return lpSubMix;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9C480 -- publish the bus.
//
// Everything that feeds this bus (the connected Sends and Routes) has already accumulated
// into mpSubMixBuffer by the time this runs, so Process only has to publish that buffer as
// the voice's output and clear it for the next frame. r5 (discontinuity) is unused; the
// single exit returns BUFFERSTATUS_AVAILABLE (1).
//
// NOTE the swap happens FIRST, before the copy: the destination descriptor becomes the
// published source, and the copy then writes into it. This is an assignment, not an
// accumulation -- the accumulation already happened upstream.
// -------------------------------------------------------------------------------------
int SubMix::Process(SubMix *self, AudioProcessContext *ctx, bool /*discontinuity*/)
{
    SampleBuffer *lpOutput = ctx->mpDstBuffer;      // lwz ctx+0x30010
    SampleBuffer *lpOldSource = ctx->mpSrcBuffer;   // lwz ctx+0x3000C
    ctx->mpSrcBuffer = lpOutput;                    // the ping-pong, done up front
    ctx->mpDstBuffer = lpOldSource;

    for (u32 luChannel = 0; luChannel < self->mOutputChannels; ++luChannel)
    {
        std::memcpy(lpOutput->mpSamples + lpOutput->muStride * luChannel,
                    self->mpSubMixBuffer + KI_FRAME_SAMPLES * luChannel,
                    KI_FRAME_SAMPLES * sizeof(f32));           // XMemCpy, 0x400 bytes
    }

    // A disconnect since the last frame folded its gain back into this bus, which would
    // otherwise step the output; ramp it out over the head of the frame.
    if (self->mDeClickRequired)
    {
        // The console also passes 256 in r6; the committed DeClick kernel consumes three
        // arguments and hard-codes its ramp length, so the frame size is not forwarded.
        DeClick(lpOutput, self->mDeClickValueTotal, self->mOutputChannels);
        self->mDeClickRequired = 0;
    }

    // Clear the accumulation buffer for the next frame's contributors.
    std::memset(self->mpSubMixBuffer, 0,
                static_cast<size_t>(self->mOutputChannels)
                    * KI_FRAME_SAMPLES * sizeof(f32));
    return 1;   // BUFFERSTATUS_AVAILABLE
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82BA0C18 -- vt[0], modelled here as the destructor (the Dac precedent).
//
// Three steps: drain every inbound connector (each Disconnect unlinks itself, so the loop
// re-reads the head each time), unlink this bus from the global registry if it was ever
// added, and free the accumulation buffer.
// -------------------------------------------------------------------------------------
SubMix::~SubMix()
{
    // Drain the inbound connector list. Disconnect(conn, 0) means "no fold-back": the bus
    // is going away, so there is nothing to fold the gains into.
    while (SubMixConnector *lpConnector = GetSendListHead())
        SubMixConnector::Disconnect(lpConnector, 0);

    if (mSubMixAdded)
    {
        // The full doubly-linked remove, guarded on being the head.
        ListDNode *lpNode = &mSubMixListNode;
        if (lpNode == sSubMixList.phead)
            sSubMixList.phead = lpNode->pnext;
        if (lpNode->pprev)
            lpNode->pprev->pnext = lpNode->pnext;
        if (lpNode->pnext)
            lpNode->pnext->pprev = lpNode->pprev;
    }

    if (mpSubMixBuffer)
        System::Free(mpSystemUseGetSystemAccessor, mpSubMixBuffer, 0);

    // FAITHFUL: the console does NOT clear mpSubMixBuffer or mSubMixAdded after freeing
    // and unlinking, so both are left stale. Clearing them would be safer but is behaviour
    // the console does not have; documented rather than silently added.
}

} // namespace core
} // namespace audio
} // namespace rw
