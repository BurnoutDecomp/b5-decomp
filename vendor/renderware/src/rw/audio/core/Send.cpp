// =====================================================================================
// rw::audio::core::Send -- member bodies reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC). The asm is authoritative for every store; see Send.h for the reconstructed
// layout. No reference source, no DecFIGS DWARF, and no ProStreet08 PDB entry exist for
// this type.
//
// Send is a PlugIn-family node that mixes (accumulates) its input into a target SubMix
// through an embedded SubMixConnector subobject (this+0x30), with a deferred connect/
// release path through the owning System's command ring (base @ +0x20, byte cursor @
// +0x10B8) and a Gain-style de-click ramp on the send level. Each store WIDTH below
// (stw / stfs / stb) is reproduced from the asm.
//
// One ledger function is intentionally NOT reconstructed:
//   ConnectByNameHandler @0x82B9FF80 -- BLOCKED (see the stub note below): it walks a
//   global by-name SubMix registry through un-homed foreign statics (off_8327EE68 list
//   head + dword_8327EE00 iterator) with an X360-offset-specific intrusive-list idiom
//   whose element (SubMix) name/list-link members are not groundable from this TU.
// =====================================================================================

#include "rw/audio/core/Send.h"
#include "rw/audio/core/PlugIn.h"      // rw::audio::core::System (deferred-command ring)
#include "rw/audio/core/MixKernels.h"  // ReChannelGainWrite / ReChannelGainMix / ...RampMix

namespace rw
{
namespace audio
{
namespace core
{

// off_8217F534 -- the Send (derived) vtable installed by CreateInstance. off_820AA810 --
// the PlugIn base vtable the scalar-deleting destructor reverts to during teardown.
// off_82F8FF60 -- the static "Send" plug-in run-time descriptor (a pointer to the
// type-name string). These are opaque data symbols in the XEX (no exported contents);
// modelled as honest file-static sentinel storage so the bodies below link without
// fabricating their contents -- the same idiom Route.cpp / Gain.cpp use.
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
    return 104;
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
// -------------------------------------------------------------------------------------
int Send::DisconnectImmediate(Send* self)
{
    const u32 luNumSubMixChannels = self->mSubMixConnector.mNumSubMixChannels; // lbz +0x40

    if (!luNumSubMixChannels)
        return reinterpret_cast<int>(
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

    return reinterpret_cast<int>(lpResult);
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82BA3EF8 -- b DisconnectImmediate (tail call; `self` passes through).
// -------------------------------------------------------------------------------------
int Send::ReleaseEvent(Send* self)
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
// -------------------------------------------------------------------------------------
int Send::ConnectByPointerHandler(int cmd)
{
    SendConnectByPointerCommand* lpCmd = reinterpret_cast<SendConnectByPointerCommand*>(cmd);
    Send* lpSend = reinterpret_cast<Send*>(lpCmd->mTarget); // send = *(cmd+4)

    DisconnectImmediate(lpSend);

    SubMix* lpSubMix = reinterpret_cast<SubMix*>(lpCmd->mSubMix); // subMix = *(cmd+8)
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
    return 12;
}

// -------------------------------------------------------------------------------------
// ConnectByNameHandler @0x82B9FF80 -- BLOCKED, NOT reconstructed.
//
// The X360 body disconnects the send's connector then walks a GLOBAL by-name SubMix
// registry to find the SubMix whose name matches the queued command's name string, and
// links the connector into it:
//   send = *(cmd+4) ; DisconnectImmediate(send)
//   for (link = off_8327EE68; ...; ) {           // global list head
//       dword_8327EE00 = link->next;             // global iterator cursor (side-effect)
//       node = (SubMix*)((char*)link - 0x2C);     // intrusive link at SubMix+0x2C
//       if (link == (void*)0x2C) break;           // empty-list sentinel == address 0x2C
//       if (strcmp(node + 0x4C, cmd + 0xC) == 0) { ...link connector into node... }
//   }
//   return *(cmd+8)
//
// This is intentionally left undefined because it cannot be faithfully reconstructed from
// this TU alone WITHOUT fabricating un-groundable state:
//   1. off_8327EE68 (registry list head) and dword_8327EE00 (search iterator cursor) are
//      un-homed FOREIGN statics owned by a SubMix-registry TU not present here; the write
//      to dword_8327EE00 each iteration is a real side-effect I cannot restore to its
//      owning symbol.
//   2. The walk is an X360-offset-specific intrusive-list idiom: node = link - 0x2C, the
//      empty-list sentinel is the literal address 0x2C, and the name is compared at
//      node+0x4C -- none of which survive the host pointer widening. Reproducing them
//      needs SubMix's registry list-link and name members, which are NOT modelled (the
//      committed SubMix in SubMixConnector.h only reaches the connector fields), and
//      cannot be grounded from this function's asm without guessing the registry's home
//      layout. A wrong link offset / name position is worse than an honest gap.
// EventEvent still faithfully stores &Send::ConnectByNameHandler into the name-path
// command; only the handler body is deferred until the SubMix registry is homed.
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
// -------------------------------------------------------------------------------------
int Send::EventEvent(int self, int useName, u32* payload)
{
    System* lpSystem = reinterpret_cast<System*>(reinterpret_cast<Send*>(self)->mBase.mpSystem); // *(self+4)
    char* lpRingBase = lpSystem->mpDeferredRingBase;   // *(sys+0x20)
    u32 luCursor = lpSystem->muDeferredRingCursor;     // *(sys+0x10B8)

    if (useName)
    {
        // strlen of the queued name (payload[0] points at the source string).
        const char* lpName = reinterpret_cast<const char*>(payload[0]);
        u32 luLen = 0;
        while (lpName[luLen])
            ++luLen;

        // 12-byte header (handler / send / size) + name + NUL, rounded up to 4 bytes.
        const u32 luSize = (luLen + 16) & ~3u;

        u32* lpCmd = reinterpret_cast<u32*>(lpRingBase + luCursor);
        lpSystem->muDeferredRingCursor = luCursor + luSize;

        lpCmd[0] = reinterpret_cast<u32>(reinterpret_cast<void*>(&Send::ConnectByNameHandler));
        lpCmd[1] = static_cast<u32>(self);
        lpCmd[2] = luSize;

        char* lpDst = reinterpret_cast<char*>(lpCmd) + 12; // cmd + 0xC
        const char* lpSrc = reinterpret_cast<const char*>(payload[0]);
        char lch;
        do
        {
            lch = *lpSrc++;
            *lpDst++ = lch;
        } while (lch);
    }
    else
    {
        u32* lpCmd = reinterpret_cast<u32*>(lpRingBase + luCursor);
        lpSystem->muDeferredRingCursor = luCursor + 12;

        lpCmd[0] = reinterpret_cast<u32>(reinterpret_cast<void*>(&Send::ConnectByPointerHandler));
        lpCmd[1] = static_cast<u32>(self);
        lpCmd[2] = payload[0]; // target SubMix pointer (connect event word 0)
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
