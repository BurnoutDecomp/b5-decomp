#pragma once

#include "types.hpp"

// =============================================================================
// DirtySock HighLevelBuddy (HLB) API -- external/platform declarations.
//
// CgsBuddyManagerDirtySock.cpp wraps the DirtySock buddy library. The HLB*
// entry points are an external (middleware) C API; the DecFIGS DWARF homes the
// handle/record types under CgsNetwork::DirtySock (hlbudapi.h / buddyapi.h):
//
//   hlbudapi.h:209  HLBApiRefT   -- opaque buddy-API instance handle
//   hlbudapi.h:211  HLBBudT      -- opaque single-buddy record
//   hlbudapi.h:61   HLBStatE     -- buddy presence/online state enum
//   buddyapi.h:226  BuddyApiMsgT -- opaque inbound chat/invite message
//
// We model the handles as opaque structs (only ever held/passed by pointer) and
// declare the HLB* functions used by this TU. Bodies live in the middleware, so
// these are declarations only -- enough to satisfy the cl /c gate. Signatures
// are taken from the X360 call sites (register usage in the asm), not from the
// PPC Hex-Rays which drops args.
// =============================================================================

namespace CgsNetwork
{
namespace DirtySock
{
    // Opaque middleware handle/record types (hlbudapi.h / buddyapi.h).
    struct HLBApiRefT;
    struct HLBBudT;
    struct BuddyApiMsgT;

    // hlbudapi.h:61 -- buddy presence state. The wrappers only test it against 0
    // (offline) vs non-zero (online), so the concrete enumerators are not needed
    // by this TU; declared as a typedef of the enum tag for the callback signature.
    enum HLBStatE
    {
        E_HLBSTAT_OFFLINE = 0
    };

    // Chat-message kinds passed as the 3rd arg of HLBListSendChatMsg. The values
    // are the immediates stored at the X360 call sites (li r5,...):
    //   RevokeInvite/RevokeAllInvites store 1, Invite Accept/Decline store 2.
    // DirtySock's HLB_OP_SEND_MSG operation code is 7 (asserted in
    // _MessageSendingCallback: "lnOperation == DirtySock::HLB_OP_SEND_MSG").
    const s32 HLB_MSG_INVITE_REVOKE = 1;
    const s32 HLB_MSG_INVITE_REPLY  = 2;
    const s32 HLB_OP_SEND_MSG       = 7;

    // Result code: HLB_ERROR_NONE == 0 (SendMessage asserts the send returned 0).
    const s32 HLB_ERROR_NONE = 0;

    // ---- API lifecycle -------------------------------------------------------
    HLBApiRefT* HLBApiCreate(void* lpConfigA, void* lpConfigB, s32 liFlags);
    void        HLBApiDestroy(HLBApiRefT* lpApi);
    void        HLBApiDisconnect(HLBApiRefT* lpApi);
    void        HLBApiUpdate(HLBApiRefT* lpApi);
    void        HLBApiSetUserIndex(HLBApiRefT* lpApi, s32 liUserIndex);

    // ---- Callback registration ----------------------------------------------
    void HLBApiRegisterBuddyChangeCallback(HLBApiRefT* lpApi, void* lpCallback, void* lpUserData);
    void HLBApiRegisterBuddyPresenceCallback(HLBApiRefT* lpApi, void* lpCallback, void* lpUserData);

    // ---- List queries --------------------------------------------------------
    s32      HLBListGetBuddyCount(HLBApiRefT* lpApi);
    HLBBudT* HLBListGetBuddyByIndex(HLBApiRefT* lpApi, s32 liIndex);
    HLBBudT* HLBListGetBuddyByName(HLBApiRefT* lpApi, const char* lpcName);
    void     HLBListSort(HLBApiRefT* lpApi);

    // HLBListSendChatMsg's full DirtySock signature is 5 args:
    //   (api, name, payload, completionCallback, userData).
    // The X360 send paths (SendMessage, the invite replies) pass all five; the
    // message-arrived registration path passes only the first three. The trailing
    // two default to null so both call shapes compile.
    s32 HLBListSendChatMsg(HLBApiRefT* lpApi, const void* lpArg2, s32 liPayload,
                           void* lpCallback = nullptr, void* lpUserData = nullptr);

    // ---- Per-buddy queries ---------------------------------------------------
    s32 HLBBudIsRealBuddy(HLBBudT* lpBuddy);
    s32 HLBBudIsJoinable(HLBBudT* lpBuddy);
    s32 HLBBudGetState(HLBBudT* lpBuddy);

    // The following three are real DirtySock entry points whose symbols the PPC
    // Hex-Rays mis-resolved to unrelated modules (the disassembler matched the
    // branch target address to the wrong export). Their true behavior is fixed by
    // the asm call sites and the surrounding logic:
    //
    //   GetInternalString(HLBBudT*)  -> the buddy's display-name string
    //       (asm 0x8287DE00: arg = buddy ptr; result asserted non-null then stored
    //        as a name into the caller's char** out array). Modelled as
    //        HLBBudGetName.
    const char* HLBBudGetName(HLBBudT* lpBuddy);

    //   GetBestImpression()  -> blocked query on the just-fetched buddy
    //       (asm 0x8287D5CC: r3 still holds the HLBListGetBuddyByName result; result
    //        compared == 1). Modelled as HLBBudIsBlocked.
    s32 HLBBudIsBlocked(HLBBudT* lpBuddy);

    //   SetOnPressObject(api, fn)  -> install the buddy-list sort comparator
    //       (asm 0x8287D65C: arg2 = &_SortBuddyFunction). Modelled as
    //        HLBApiSetSortFunction.
    void HLBApiSetSortFunction(HLBApiRefT* lpApi, void* lpSortCallback);
}
}
