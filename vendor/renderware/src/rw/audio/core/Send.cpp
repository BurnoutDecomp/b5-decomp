// =====================================================================================
// rw::audio::core::Send -- member bodies reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC). The asm is authoritative for every store; see Send.h for the reconstructed
// layout.
//
// PROVENANCE (corrected twice now; an earlier note here claimed "no reference source and
// no DecFIGS DWARF exist for this type" -- BOTH halves of that were wrong):
//   references/Feb-2007/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/2.11.00/include/
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

static void* skSendBaseVTableSlot = 0;
static void* const skpSendBaseVTable = &skSendBaseVTableSlot;     // off_820AA810

static char* const skpSendDescName = const_cast<char*>("Send");   // off_82F8FF60 (label "Send")

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
    return const_cast<char**>(&skpSendDescName);
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
//   ReChannelGainWrite(dstPtr, srcGain, n(numDst), self->mInputChannels(numSrc), 1, 1.0)
//   result = SubMixConnector::Disconnect(&self->connector, foldBack)
//   zero 6 dwords at self->mDeClickValue(+0x44)
//   return result
// (The X360 passes DisconnectImmediate a2/a3 through to ReChannelGainWrite's dead r5 slot;
// both are unused, so the reconstruction takes only `self`.)
//
// FLAG (shape, vs the Feb-2007 leak): plugins/send.h declares this as the non-static
// member `void DisconnectImmediate();`. This tree models the same ABI as
// `static SubMixConnector* DisconnectImmediate(Send* self)` -- `self` in r3 is identical
// either way, and the SubMixConnector* return is real in the asm (it is
// SubMixConnector::Disconnect's r3, tail-forwarded by ReleaseEvent @0x82BA3EF8) even
// though the leak's declaration discards it. Changing the shape would mean editing the
// shared Send.h; it is recorded here rather than silently "corrected".
// -------------------------------------------------------------------------------------
SubMixConnector* Send::DisconnectImmediate(Send* self)
{
    const u32 luNumSubMixChannels = self->mSubMixConnector.mNumSubMixChannels; // lbz +0x40

    if (!luNumSubMixChannels)
        return (
            SubMixConnector::Disconnect(&self->mSubMixConnector, 0)); // r3=self+0x30, r4=0

    // Remap the send's per-input-channel gains (mDeClickValue) into a SubMix-channel
    // foldback array. The two pointer arrays and the 24-byte output buffer mirror the
    // X360 stack temporaries (v10 src-gain ptrs, v11 dst ptrs, v12 foldback gains).
    f32* lpSrcGain[6]; // v10 -- &mDeClickValue[i]
    f32* lpDstPtr[6];  // v11 -- &lFoldBack[i]
    f32  lFoldBack[6]; // v12 -- remapped per-SubMix-channel foldback gains
    for (int li = 0; li < 6; ++li)
    {
        lpSrcGain[li] = &self->mDeClickValue[li];
        lpDstPtr[li] = &lFoldBack[li];
    }

    // numDst = mNumSubMixChannels, numSrc = mInputChannels, numSamples = 1, gain = 1.0.
    ReChannelGainWrite(lpDstPtr, lpSrcGain, luNumSubMixChannels,
                       self->mBase.mbFlag20, 1, 1.0f);

    SubMixConnector* lpResult =
        SubMixConnector::Disconnect(&self->mSubMixConnector, lFoldBack); // r3=self+0x30, r4=v12

    for (int li = 0; li < 6; ++li)
        self->mDeClickValue[li] = 0.0f; // zero 6 dwords at +0x44

    return lpResult;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82BA3EF8 -- b DisconnectImmediate (tail call; `self` passes through).
// -------------------------------------------------------------------------------------
SubMixConnector* Send::ReleaseEvent(Send* self)
{
    return DisconnectImmediate(self);
}

// -------------------------------------------------------------------------------------
// Process @0x82B9B7A8 -- accumulate the input into the target SubMix's buffer.
//   if (resetRamp || mbNeedReset) { mfCurrentGain = mfGain ; mbNeedReset = 0 }
//   n = connector.mNumSubMixChannels(+0x40) ; if (!n) { mbNeedReset = 1 ; return 1 }
//   in = mInputChannels(+0x20) ; dstBase = connector.mpSubMixBuffer(+0x38) ; src = ctx->mpSrcBuffer
//   for (c<in) srcPtr[c] = src->mpSamples + src->muStride*c
//   for (i<n)  dstPtr[i] = dstBase + 256*i                       ; 1024-byte channel stride
//   if (mfGain == mfCurrentGain) ReChannelGainMix    (dstPtr, srcPtr, n, in, 256, mfGain)
//   else                         ReChannelGainMixRamp(dstPtr, srcPtr, n, in, 256, mfGain, mfCurrentGain)
//   mfCurrentGain = mfGain
//   for (c<in) mDeClickValue[c] = srcPtr[c][255] * mfGain       ; last-sample foldback seed
//   return 1
// -------------------------------------------------------------------------------------
int Send::Process(Send* self, AudioProcessContext* ctx, char resetRamp)
{
    if (resetRamp || self->mbNeedReset)
    {
        self->mbNeedReset = 0;                 // stb 0, 0x60
        self->mfCurrentGain = self->mfGain;    // stfs mfGain, 0x5C
    }

    const u32 luNumSubMixChannels = self->mSubMixConnector.mNumSubMixChannels; // lbz +0x40
    if (!luNumSubMixChannels)
    {
        self->mbNeedReset = 1;                 // stb 1, 0x60
        return 1;
    }

    const u32 luInputChannels = self->mBase.mbFlag20;              // lbz +0x20 (input channels)
    f32* const pSubMixBuffer = static_cast<f32*>(self->mSubMixConnector.mpSubMixBuffer); // lwz +0x38
    AudioChannelBuffer* srcBuffer = ctx->mpSrcBuffer;             // *(ctx+0x3000C)

    f32* lpSrcChannel[8]; // v17 (32-byte reservation)
    f32* lpDstChannel[6]; // v18 (24-byte reservation)

    if (luInputChannels)
    {
        for (u32 lc = 0; lc < luInputChannels; ++lc)
            lpSrcChannel[lc] = srcBuffer->mpSamples + srcBuffer->muStride * lc;
    }

    for (u32 li = 0; li < luNumSubMixChannels; ++li)
        lpDstChannel[li] = pSubMixBuffer + KI_MIXER_FRAME_SIZE * li; // +1024 bytes/channel

    if (self->mfGain == self->mfCurrentGain)
        ReChannelGainMix(lpDstChannel, lpSrcChannel, luNumSubMixChannels,
                         luInputChannels, KI_MIXER_FRAME_SIZE, self->mfGain);
    else
        ReChannelGainMixRamp(lpDstChannel, lpSrcChannel, luNumSubMixChannels,
                             luInputChannels, KI_MIXER_FRAME_SIZE, self->mfGain,
                             self->mfCurrentGain);

    self->mfCurrentGain = self->mfGain; // stfs mfGain, 0x5C

    if (luInputChannels)
    {
        for (u32 lc = 0; lc < luInputChannels; ++lc)
            self->mDeClickValue[lc] = lpSrcChannel[lc][255] * self->mfGain; // lfs +0x3FC * mfGain
    }
    return 1;
}

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
int Send::ConnectByPointerHandler(void* cmd)
{
    SendConnectByPointerCommand* lpCmd = static_cast<SendConnectByPointerCommand*>(cmd);
    Send* lpSend = lpCmd->mpTarget; // send = *(cmd+4)

    DisconnectImmediate(lpSend);

    SubMix* lpSubMix = lpCmd->mpSubMix; // subMix = *(cmd+8)
    if (lpSubMix)
    {
        SubMixConnector* lpConn = &lpSend->mSubMixConnector; // conn = send+0x30
        lpConn->mpSubMix = lpSubMix;                                     // stw 0xC(conn)
        lpConn->mpSubMixBuffer = lpSubMix->mpSubMixBuffer;               // lwz 0x24 -> stw 8
        lpConn->mNumSubMixChannels = static_cast<u8>(lpSubMix->mbNumChannels); // lbz 0x21 -> stb 0x10

        SubMixConnector* lpHead = lpSubMix->mpConnectorHead;            // lwz 0x28(subMix)
        lpConn->mppPrev = 0;                                            // stw 0, 4(conn)
        lpConn->mpNext = lpHead;                                        // stw head, 0(conn)
        if (lpHead)
            lpHead->mppPrev = reinterpret_cast<SubMixConnector**>(lpConn); // stw conn, 4(head)
        lpSubMix->mpConnectorHead = lpConn;                            // stw conn, 0x28(subMix)
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
// WAVEL UPDATE -- the registry is FULLY grounded, and the grounding is the Feb-2007 leak
// (references/Feb-2007/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/2.11.00/include/
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
//                    = 6 (Feb-2007 channel.h:22), and +0x8D is mDeClickRequired.
// STATUS: both of the out-of-file prerequisites are DONE.
//   1. SubMixConnector.h (SHARED -- Route.h and the embed check also include it) now
//      carries the SubMix registry members and the two static declarations. The statics'
//      DEFINITIONS still belong to the seeded-but-todo class:rw::audio::core::SubMix TU
//      (there is no SubMix.cpp yet) and stay unresolved at link until it lands -- the
//      normal leaf-first stubbing situation, invisible to `cl /c`.
//      (Header naming note: it keeps the ARTIST-grounded spellings mafChannelGain /
//      mbSubMixAdded / mbDirty and records the original spellings mDeClickValueTotal /
//      mSubMixAdded / mDeClickRequired per member -- those originals are now attested by
//      the Feb-2007 header, not only by the PDB as the header's comment still says.)
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

        lpCmd->mpHandler = &Send::ConnectByNameHandler; // cmd[0]
        lpCmd->mpTarget = self;                         // cmd[1]
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

        lpCmd->mpHandler = &Send::ConnectByPointerHandler; // cmd[0]
        lpCmd->mpTarget = self;                            // cmd[1]
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
