// =====================================================================================
// rw::audio::core::Send -- member bodies reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC). The asm is authoritative for every store; see Send.h for the reconstructed
// layout.
//
// PROVENANCE (corrected twice now; an earlier note here claimed "no reference source and
// no DecFIGS DWARF exist for this type" -- BOTH halves of that were wrong):
//   references/LegacySrc/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/2.11.00/include/
//     rw/audio/core/plugins/send.h -- the complete class, in declaration order:
//     `Attribute_t mAttribute[ATTRIBUTE_MAX]; SubMixConnector mSubMixConnector;
//     float mDeClickValue[MAX_CHANNELS]; float mPreviousGain; unsigned char
//     mDiscontinuity;` with `enum EventId { EVENT_CONNECTBYPOINTER = 0,
//     EVENT_CONNECTBYNAME = 1 }` and both Connect*Handler declarations. This is the
//     highest-authority source and it agrees with the reconstructed layout member-for-
//     member. (Its `void DisconnectImmediate();` is a non-static member returning void;
//     this tree models the identical ABI as `static SubMixConnector*
//     DisconnectImmediate(Send*)` -- see the FLAG on that function.)
//   references/DecFIGS/dwarfdump/SDKs/EATech/include/rw/audio/core/plugins/send.h --
//     partial (Send::ConnectByPointerParams and `GUID = 1399156272` == 'Sen0').
//   The NFS ProStreet08Milestone.pdb (X360, Oct-2007) carries rw::audio::core::Send
//   [sizeof=104] with the same offsets (mAttribute[1] @+0x28, mSubMixConnector @+0x30,
//   mDeClickValue[6] @+0x44, mPreviousGain @+0x5C, mDiscontinuity @+0x60) and is kept as
//   corroboration (full dump: scratchpad/waveL/Send.spec.md).
//
// Send is a PlugIn-family node that mixes (accumulates) its input into a target SubMix
// through an embedded SubMixConnector subobject (this+0x30), with a deferred connect/
// release path through the owning System's command ring (base @ +0x20, byte cursor @
// +0x10B8) and a Gain-style de-click ramp on the send level. Each store WIDTH below
// (stw / stfs / stb) is reproduced from the asm.
//
// PART-FILE / ODR NOTE: one ledger function of this TU is bodied in a SIBLING part-file,
// not here -- `Send::ConnectByNameHandler` @0x82B9FF80 lives in
// b5-decomp/vendor/renderware/src/rw/audio/core/Send_wL_01.cpp (wave L). It IS defined.
// Do not add a second definition in this file. An earlier banner here and a block further
// down both said "BLOCKED, NOT reconstructed"; that is no longer true and the stale text
// has been replaced.
// =====================================================================================

#include "rw/audio/core/Send.h"
#include "rw/audio/core/SubMix.h" // SubMix (the real class; moved out of SubMixConnector.h)
#include "rw/audio/core/PlugIn.h"      // rw::audio::core::System (deferred-command ring)
#include "rw/audio/core/MixKernels.h"  // ReChannelGainWrite / ReChannelGainMix / ...RampMix

#include <cstddef> // offsetof -- the name-command record's header size on the host

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F534 -- the Send (derived) vtable installed by CreateInstance: 4 slots
// {ReleaseEvent, EventEvent, <ICF'd trivial virtual>, scalar deleting destructor}.
// off_820AA810 -- the PlugIn base vtable the scalar-deleting destructor reverts to.
// off_82F8FF60 -- Send::sPlugInDescRunTime: a full 52-byte rw::audio::core::
// PlugInDescRunTime { name="Send", &GetSize, &CreateInstance, pPreProcess=0, &Process,
// pChannelMaps, pParameterDescRunTime, pEventDescRunTime, 0, 0, guid='Sen0',
// type=4/ctorParams=0/attrs=1/events=2 }. The contents of all three ARE recoverable
// (rodata dump: scratchpad/waveL/Send.spec.md) -- an earlier note claiming "no exported
// contents" was wrong. They stay modelled as file-static sentinel storage here only for
// consistency with the Route.cpp / Gain.cpp idiom until PlugInDescRunTime gets a real
// type home; the sentinels under-model the descriptor, which is safe today because no
// committed consumer dereferences it yet (see the spec's follow-up note).
static void* skSendVTableSlot = 0;
static void* const skpSendVTable = &skSendVTableSlot;             // off_8217F534

static void* const skpSendBaseVTable = gpBasePlugInVTableSentinel;  // off_820AA810 (SHARED sentinel, PlugIn.h -- wave 5)

// off_82F8FF60 -- the "Send" runtime descriptor, REAL (descriptor-record wave).
// Metadata FLAG'd null.
static PlugInDescRunTime g_SendDesc = {
    "Send",
    reinterpret_cast<void *>(&Send::GetSize),        // @0x82B98300
    reinterpret_cast<void *>(&Send::CreateInstance), // @0x82BA3E98
    0,
    reinterpret_cast<void *>(&Send::Process),        // @0x82B9B7A8
    0, 0, 0, 0,
    0,
    0x53656E30u,       // 'Sen0'
    4, 0, 1, 2, 0, 0,
    0
};

// Full mixer frame processed per Process call (li r?, 0x100).
enum { KI_MIXER_FRAME_SIZE = 256 };

// -------------------------------------------------------------------------------------
// GetSize @0x82B98300 -- li r3, 0x68 ; blr
// -------------------------------------------------------------------------------------
int Send::GetSize()
{
    // X360-LITERAL TRAP: the console immediate is this object's CONSOLE footprint, but
    // GetSize is the plug-in factory's allocation stride -- it allocates GetSize() bytes and
    // constructs the object into them. On the host the object is larger (widened vptr and
    // pointers), so returning the console value under-allocates and corrupts what follows.
    // Return host sizeof; the console immediate stays in the comment above.
    return static_cast<int>(sizeof(Send));   // X360: li r3, 0x68
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9B798 -- lis/addi off_82F8FF60 ; blr (returns &descriptor)
// -------------------------------------------------------------------------------------
char** Send::GetPlugInDescRunTime()
{
    return reinterpret_cast<char**>(&g_SendDesc); // &off_82F8FF60 (the record address)
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA3E98 -- placement-init a Send over `self`.
//   if (self) { *self = off_8217F534 ; self->connector.mpSubMixBuffer(+0x38)=0 ;
//               self->connector.mpSubMix(+0x3C)=0 ; self->connector.mNumSubMixChannels(+0x40)=0 (stb) }
//   self->mbNeedReset(+0x60) = 0 (stb)
//   self->mfCurrentGain(+0x5C) = 1.0
//   self->mBase.mpAttributes(+0x0C) = &self->mfGain(+0x28)
//   self->mfGain(+0x28) = 1.0
//   zero 6 dwords at self->mDeClickValue(+0x44)
//   return 1
// The +0x60/+0x5C/+0x0C/+0x28 stores + the +0x44 loop run unconditionally (even when
// self == 0); the only real caller always passes a valid buffer, so the (semantically
// identical) guarded form is used for the connector stores while the rest run inline.
// -------------------------------------------------------------------------------------
int Send::CreateInstance(Send* self)
{
    if (self)
    {
        self->mBase.mpVTable = skpSendVTable;              // *self = off_8217F534
        self->mSubMixConnector.mpSubMixBuffer = 0;         // stw 0, 0x38
        self->mSubMixConnector.mpSubMix = 0;               // stw 0, 0x3C
        self->mSubMixConnector.mNumSubMixChannels = 0;     // stb 0, 0x40
    }

    self->mbNeedReset = 0;                                 // stb 0, 0x60
    self->mfCurrentGain = 1.0f;                            // stfs 1.0, 0x5C
    // Point the base attribute-table slot at the live send-level attribute so the base
    // SetAttribute writes land on mfGain.
    self->mBase.mpAttributes = &self->mfGain;              // stw self+0x28, 0x0C
    self->mfGain = 1.0f;                                   // stfs 1.0, 0x28

    for (int li = 0; li < 6; ++li)
        self->mDeClickValue[li] = 0.0f;                    // stw 0 -> +0x44 + 4*i
    return 1;
}

// -------------------------------------------------------------------------------------
// DisconnectImmediate @0x82B9FE38 -- unlink the embedded connector, folding the send's
// per-input-channel gains back into the target SubMix's channels.
//   n = self->connector.mNumSubMixChannels(+0x40)          ; lbz
//   if (!n) return SubMixConnector::Disconnect(&self->connector, 0)
//   for (i=0; i<6; ++i) { srcGain[i] = &self->mDeClickValue[i] ; dstPtr[i] = &foldBack[i] }
//   ReChannelGainWrite(dstPtr, srcGain, 1.0, n, self->mInputChannels, 1)
//   result = SubMixConnector::Disconnect(&self->connector, foldBack)
//   zero 6 dwords at self->mDeClickValue(+0x44)
//   return result
// (The X360 passes DisconnectImmediate a2/a3 through to ReChannelGainWrite's dead r5 slot;
// both are unused, so the reconstruction takes only `self`.)
//
// LegacySrc send.h and the Xbox 360 SDK map give the void member and release signatures.
// -------------------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
// The original rwAudio plug-in object emits these scalar loops in its size form.
#pragma optimize("s", on)
#endif
void Send::DisconnectImmediate()
{
    const unsigned channels = mSubMixConnector.mNumSubMixChannels;
    if (channels)
    {
        f32* source[6];
        f32* destination[6];
        f32 foldback[6];
        for (unsigned channel = 0; channel < 6; ++channel)
        {
            source[channel] = &mDeClickValue[channel];
            destination[channel] = &foldback[channel];
        }

        ReChannelGainWrite(destination, source, 1.0f, channels, mBase.mbFlag20, 1);
        SubMixConnector::Disconnect(&mSubMixConnector, foldback);
        for (unsigned channel = 0; channel < 6; ++channel)
            mDeClickValue[channel] = 0.0f;
    }
    else
    {
        SubMixConnector::Disconnect(&mSubMixConnector, 0);
    }
}

#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82BA3EF8 -- b DisconnectImmediate (tail call; `self` passes through).
// -------------------------------------------------------------------------------------
void Send::ReleaseEvent()
{
    DisconnectImmediate();
}

// -------------------------------------------------------------------------------------
// Process @0x82B9B7A8 -- accumulate the input into the target SubMix's buffer.
//   if (resetRamp || mbNeedReset) { mfCurrentGain = mfGain ; mbNeedReset = 0 }
//   n = connector.mNumSubMixChannels(+0x40) ; if (!n) { mbNeedReset = 1 ; return 1 }
//   in = mInputChannels(+0x20) ; dstBase = connector.mpSubMixBuffer(+0x38) ; src = ctx->mpSrcBuffer
//   for (c<in) srcPtr[c] = src->mpSamples + src->muStride*c
//   for (i<n)  dstPtr[i] = dstBase + 256*i                       ; 1024-byte channel stride
//   if (mfGain == mfCurrentGain) ReChannelGainMix    (dstPtr, srcPtr, mfGain, n, in, 256)
//   else                         ReChannelGainMixRamp(dstPtr, srcPtr, mfGain, mfCurrentGain, n, in, 256)
//   mfCurrentGain = mfGain
//   for (c<in) mDeClickValue[c] = srcPtr[c][255] * mfGain       ; last-sample foldback seed
//   return 1
// -------------------------------------------------------------------------------------
#if defined(_XBOX) && defined(_MSC_VER)
// The original rwAudio plug-in object emits these scalar loops in its size form.
#pragma optimize("s", on)
#endif
BufferStatus Send::Process(PlugIn* plugIn, Mixer* ctx, bool resetRamp)
{
    Send* self = reinterpret_cast<Send*>(plugIn);
    if (resetRamp || self->mbNeedReset)
    {
        self->mbNeedReset = 0;                 // stb 0, 0x60
        self->mfCurrentGain = self->mfGain;    // stfs mfGain, 0x5C
    }

    const u32 luNumSubMixChannels = self->mSubMixConnector.mNumSubMixChannels; // lbz +0x40
    if (!luNumSubMixChannels)
    {
        self->mbNeedReset = 1;                 // stb 1, 0x60
        return BUFFERSTATUS_AVAILABLE;
    }

    const u32 luInputChannels = self->mBase.mbFlag20;              // lbz +0x20 (input channels)
    f32* const pSubMixBuffer = static_cast<f32*>(self->mSubMixConnector.mpSubMixBuffer); // lwz +0x38
    AudioChannelBuffer* srcBuffer = ctx->mpSrcBuffer;             // *(ctx+0x3000C)

    f32* lpSrcChannel[6]; // v17 (32-byte reservation)
    f32* lpDstChannel[6]; // v18 (24-byte reservation)

    for (u32 lc = 0; lc < luInputChannels; ++lc)
        lpSrcChannel[lc] = srcBuffer->LockChannel(lc);

    for (u32 li = 0; li < luNumSubMixChannels; ++li)
        lpDstChannel[li] = pSubMixBuffer + KI_MIXER_FRAME_SIZE * li; // +1024 bytes/channel

    if (self->mfGain != self->mfCurrentGain)
        ReChannelGainMixRamp(lpDstChannel, lpSrcChannel, self->mfGain, self->mfCurrentGain,
                             luNumSubMixChannels, luInputChannels, KI_MIXER_FRAME_SIZE);
    else
        ReChannelGainMix(lpDstChannel, lpSrcChannel, self->mfGain,
                         luNumSubMixChannels, luInputChannels, KI_MIXER_FRAME_SIZE);

    self->mfCurrentGain = self->mfGain; // stfs mfGain, 0x5C

    if (luInputChannels)
    {
        for (u32 lc = 0; lc < luInputChannels; ++lc)
            self->mDeClickValue[lc] = lpSrcChannel[lc][255] * self->mfGain; // lfs +0x3FC * mfGain
    }
    return BUFFERSTATUS_AVAILABLE;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", off)
#endif


// -------------------------------------------------------------------------------------
// ConnectByPointerHandler @0x82B9FEF8 -- replay a queued pointer-connect command (cmd =
// the command record; cmd+4 = the Send instance, cmd+8 = the target SubMix).
//   send = *(cmd+4) ; DisconnectImmediate(send)
//   subMix = *(cmd+8)
//   if (subMix) {
//       conn = &send->connector(+0x30)
//       conn->mpSubMix        = subMix                     ; stw 0xC(conn)
//       conn->mpSubMixBuffer  = subMix->mpSubMixBuffer     ; lwz 0x24 -> stw 8(conn)
//       conn->mNumSubMixChannels = (u8)subMix->mbNumChannels ; lbz 0x21 -> stb 0x10(conn)
//       head = subMix->mpConnectorHead(+0x28)
//       conn->mppPrev = 0                                  ; stw 0, 4(conn)
//       conn->mpNext  = head                               ; stw head, 0(conn)
//       if (head) head->mppPrev = conn                     ; stw conn, 4(head)
//       subMix->mpConnectorHead = conn                     ; stw conn, 0x28(subMix)
//   }
//   return 12
// NOTE the head back-link: the asm `stw r11,4(r9)` writes the connector pointer into the
// old head's mppPrev slot, matching Route::ConnectByPointerHandler. (Unlike Route, Send
// stores no trailing byte-gain triple and returns 12, not 24.)
// RECORD STRIDE (X360-literal trap): the returned value IS the ring advance for this
// record, i.e. the console sizeof(SendConnectByPointerCommand) -- on the host that is the
// host sizeof (24: three widened pointers), never the console literal 12.
// -------------------------------------------------------------------------------------
int Send::ConnectByPointerHandler(Command * cmd)
{
    SendConnectByPointerCommand* lpCmd = static_cast<SendConnectByPointerCommand*>(cmd);
    Send* lpSend = static_cast<Send *>(lpCmd->pObject); // send = *(cmd+4)

    lpSend->DisconnectImmediate();

    SubMix* lpSubMix = lpCmd->mpSubMix; // subMix = *(cmd+8)
    if (lpSubMix)
    {
        SubMixConnector* lpConn = &lpSend->mSubMixConnector; // conn = send+0x30
        lpConn->mpSubMix = lpSubMix;                                     // stw 0xC(conn)
        lpConn->mpSubMixBuffer = lpSubMix->mpSubMixBuffer;               // lwz 0x24 -> stw 8
        lpConn->mNumSubMixChannels = lpSubMix->mOutputChannels;              // lbz 0x21 -> stb 0x10

        SubMixConnector* lpHead = lpSubMix->GetSendListHead();          // lwz 0x28(subMix)
        lpConn->mppPrev = 0;                                            // stw 0, 4(conn)
        lpConn->mpNext = lpHead;                                        // stw head, 0(conn)
        if (lpHead)
            lpHead->mppPrev = reinterpret_cast<SubMixConnector**>(lpConn); // stw conn, 4(head)
        lpSubMix->SetSendListHead(lpConn);                             // stw conn, 0x28(subMix)
    }
    return static_cast<int>(sizeof(SendConnectByPointerCommand)); // X360: li r3, 0xC @0x82B9FF64
}

// -------------------------------------------------------------------------------------
// ConnectByNameHandler @0x82B9FF80 -- DEFINED, in the sibling part-file
// b5-decomp/vendor/renderware/src/rw/audio/core/Send_wL_01.cpp. NOT a hole: adding a
// definition here would be an ODR violation. What follows is the reading notes that
// unblocked it, kept because they carry SubMix-side asm this file's siblings still need.
//
// The X360 body disconnects the send's connector then walks a GLOBAL by-name SubMix
// registry to find the SubMix whose name matches the queued command's name string, and
// links the connector into it:
//   send = *(cmd+4) ; DisconnectImmediate(send)
//   for (link = off_8327EE68; ...; ) {           // global list head
//       dword_8327EE00 = link->next;             // global iterator cursor (side-effect)
//       node = (SubMix*)((char*)link - 0x2C);     // intrusive link at SubMix+0x2C
//       if (!node) break;                         // `addic. r10,r11,-0x2C ; bne` -- the
//                                                 // owner-is-NULL test, i.e. the caller's
//                                                 // `EnumerateSubMix() != NULL`, NOT an
//                                                 // "address 0x2C sentinel"
//       if (strcmp(node + 0x4C, cmd + 0xC) == 0) { ...link connector into node... }
//   }
//   return *(cmd+8)                              // == the record's own muRecordSize
//
// The registry's SHAPE is no longer a guess -- it is now grounded in the two SubMix
// functions that own it (both still un-reconstructed; there is no SubMix.cpp):
//   SubMix::CreateInstanceHandler @0x82B9C380 pushes onto the list:
//     node = self+0x2C ; node->mpNext(+0x2C) = off_8327EE68 ; node->mppPrev(+0x30) = 0
//     if (off_8327EE68) off_8327EE68->mppPrev = node ; off_8327EE68 = node
//     self->mbRegistered(+0x8C) = 1 ; return 8
//   SubMix::ReleaseEvent @0x82BA0C18 unlinks it (guarded by mbRegistered):
//     if (node == off_8327EE68) off_8327EE68 = node->mpNext
//     if (node->mppPrev) *node->mppPrev = node->mpNext
//     if (node->mpNext) node->mpNext->mppPrev = node->mppPrev
// So off_8327EE68 is the head of an intrusive DOUBLY-linked list whose node sits at
// SubMix+0x2C (mpNext at +0x2C, mppPrev at +0x30 -- the same ListDNode idiom as
// SubMixConnector), the loop's `link == 0x2C` test is just `link - 0x2C == NULL` (an
// owner-is-null break), and the compared name lives at SubMix+0x4C.
//
// WAVEL UPDATE -- the registry is FULLY grounded, and the grounding is the LegacySrc reference
// (references/LegacySrc/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/2.11.00/include/
// rw/audio/core/plugins/submix.h, which declares the whole SubMix class in order) plus
// the DecFIGS dwarfdump of the same header (`extern ListDStack sSubMixList;` submix.h:151,
// `extern ListDNode * spSubMixNextNode;` submix.h:152); ProStreet08Milestone.pdb + .map
// merely corroborate. All confirmed against the ARTIST asm; see
// scratchpad/waveL/Send.spec.md:
//   off_8327EE68  == SubMix::sSubMixList (private static ListDStack {phead}) -- the map
//                    carries ?sSubMixList@SubMix@core@audio@rw@@0VListDStack@234@A.
//   dword_8327EE00== SubMix::spSubMixNextNode (private static ListDNode*) -- the safe-
//                    iteration cursor of SubMix::EnumerateSubMixReset/EnumerateSubMix,
//                    which ARTIST inlined into this function (that is why nothing else
//                    in the export reads it -- the standalone enumerators were folded).
//                    submix.h spells the reset inline: `spSubMixNextNode =
//                    sSubMixList.GetHead();` == `dword_8327EE00 = *off_8327EE68`.
//   node @+0x2C   == ListDNode SubMix::mSubMixListNode; name @+0x4C == char mName[64];
//                    flag @+0x8C == u8 mSubMixAdded (set by CreateInstanceHandler
//                    @0x82B9C380, tested by ReleaseEvent @0x82BA0C18). The gap +0x34..
//                    +0x4B is `float mDeClickValueTotal[MAX_CHANNELS]` with MAX_CHANNELS
//                    = 6, and +0x8D is mDeClickRequired.
// STATUS: both of the out-of-file prerequisites are DONE.
//   1. SubMixConnector.h (SHARED -- Route.h and the embed check also include it) now
//      carries the SubMix registry members and the two static declarations. The statics'
//      DEFINITIONS still belong to the seeded-but-todo class:rw::audio::core::SubMix TU
//      (there is no SubMix.cpp yet) and stay unresolved at link until it lands -- the
//      normal leaf-first stubbing situation, invisible to `cl /c`.
//      (Header naming note: it keeps the ARTIST-grounded spellings mafChannelGain /
//      mbSubMixAdded / mbDirty and records the original spellings mDeClickValueTotal /
//      mSubMixAdded / mDeClickRequired per member -- those originals are now attested by
//      the LegacySrc header, not only by the PDB as the header's comment still says.)
//   2. The header's old `mafChannelGain` f32[22] spanning +0x34..+0x8B -- which swallowed
//      mName and packed its dirty byte at +0x8C, one byte early -- is gone; the tail is
//      now f32[6] @+0x34, char[64] @+0x4C, and the two flag bytes @+0x8C/+0x8D, pinned by
//      offsetof static_asserts in the header.
// EventEvent below faithfully stores &Send::ConnectByNameHandler into the name-path
// command; the handler body itself is in Send_wL_01.cpp.
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// EventEvent @0x82BA3F00 -- queue a connect command into the owning System's deferred-
// command ring (base @ System+0x20, byte cursor @ System+0x10B8).
//   sys = self->mpSystem(+0x04)
//   if (useName) {                                  // variable-length name command
//       len = strlen(payload[0])
//       size = (len + 16) & ~3                       ; 12-byte header + name+NUL, 4-aligned
//       cmd  = sys->ring + cursor ; cursor += size
//       cmd[0] = &ConnectByNameHandler ; cmd[1] = self ; cmd[2] = size
//       strcpy((char*)cmd + 12, payload[0])
//   } else {                                        // 12-byte pointer command
//       cmd = sys->ring + cursor ; cursor += 12
//       cmd[0] = &ConnectByPointerHandler ; cmd[1] = self ; cmd[2] = payload[0]  (SubMix*)
//   }
//   return self
//
// RECORD STRIDE (X360-literal trap): both cursor advances are the record's OWN size, so
// they are host sizeof/offsetof expressions here -- the console immediates (`addi r9,r9,0xC`
// @0x82BA3F20 and the `(len + 16) & ~3` sequence @0x82BA3F74-7C) are the 32-bit sizes and
// would under-advance the ring once the handler/target pointers widen to 8 bytes.
// -------------------------------------------------------------------------------------
Send* Send::EventEvent(Send* self, int useName, void* const* payload)
{
    System* lpSystem = static_cast<System*>(self->mBase.mpSystem); // *(self+4)
    char* lpRingBase = lpSystem->mpDeferredRingBase;   // *(sys+0x20)
    u32 luCursor = lpSystem->muDeferredRingCursor;     // *(sys+0x10B8)

    if (useName)
    {
        // strlen of the queued name (payload[0] points at the source string).
        const char* lpName = static_cast<const char*>(payload[0]);
        u32 luLen = 0;
        while (lpName[luLen])
            ++luLen;

        // The record's own byte size: header + name + NUL, rounded up so the NEXT record's
        // leading handler pointer stays aligned. X360: `(len + 16) & ~3` (@0x82BA3F74 addi
        // 0x10 / @0x82BA3F7C clrrwi 2) == a 12-byte header + NUL, 4-aligned for the console's
        // 4-byte pointers; the host header is offsetof(maName) and the align is 8.
        const u32 luSize =
            (static_cast<u32>(offsetof(SendConnectByNameCommand, maName)) + luLen + 1 + 7) & ~7u;

        SendConnectByNameCommand* lpCmd =
            reinterpret_cast<SendConnectByNameCommand*>(lpRingBase + luCursor);
        lpSystem->muDeferredRingCursor = luCursor + luSize;

        lpCmd->handler = &Send::ConnectByNameHandler; // cmd[0]
        lpCmd->pObject = self;                         // cmd[1]
        lpCmd->muRecordSize = luSize;                   // cmd[2]

        char* lpDst = lpCmd->maName; // cmd + 0xC (X360)
        const char* lpSrc = lpName;
        char lch;
        do
        {
            lch = *lpSrc++;
            *lpDst++ = lch;
        } while (lch);
    }
    else
    {
        SendConnectByPointerCommand* lpCmd =
            reinterpret_cast<SendConnectByPointerCommand*>(lpRingBase + luCursor);
        lpSystem->muDeferredRingCursor =
            luCursor + static_cast<u32>(sizeof(SendConnectByPointerCommand)); // X360: +0xC

        lpCmd->handler = &Send::ConnectByPointerHandler; // cmd[0]
        lpCmd->pObject = self;                            // cmd[1]
        lpCmd->mpSubMix = static_cast<SubMix*>(payload[0]); // cmd[2] -- connect event word 0
    }
    return self;
}

// -------------------------------------------------------------------------------------
// ScalarDeletingDestructor @0x82BA1D58
//   *self = off_820AA810   ; revert to the PlugIn base vtable
//   if (flags & 1) operator delete(self)
//   return self
// -------------------------------------------------------------------------------------
void* Send::ScalarDeletingDestructor(void* self, char flags)
{
    *reinterpret_cast<void**>(self) = skpSendBaseVTable; // *self = off_820AA810
    if (flags & 1)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw

// FOLDED FROM Send_wL_01.cpp
// =====================================================================================
// rw::audio::core::Send -- part-file 01 of the TU: the one remaining ledger body,
//
//   Send::ConnectByNameHandler @0x82B9FF80
//
// It lands in this part-file rather than in the committed Send.cpp because that file is
// owned by another worker this wave. Reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is authoritative for every store.
//
// PROVENANCE (corrected -- an earlier banner here claimed "there is no LegacySrc source
// and no DecFIGS DWARF for this type"; both halves of that were WRONG, and the claim was
// suppressing the highest-authority sources in the tree):
//   references/LegacySrc/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/2.11.00/include/
//     rw/audio/core/plugins/send.h    -- declares `static int ConnectByNameHandler(
//     Command *pCommand);`, `struct ConnectByNameCommand : public Command,
//     ConnectByNameParams {};`, `void DisconnectImmediate();` and the Send member list.
//   .../plugins/submix.h              -- the COMPLETE SubMix class: `static ListDStack
//     sSubMixList; static ListDNode *spSubMixNextNode; float *mpSubMixBuffer; ListDStack
//     mSendList; ListDNode mSubMixListNode; float mDeClickValueTotal[MAX_CHANNELS];
//     char mName[64]; unsigned char mSubMixAdded; unsigned char mDeClickRequired;`
//     (= console +0x24/+0x28/+0x2C/+0x34/+0x4C/+0x8C/+0x8D -- every one of those matches
//     an ARTIST load/store in this function or its SubMix-side siblings), plus the two
//     enumerator bodies quoted at the MEASURED block below and `const char *GetName()
//     { return (mName); }`. .../channel.h:22 gives `MAX_CHANNELS = 6`, which is why
//     mDeClickValueTotal spans exactly 0x34..0x4B.
//   references/DecFIGS/dwarfdump/SDKs/EATech/include/rw/audio/core/plugins/submix.h
//     -- emits `extern ListDStack sSubMixList;` (submix.h:151) and `extern ListDNode *
//     spSubMixNextNode;` (submix.h:152), independently confirming the two statics.
//   The October 2007 X360 audio PDB and map for the same RenderWare audio interface
//   agrees with all of the above and is kept only as corroboration, not as the basis.
// Every offset was still re-checked against the ARTIST asm (see the annotated walk below
// and scratchpad/waveL/Send.spec.md sections 1 and 3).
//
// ARTIST-vs-LegacySrc DIVERGENCE (measured -- the reference must NOT be copied here). LegacySrc
// `struct ConnectByNameParams { const char *pName; };` puts a POINTER at command+0xC.
// ARTIST does not: `addi r3,r31,8 ; addi r4,r3,4` makes r4 = cmd+0xC and the compare loop
// then does `mr r9,r4 ; lbz r7,0(r9)` -- a byte load straight off cmd+0xC with NO
// intervening `lwz`. The ARTIST record therefore INLINES the name string, which is what
// EventEvent @0x82BA3F00 builds (12-byte header + strcpy'd name, size `(len+16)&~3` on
// the console). The tree's `char maName[1]` + `muRecordSize` model in Send.h is the
// correct one for this build; the 2.11.00 by-pointer form is a different revision.
//
// PATH NOTE (measured): the wave brief named
// `b5-decomp/vendor/renderware/audio/core/Send_wL_01.cpp`, but that directory does not
// exist in the tree -- the renderware vendor sources live under
// `b5-decomp/vendor/renderware/src/rw/audio/core/` (where Send.cpp itself and the
// previous wave's part-files EaXmaDec_wG_02/03/04.cpp sit). The basename is unchanged,
// so the wave's collision-free-by-construction property is preserved.
//
// GATE STATE: COMPILES. The shared-header change this file used to be blocked on has
// landed -- b5-decomp/vendor/renderware/include/rw/audio/core/SubMixConnector.h now
// carries `ListDNode mSubMixListNode` (console +0x2C), `char mName[64]` (console +0x4C),
// `static ListDStack sSubMixList` (off_8327EE68) and `static ListDNode *spSubMixNextNode`
// (dword_8327EE00), plus the `struct ListDStack { ListDNode *phead; }` definition.
// MEASURED (this pass): `PYTHONUTF8=1 py tools/work/selfcheck.py Send.cpp Send_wL_01.cpp`
// -> STATUS=pass. Do not re-open the header on this function's account.
//
// HISTORICAL NOTE ON THE UNBLOCK SPEC -- DO NOT APPLY ITS SECTION 2 VERBATIM (measured):
// scratchpad/waveL/Send.spec.md section 2(a) asks SubMixConnector.h to DEFINE a new
// `class ListDNode { pnext; pprev; }`. That type ALREADY HAS A REAL HOME:
// `struct rw::audio::core::ListDNode` at
// b5-decomp/vendor/renderware/include/rw/audio/core/ITask.h:31 (same members; LegacySrc
// homes both -- `class ListDNode` at private/linklist.h:54, `class ListDStack` at
// private/linklist.h:142 -- in rw/audio/core/private/linklist.h). Defining it a second
// time is a redefinition (C2011) in every TU that sees both headers -- MEASURED: Route.cpp
// and Send.cpp both FAILED to compile with the spec's verbatim text (Send.cpp reaches
// ITask.h through PlugIn.h). The header took the correct form instead: it `#include`s
// "rw/audio/core/ITask.h" and adds only `struct ListDStack { ListDNode *phead; };`
// (1 member -- LegacySrc's ListDStack has exactly one data member, `ListDNode *phead`, the
// rest of the class being inline accessors that ARTIST open-codes at every call site).
//
// LINK-TIME (cl /c cannot see these -- reported, not hidden): the DEFINITIONS of
// SubMix::sSubMixList and SubMix::spSubMixNextNode belong to the seeded-but-todo
// `class:rw::audio::core::SubMix` TU (GetSize @0x82B982F0, GetPlugInDescRunTime
// @0x82B9C370, CreateInstanceHandler @0x82B9C380, Process @0x82B9C480, ReleaseEvent
// @0x82BA0C18, scalar-deleting dtor @0x82BA1DE8 -- none reconstructed yet). They stay
// unresolved at link until that TU lands, exactly like any other not-yet-reconstructed
// callee. Send::DisconnectImmediate is already bodied in Send.cpp.
// =====================================================================================

#include "rw/audio/core/Send.h"            // Send, SendConnectByNameCommand
#include "rw/audio/core/SubMixConnector.h" // SubMixConnector, ListDNode, ListDStack
#include "rw/audio/core/SubMix.h"          // SubMix (the real class; moved out of SubMixConnector.h)

#include <cstddef> // offsetof -- node-to-owner conversion on the HOST
#include <cstring> // std::strcmp -- the asm's inlined character loop, de-inlined

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// ConnectByNameHandler @0x82B9FF80 -- replay a queued NAME-connect command off the
// owning System's deferred-command ring (the consumer calls it through int (*)(void *)
// with the record's own address; EventEvent @0x82BA3F00 wrote the record).
//
// Annotated asm walk (raw listing: .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82B9FF80.json):
//   r31 = cmd ; r30 = *(cmd+4) = send ; bl Send::DisconnectImmediate(send)
//   r11 = off_8327EE68            ; sSubMixList.phead
//   dword_8327EE00 = r11          ; spSubMixNextNode = head  (EnumerateSubMixReset inlined)
//   r3  = cmd+8 (kept live for the return) ; r4 = r3+4 = cmd+0xC (the queued name)
//   b loc_82B9FFF4                ; the loop's node-null test is at the TOP
// loc_82B9FFF4:
//   cmplwi cr6, r11, 0 ; beq -> done          ; end of list
//   addic. r10, r11, -0x2C                    ; node -> owning SubMix (CR0 <- owner)
//   lwz r11, 0(r11)                           ; node = node->pnext
//   stw r11, dword_8327EE00                   ; spSubMixNextNode = node  (EnumerateSubMix inlined)
//   bne loc_82B9FFC0 ; else b done            ; owner == NULL -> stop
// loc_82B9FFC0 .. loc_82B9FFEC:               ; inlined strcmp(cmd+0xC, subMix+0x4C)
//   r8 = r10+0x4C ; r9 = r4 ; per char: r7=*r9, r5=*r8, r5 = r7 - r5
//   exit when *r9 == 0 or r5 != 0 ; then `cmpwi r5,0 ; beq loc_82BA0010` == equal strings
//   not equal -> fall through to loc_82B9FFF4 (next node)
// loc_82BA0010 (MATCH -- the same tail as ConnectByPointerHandler @0x82BA0010..0x82BA0048):
//   r11 = r30+0x30 = &send->mSubMixConnector
//   stw r10, 0xC(r11)                         ; conn->mpSubMix = subMix
//   lwz r8, 0x24(r10) ; stw r8, 8(r11)        ; conn->mpSubMixBuffer = subMix->mpSubMixBuffer
//   lbz r8, 0x21(r10) ; stb r8, 0x10(r11)     ; conn->mNumSubMixChannels = subMix->mbNumChannels
//   lwz r8, 0x28(r10) ; stw 0, 4(r11) ; stw r8, 0(r11)
//   lwz r9, 0x28(r10) ; if (r9) stw r11, 4(r9)  ; head->mppPrev = conn
//   stw r11, 0x28(r10)                        ; subMix->mpConnectorHead = conn
//   (falls through to done -- stops after the FIRST match)
// done: lwz r3, 0(r3) ; blr                   ; return *(cmd+8) == muRecordSize
//
// MEASURED. Every offset/width/side-effect above, the loop shape, the first-match break
// and the return operand come from the asm. The NAMES and TYPES are measured too, from
// LegacySrc plugins/submix.h (see the provenance block at the top) and corroborated by the
// DecFIGS dwarfdump of the same header: `static ListDStack sSubMixList` and
// `static ListDNode *spSubMixNextNode` are SubMix's own private statics, and
// `mSubMixListNode` / `mName` are the members the asm's -0x2C and +0x4C reach.
//
// The two cursor writes are the inlined enumerator pair, also MEASURED rather than
// guessed: submix.h spells `static void EnumerateSubMixReset() { spSubMixNextNode =
// sSubMixList.GetHead(); }` -- which is exactly `dword_8327EE00 = *off_8327EE68`
// @0x82B9FFB4-B8 -- and declares `static SubMix *EnumerateSubMix();`, whose
// node->owner + advance + return-NULL contract is exactly the `addic. r10,r11,-0x2C /
// lwz r11,0(r11) / stw r11,cursor / bne` group @0x82B9FFFC-0x82BA0008.
//
// Still INFERRED (identification only, unaffected by the code): that the specific data
// addresses off_8327EE68 / dword_8327EE00 in THIS image are those two statics. The
// correspondence is exact in shape, offsets and order, and the X360 audio map manglings
// ?sSubMixList@SubMix@core@audio@rw@@0VListDStack@234@A /
// ?spSubMixNextNode@SubMix@core@audio@rw@@0PAVListDNode@234@A agree; either way the
// cursor writes are reproduced, because they are real stores to class-owned state.
//
// X360-LITERAL TRAPS handled: (a) the console `-0x2C` is offsetof(SubMix,
// mSubMixListNode) on the host, never the literal -- SubMix carries host-width pointers
// so the byte offset differs; (b) the console `+0x4C` name offset is `mName` by member
// access, not arithmetic; (c) the return is the record's stored muRecordSize, which
// EventEvent already wrote as the HOST record size, NOT the console `(len + 16) & ~3`.
// No floating point anywhere in this function, so no fcmpu/NaN-polarity concern.
// -------------------------------------------------------------------------------------
int Send::ConnectByNameHandler(Command * cmd)
{
    SendConnectByNameCommand* lpCmd = static_cast<SendConnectByNameCommand*>(cmd);
    Send* lpSend = static_cast<Send *>(lpCmd->pObject); // lwz r30, 4(r31)

    lpSend->DisconnectImmediate(); // bl @0x82B9FFA0

    // Walk the global by-name SubMix registry. The X360 build inlined
    // SubMix::EnumerateSubMixReset()/EnumerateSubMix() here (both declared in LegacySrc
    // plugins/submix.h), so the class-static cursor SubMix::spSubMixNextNode is advanced
    // exactly where the asm advances it (a real side effect on SubMix-owned state -- kept).
    ListDNode* lpNode = SubMix::sSubMixList.phead; // lwz off_8327EE68
    SubMix::spSubMixNextNode = lpNode;             // stw dword_8327EE00
    while (lpNode)                                 // cmplwi cr6, r11, 0 @0x82B9FFF4
    {
        // Node-to-owner (EnumerateSubMix's body). The console form is
        // `addic. r10, r11, -0x2C`; its CR0 side effect is the owner-is-NULL test consumed
        // by the `bne` below. The host form must use offsetof -- the console byte offset is
        // not the host one, because SubMix's leading members widen on LLP64.
        SubMix* lpSubMix = reinterpret_cast<SubMix*>(
            reinterpret_cast<char*>(lpNode) - offsetof(SubMix, mSubMixListNode));

        lpNode = lpNode->pnext;            // lwz r11, 0(r11)
        SubMix::spSubMixNextNode = lpNode; // stw r11, dword_8327EE00

        // `bne loc_82B9FFC0 / b done` -- NOT a degenerate guard. LegacySrc submix.h
        // declares `static SubMix *EnumerateSubMix();`, so this is the caller's
        // `while ((pSubMix = EnumerateSubMix()) != NULL)` test: the enumerator's own
        // node-is-NULL exit is the `cmplwi cr6,r11,0` at the top of the loop, and this is
        // the returned-owner test the compiler could not fold away across the offsetof
        // subtraction. Kept because the asm keeps it.
        if (!lpSubMix)
            break;

        // Inlined strcmp @0x82B9FFC8..0x82B9FFE8, de-inlined per the de-optimization rule.
        // The asm's exit conditions (*name == 0, or a non-zero difference) plus the final
        // `difference == 0` test are exactly strcmp(...) == 0 for every prefix case.
        if (std::strcmp(lpCmd->maName, lpSubMix->mName) == 0)
        {
            SubMixConnector* lpConn = &lpSend->mSubMixConnector; // r11 = r30 + 0x30
            lpConn->mpSubMix = lpSubMix;                                     // stw 0xC(conn)
            lpConn->mpSubMixBuffer = lpSubMix->mpSubMixBuffer;               // lwz 0x24 -> stw 8
            lpConn->mNumSubMixChannels = lpSubMix->mOutputChannels;              // lbz 0x21 -> stb 0x10

            SubMixConnector* lpHead = lpSubMix->GetSendListHead(); // lwz 0x28(subMix)
            lpConn->mppPrev = 0;                                 // stw r9(=0), 4(conn)
            lpConn->mpNext = lpHead;                             // stw r8, 0(conn)
            if (lpHead)
                lpHead->mppPrev = reinterpret_cast<SubMixConnector**>(lpConn); // stw r11, 4(r9)
            lpSubMix->SetSendListHead(lpConn);                   // stw r11, 0x28(subMix)
            break; // the asm falls straight through to the epilogue: FIRST match only
        }
    }

    // The record's own byte size == the ring advance. EventEvent stored the HOST size
    // here, so this is NOT the console `(strlen + 16) & ~3`.
    return static_cast<int>(lpCmd->muRecordSize); // lwz r3, 0(r3) where r3 == cmd+8
}

} // namespace core
} // namespace audio
} // namespace rw
