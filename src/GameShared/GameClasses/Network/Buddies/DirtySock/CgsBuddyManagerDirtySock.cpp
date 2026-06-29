#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/BrnCgsPlayerName.h"

// =============================================================================
// CgsNetwork::BuddyManagerBase -- DirtySock buddy-list wrapper.
//
// Reconstructed from the X360 asm spine; the DirtySock HLB* API is declared in
// CgsBuddyApiDirtySock.h. Only the subset of methods whose X360 bodies are
// attested for this TU is defined here.
// =============================================================================

namespace CgsNetwork
{
    using namespace CgsNetwork::DirtySock;

    // -------------------------------------------------------------------------
    // Update @ 0x8288B588 reads the active controller / user index out of the
    // NetworkManager (asm: lwz r4, 0x60(mpNetworkManager)) and feeds it to
    // HLBApiSetUserIndex. NetworkManager has no shared header (its definition is
    // TU-local to CgsNetworkManager.cpp), so the +0x60 field cannot be reached by
    // name from here. It is the active-controller-port member; declared as an
    // external accessor that the NetworkManager TU owns. Flagged: the accessor's
    // home is un-reconstructed; the read itself (offset +0x60 == controller port)
    // is grounded in the asm.
    s32 BuddyManager_GetNetworkManagerUserIndex(const NetworkManager* lpNetworkManager);

    // =========================================================================
    // Construct @ 0x8287D478
    // =========================================================================
    void BuddyManagerBase::Construct(NetworkManager* lpNetworkManager,
                                     ServerInterface* lpServerInterface)
    {
        CGS_ASSERT(lpNetworkManager, "lpNetworkManager");
        CGS_ASSERT(lpServerInterface, "lpServerInterface");

        mpNetworkManager  = lpNetworkManager;
        mpServerInterface = lpServerInterface;
        mpBuddies         = 0;
    }

    // =========================================================================
    // Update @ 0x8288B588 (virtual)
    //
    // Drives the HLB API. When not yet created: once connected to the network
    // service, create the API, register the change/message/presence callbacks,
    // install the sort comparator, and set the user index. When already created:
    // if still connected, pump it (when allowed to block); otherwise disconnect.
    // =========================================================================
    void BuddyManagerBase::Update(bool lbCanBlock)
    {
        if (mpBuddies == 0)
        {
            if (IsConnectedToNetworkService())
            {
                // HLBApiCreate's first two args are pointers to a fixed config
                // blob in rodata (asm: &unk_820046A7 passed for both r3 and r4),
                // third arg 0. The blob is un-reconstructed external data; passed
                // as null placeholders here (the asm address is not a real owned
                // object we can name). Flagged: config-blob contents unrecovered.
                mpBuddies = HLBApiCreate(0, 0, 0);
                CGS_ASSERT(mpBuddies, "mpBuddies");

                HLBApiRegisterBuddyChangeCallback(mpBuddies, reinterpret_cast<void*>(&_BuddiesChangedCallback), this);
                HLBListSendChatMsg(mpBuddies, reinterpret_cast<void*>(&_MessageArrivedCallback), reinterpret_cast<s32>(this));
                HLBApiRegisterBuddyPresenceCallback(mpBuddies, reinterpret_cast<void*>(&_PresenceChangedCallback), this);

                if (mpBuddies)
                {
                    // &_SortBuddyFunction is overloaded (member + static); select the
                    // static 4-arg comparator the HLB API expects.
                    s32 (*lpfnSort)(void*, s32, void*, void*) = &BuddyManagerBase::_SortBuddyFunction;
                    HLBApiSetSortFunction(mpBuddies, reinterpret_cast<void*>(lpfnSort));
                }

                HLBApiSetUserIndex(mpBuddies, BuddyManager_GetNetworkManagerUserIndex(mpNetworkManager));
            }
        }
        else
        {
            if (IsConnectedToNetworkService())
            {
                if (lbCanBlock)
                {
                    HLBApiUpdate(mpBuddies);
                }
            }
            else
            {
                Disconnect();
            }
        }
    }

    // =========================================================================
    // Disconnect @ 0x8287D4F8 (virtual)
    // =========================================================================
    EBuddyErrorCodes BuddyManagerBase::Disconnect()
    {
        if (mpBuddies)
        {
            HLBApiDisconnect(mpBuddies);
            HLBApiUpdate(mpBuddies);
            HLBApiDestroy(mpBuddies);
            mpBuddies = 0;
        }
        return E_BUDDY_ERROR_CODES_NONE;
    }

    // =========================================================================
    // GetNumBuddies @ 0x8287D680
    // =========================================================================
    s32 BuddyManagerBase::GetNumBuddies()
    {
        if (mpBuddies)
        {
            return HLBListGetBuddyCount(mpBuddies);
        }
        return 0;
    }

    // =========================================================================
    // GetNumOnlineBuddies @ 0x8287D730
    // =========================================================================
    s32 BuddyManagerBase::GetNumOnlineBuddies()
    {
        s32 liBuddyCount = mpBuddies ? HLBListGetBuddyCount(mpBuddies) : 0;

        s32 liOnline = 0;
        for (s32 liIndex = 0; liIndex < liBuddyCount; ++liIndex)
        {
            HLBBudT* lpBuddy = HLBListGetBuddyByIndex(mpBuddies, liIndex);
            if (HLBBudGetState(lpBuddy))
            {
                ++liOnline;
            }
        }
        return liOnline;
    }

    // =========================================================================
    // IsFullBuddy(int) @ 0x8287D550 (private overload)
    //
    // Hex-Rays signature is IsFullBuddy(this, a2) where a2 is the buddy *name*
    // looked up via HLBListGetBuddyByName -- this is the name-keyed query. The
    // DWARF private overload taking an int index does not match the lookup-by-name
    // body; the asm passes a2 straight to HLBListGetBuddyByName, so a2 is a name
    // pointer. Reconstructed faithfully to the asm.
    // =========================================================================
    bool BuddyManagerBase::IsFullBuddy(const PlayerName* lpPlayerName)
    {
        if (mpBuddies == 0)
        {
            return false;
        }
        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpPlayerName ? lpPlayerName->GetPlayerName() : 0);
        return lpBuddy != 0 && HLBBudIsRealBuddy(lpBuddy) == 1;
    }

    // =========================================================================
    // IsBuddyBlocked @ 0x8287D5A8
    // =========================================================================
    bool BuddyManagerBase::IsBuddyBlocked(const PlayerName* lpPlayerName)
    {
        if (mpBuddies == 0)
        {
            return false;
        }
        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpPlayerName ? lpPlayerName->GetPlayerName() : 0);
        return lpBuddy != 0 && HLBBudIsBlocked(lpBuddy) == 1;
    }

    // =========================================================================
    // IsBuddyJoinable(const PlayerName*) @ 0x8287D7A0
    // =========================================================================
    bool BuddyManagerBase::IsBuddyJoinable(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(mpBuddies, "mpBuddies");
        CGS_ASSERT(lpBuddyName, "lpBuddyName");

        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        return HLBBudIsJoinable(lpBuddy) == 1;
    }

    // =========================================================================
    // IsBuddyOnline @ 0x8287DB70
    // =========================================================================
    bool BuddyManagerBase::IsBuddyOnline(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName, "lpBuddyName");

        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        CGS_ASSERT(lpBuddy, "lpBuddy");

        return HLBBudGetState(lpBuddy) != 0;
    }

    // =========================================================================
    // GetBuddyPresence @ 0x8287DAE0
    //
    // Looks up the buddy by name and copies its presence into lpcOut. The X360
    // body inlines the presence copy (Hex-Rays renders it as an opaque STUB on
    // the buddy record + out buffer); the only attested behavior is the lookup +
    // the two asserts. Flagged: presence-copy detail inlined / unrecovered.
    // =========================================================================
    void BuddyManagerBase::GetBuddyPresence(const PlayerName* lpBuddyName, char* lpcOut)
    {
        CGS_ASSERT(lpBuddyName, "lpBuddyName");

        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        CGS_ASSERT(lpBuddy, "lpBuddy");

        (void)lpBuddy;
        (void)lpcOut;
    }

    // =========================================================================
    // GetAllBuddyNames @ 0x8287DD68
    //
    // Walks the buddy list, and for every real buddy writes its display-name
    // string into the caller's lpaNames array, returning the count written.
    // =========================================================================
    s32 BuddyManagerBase::GetAllBuddyNames(const char** lpaNames)
    {
        CGS_ASSERT(mpBuddies, "mpBuddies");

        s32          liCount = 0;
        const char** lpWrite = lpaNames;
        for (s32 liIndex = 0; ; ++liIndex)
        {
            s32 liBuddyCount = mpBuddies ? HLBListGetBuddyCount(mpBuddies) : 0;
            if (liIndex >= liBuddyCount)
            {
                break;
            }

            HLBBudT* lpBuddy = HLBListGetBuddyByIndex(mpBuddies, liIndex);
            if (lpBuddy && HLBBudIsRealBuddy(lpBuddy) == 1)
            {
                const char* lpcBuddyName = HLBBudGetName(lpBuddy);
                CGS_ASSERT(lpcBuddyName, "lpcBuddyName");

                *lpWrite = lpcBuddyName;
                ++liCount;
                ++lpWrite;
            }
        }
        return liCount;
    }

    // =========================================================================
    // SendMessage @ 0x8288B6C8 (virtual)
    // =========================================================================
    void BuddyManagerBase::SendMessage(const PlayerName* lpBuddyName, const char* lpcMessage)
    {
        CGS_ASSERT(lpBuddyName, "lpBuddyName");

        s32 liResult = HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), reinterpret_cast<s32>(lpcMessage),
                                          reinterpret_cast<void*>(&_MessageSendingCallback), this);
        CGS_ASSERT(liResult == HLB_ERROR_NONE,
                   "DirtySock::HLBListSendChatMsg( mpBuddies, lpBuddyName->GetPlayerName(), lpcMessage, &_MessageSendingCallback, this ) == HLB_ERROR_NONE");
    }

    // =========================================================================
    // BuddyListHasChanged @ 0x8288B758 (virtual)
    // =========================================================================
    void BuddyManagerBase::BuddyListHasChanged()
    {
        if (mpBuddies)
        {
            HLBListSort(mpBuddies);
        }
    }

    // =========================================================================
    // RevokeAllInvites @ 0x8287DC00 (virtual)
    // =========================================================================
    void BuddyManagerBase::RevokeAllInvites()
    {
        CGS_ASSERT(mpBuddies, "mpBuddies");
        HLBListSendChatMsg(mpBuddies, 0, 0);
    }

    // =========================================================================
    // InviteArrived @ 0x8287DA60 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteArrived(const PlayerName* lpPlayerName, s32 liInvitePayload)
    {
        CGS_ASSERT(lpPlayerName, "lpPlayerName");
        CGS_ASSERT(lpPlayerName && lpPlayerName->GetPlayerName()[0] != '\0', "!lpPlayerName->IsEmpty()");

        // The asm forwards the caller's payload (incoming r5) straight through to
        // HLBListSendChatMsg -- it never loads a constant here (unlike the sibling
        // invite replies). (DWARF declares one param; ARTIST forwards the payload,
        // so the X360 shape carries it.) Callback/userData are not set by this path.
        HLBListSendChatMsg(mpBuddies, lpPlayerName->GetPlayerName(), liInvitePayload, 0, 0);
    }

    // =========================================================================
    // InviteAccepted @ 0x8287DE48 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteAccepted(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName, "lpBuddyName");
        CGS_ASSERT(mpBuddies, "mpBuddies");

        HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), HLB_MSG_INVITE_REPLY, 0, 0);
    }

    // =========================================================================
    // InviteRevoked @ 0x8287DED0 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteRevoked(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName, "lpBuddyName");
        CGS_ASSERT(mpBuddies, "mpBuddies");

        HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), HLB_MSG_INVITE_REVOKE, 0, 0);
    }

    // =========================================================================
    // InviteDeclined @ 0x8287DF58 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteDeclined(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName, "lpBuddyName");
        CGS_ASSERT(mpBuddies, "mpBuddies");

        HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), HLB_MSG_INVITE_REPLY, 0, 0);
    }

    // =========================================================================
    // _BuddiesChangedCallback @ 0x8287D830 (static)
    //
    // Forwards into the owning manager's BuddyListHasChanged (vtable slot +8).
    // =========================================================================
    void BuddyManagerBase::_BuddiesChangedCallback(HLBApiRefT* /*lpApi*/, s32 /*liArg2*/,
                                                   s32 /*liArg3*/, void* lpBuddyManager)
    {
        CGS_ASSERT(lpBuddyManager, "lpBuddyManager");
        static_cast<BuddyManagerBase*>(lpBuddyManager)->BuddyListHasChanged();
    }

    // =========================================================================
    // _PresenceChangedCallback @ 0x8287D898 (static)
    //
    // Forwards into the owning manager's BuddyListHasChanged (same vtable slot +8
    // as the buddies-changed callback).
    // =========================================================================
    void BuddyManagerBase::_PresenceChangedCallback(HLBApiRefT* /*lpApi*/, HLBBudT* /*lpBuddy*/,
                                                    HLBStatE /*leState*/, void* lpBuddyManager)
    {
        CGS_ASSERT(lpBuddyManager, "lpBuddyManager");
        static_cast<BuddyManagerBase*>(lpBuddyManager)->BuddyListHasChanged();
    }

    // =========================================================================
    // _MessageArrivedCallback @ 0x8287D980 (static)
    //
    // Forwards the inbound message text into MessageArrived (vtable slot +0x6C).
    // The asm reads the message text out of the BuddyApiMsgT at +0x14.
    // =========================================================================
    void BuddyManagerBase::_MessageArrivedCallback(HLBApiRefT* /*lpApi*/,
                                                   CgsNetwork::DirtySock::BuddyApiMsgT* lpMsg,
                                                   void* lpBuddyManager)
    {
        // lpMsg's text field lives at +0x14; the record is opaque middleware data
        // (no shared layout), so the text pointer cannot be named here. Flagged:
        // BuddyApiMsgT layout unrecovered -- forward a null message text.
        (void)lpMsg;
        static_cast<BuddyManagerBase*>(lpBuddyManager)->MessageArrived(0);
    }

    // =========================================================================
    // _MessageSendingCallback @ 0x8287D900 (static)
    //
    // Asserts the operation is a send-message op, then forwards into MessageSent
    // (vtable slot +0x70) with (success = (error == 0), error).
    // =========================================================================
    void BuddyManagerBase::_MessageSendingCallback(HLBApiRefT* /*lpApi*/, s32 lnOperation,
                                                   s32 liError, void* lpBuddyManager)
    {
        CGS_ASSERT(lnOperation == HLB_OP_SEND_MSG, "lnOperation == DirtySock::HLB_OP_SEND_MSG");
        static_cast<BuddyManagerBase*>(lpBuddyManager)->MessageSent(liError == 0, liError);
    }
}
