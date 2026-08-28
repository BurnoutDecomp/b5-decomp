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

// off_82F8FBC8 -- the static "Route" plug-in run-time descriptor: a pointer to the
// type-name string. GetPlugInDescRunTime returns &off_82F8FBC8 (a char**).
static char* const skpRouteDescName = const_cast<char*>("Route");

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA3D40
//   if (a1) { *a1 = off_8217F524 ; *(a1+0x2C)=0 ; *(a1+0x30)=0 ; *(a1+0x34)=0 (stb) }
//   zero 6 dwords at a1+0x38 (the foldback gain array)  -- always, even when a1 == 0
//   return 1
// The +0x2C/+0x30/+0x34 stores clear the embedded connector's mField08 / mpSubMix /
// mbField10; the +0x38 loop clears maChannelGain[0..5].
// -------------------------------------------------------------------------------------
int Route::CreateInstance(int a1)
{
    if (a1)
    {
        Route* lpSelf = reinterpret_cast<Route*>(a1);
        lpSelf->mpVTable = skpRouteVTable;        // *a1 = off_8217F524
        lpSelf->mSubMixConnector.mpSubMixBuffer = 0;    // stw 0, 0x2C(a1)
        lpSelf->mSubMixConnector.mpSubMix = 0;          // stw 0, 0x30(a1)
        lpSelf->mSubMixConnector.mNumSubMixChannels = 0;// stb 0, 0x34(a1)

        // The X360 zeroes the 6-dword gain array (a1+0x38) AFTER the beq, i.e.
        // unconditionally; reproducing that literally through a null a1 would be a
        // host null-store. The only real caller always passes a valid buffer, so the
        // (semantically identical) guarded form is used.
        for (int li = 0; li < 6; ++li)
            lpSelf->mDeClickValue[li] = 0.0f;     // stw 0 -> +0x38 + 4*i
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
    return const_cast<char**>(&skpRouteDescName);
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
    System* lpSystem = self->mpSystem;                             // v3 = *(result+4)
    char* lpRingBase = lpSystem->mpDeferredRingBase;               // *(v3+0x20)

    u32 luCursor = lpSystem->muDeferredRingCursor;                 // *(v3+0x10B8)
    RouteConnectCommand* lpCmd =
        reinterpret_cast<RouteConnectCommand*>(lpRingBase + luCursor);
    // RECORD STRIDE (X360-literal trap): the asm's `addi r9,r9,0x18` IS the console
    // sizeof(RouteConnectCommand) ({int(*)(void*), Route*, SubMix*, f32 x3} = 4+4+4+4+4+4).
    // On the host the three widened pointers make the same record 40 bytes, and
    // ExecuteCommands advances the ring by the handler's return -- ConnectByPointerHandler
    // below likewise returns sizeof(RouteConnectCommand) -- so the enqueue stride must be
    // the HOST sizeof, never the literal 24. The cursor is advanced before the fields are
    // written, exactly as the asm orders the stores.
    lpSystem->muDeferredRingCursor =
        luCursor + static_cast<u32>(sizeof(RouteConnectCommand)); // X360: addi r9,r9,0x18 @0x82BA3DE0

    lpCmd->mpHandler = &Route::ConnectByPointerHandler; // cmd[0]
    lpCmd->mpTarget = self;                             // cmd[1]
    lpCmd->mpSubMix = pEvent->mpSubMix;                 // cmd[2] = event word 0
    lpCmd->mfGain0 = pEvent->mfGain0;                   // cmd[3] = event word 1
    lpCmd->mfGain1 = pEvent->mfGain1;                   // cmd[4] = event word 2
    lpCmd->mfGain2 = pEvent->mfGain2;                   // cmd[5] = event word 3
    return self;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82BA3D88
//   result = SubMixConnector::Disconnect(a1+0x24, a1+0x38)   ; fold gains back, unlink
//   zero 6 dwords at a1+0x38 (the gain array)
//   return result
// -------------------------------------------------------------------------------------
int Route::ReleaseEvent(int a1)
{
    Route* lpSelf = reinterpret_cast<Route*>(a1);
    SubMixConnector* lpResult =
        SubMixConnector::Disconnect(&lpSelf->mSubMixConnector, lpSelf->mDeClickValue); // r3 = a1+0x24, r4 = a1+0x38

    for (int li = 0; li < 6; ++li)
        lpSelf->mDeClickValue[li] = 0.0f; // stw 0 -> +0x38 + 4*i

    return static_cast<int>(reinterpret_cast<uintptr_t>(lpResult));
}

// -------------------------------------------------------------------------------------
// ConnectByPointerHandler @0x82B9FB38 -- replay a queued connect command (a1 = the
// command record; *(a1+4) = the Route instance).
//   v4 = SubMixConnector::Disconnect(route+0x24, route+0x38)   ; unlink + fold back
//   zero 6 dwords at route+0x38 (the gain array)
//   v6 = *(a1+8)                                               ; target SubMix
//   if (v6) {
//       v4->mpSubMix = v6                       ; stw v6, 0xC(v4)
//       v4->mField08 = v6->mField24             ; lwz 0x24(v6) -> stw 8(v4)
//       v4->mbField10 = (u8)v6->mbNumChannels   ; lbz 0x21(v6) -> stb 0x10(v4)
//       head = v6->mpConnectorHead              ; lwz 0x28(v6)
//       v4->mppPrev = 0                         ; stw 0, 4(v4)
//       v4->mpNext = head                       ; stw head, 0(v4)
//       if (head) head->mppPrev = (&v4)         ; stw v4, 4(head)   [stores v4 itself]
//       v6->mpConnectorHead = v4                ; stw v4, 0x28(v6)
//       route->mau8Gain[0] = (u8)(s64)*(a1+0xC) ; fctidz/stfd/lbz+7 -> stb 0x50
//       route->mau8Gain[1] = (u8)(s64)*(a1+0x10); stb 0x51
//       route->mau8Gain[2] = (u8)(s64)*(a1+0x14); stb 0x52
//   }
//   return 24
//
// NOTE the head back-link: the asm `stw r3,4(r10)` writes v4 (the connector pointer)
// into the old head's mppPrev slot. mppPrev is a SubMixConnector** ; the X360 stores the
// connector address there directly, so the reconstruction writes &v4's storage by taking
// the connector's own address (the value the asm stores is v4, i.e. &mConnector).
// -------------------------------------------------------------------------------------
int Route::ConnectByPointerHandler(void* cmd)
{
    RouteConnectCommand* lpCmd = static_cast<RouteConnectCommand*>(cmd);
    Route* lpRoute = lpCmd->mpTarget; // v6/r6 = *(a1+4)

    SubMixConnector* lpConn =
        SubMixConnector::Disconnect(&lpRoute->mSubMixConnector, lpRoute->mDeClickValue); // v4 = r3

    for (int li = 0; li < 6; ++li)
        lpRoute->mDeClickValue[li] = 0.0f; // zero 6 dwords at route+0x38

    SubMix* lpSubMix = lpCmd->mpSubMix; // v6 = *(a1+8)
    if (lpSubMix)
    {
        lpConn->mpSubMix = lpSubMix;                                   // stw 0xC(v4)
        lpConn->mpSubMixBuffer = lpSubMix->mpSubMixBuffer;             // lwz 0x24 -> stw 8
        lpConn->mNumSubMixChannels = lpSubMix->mOutputChannels;               // lbz 0x21 -> stb 0x10

        SubMixConnector* lpHead = lpSubMix->GetSendListHead();         // lwz 0x28(v6)
        lpConn->mppPrev = 0;                                           // stw 0, 4(v4)
        lpConn->mpNext = lpHead;                                       // stw head, 0(v4)
        if (lpHead)
            lpHead->mppPrev = reinterpret_cast<SubMixConnector**>(lpConn); // stw v4, 4(head)
        lpSubMix->SetSendListHead(lpConn);                             // stw v4, 0x28(v6)

        const f32 lfGain0 = lpCmd->mfGain0; // lfs 0xC(a1)
        const f32 lfGain1 = lpCmd->mfGain1; // lfs 0x10(a1)
        const f32 lfGain2 = lpCmd->mfGain2; // lfs 0x14(a1)
        lpRoute->mSourceStartChannel = static_cast<u8>(static_cast<long long>(lfGain0)); // stb 0x50
        lpRoute->mTargetStartChannel = static_cast<u8>(static_cast<long long>(lfGain1)); // stb 0x51
        lpRoute->mNumChannels        = static_cast<u8>(static_cast<long long>(lfGain2)); // stb 0x52
    }
    // RECORD STRIDE (X360-literal trap): the asm's `li r3,0x18` is the console
    // sizeof(RouteConnectCommand); it must stay identical to the producer's advance in
    // EventEvent above, so both sides use the HOST sizeof (40 on x64).
    return static_cast<int>(sizeof(RouteConnectCommand)); // X360: li r3,0x18 @0x82B9FBEC
}

// -------------------------------------------------------------------------------------
// ScalarDeletingDestructor @0x82BA1D10
//   *a1 = off_820AA810   ; revert to the PlugIn base vtable
//   if (a2 & 1) operator delete(a1)
//   return a1
// -------------------------------------------------------------------------------------
void* Route::ScalarDeletingDestructor(void* a1, char a2)
{
    *reinterpret_cast<void**>(a1) = skpRouteBaseVTable; // *a1 = off_820AA810
    if (a2 & 1)
        ::operator delete(a1);
    return a1;
}

} // namespace core
} // namespace audio
} // namespace rw
