#pragma once

#include "types.hpp"
#include "hlbudapi.h"   // HLBApiRefT, HLBBudT and the HLB* C API

// =============================================================================
// DirtySock HighLevelBuddy (HLB) API as the CGS buddy manager names it.
//
// The handle / record types and the HLB* entry points are the DirtySDK C API
// (hlbudapi.h, global scope, C linkage). The buddy manager spells them through
// CgsNetwork::DirtySock, so they are re-exported here by using-declarations:
// one type and one function per name, whichever spelling a caller uses.
//
// The console's chat / invite entry points all share one body that answers
// HLB_ERROR_UNSUPPORTED; the manager calls them through HLBListSendChatMsg.
// =============================================================================

namespace CgsNetwork
{
namespace DirtySock
{
    using ::HLBApiRefT;
    using ::HLBBudT;

    // Opaque inbound chat / invite message (buddyapi.h).
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

    // ---- API lifecycle -------------------------------------------------------
    using ::HLBApiCreate;
    using ::HLBApiDestroy;
    using ::HLBApiDisconnect;
    using ::HLBApiUpdate;
    using ::HLBApiSetUserIndex;

    // ---- Callback registration / sort comparator -----------------------------
    using ::HLBApiRegisterBuddyChangeCallback;
    using ::HLBApiRegisterBuddyPresenceCallback;
    using ::HLBApiSetSortFunction;

    // ---- List queries --------------------------------------------------------
    using ::HLBListGetBuddyCount;
    using ::HLBListGetBuddyByIndex;
    using ::HLBListGetBuddyByName;
    using ::HLBListSort;
    using ::HLBListSendChatMsg;

    // ---- Per-buddy queries ---------------------------------------------------
    using ::HLBBudIsRealBuddy;
    using ::HLBBudIsJoinable;
    using ::HLBBudGetState;
    using ::HLBBudGetName;
    using ::HLBBudIsBlocked;
    using ::HLBBudGetXenonXUID;
}
}
