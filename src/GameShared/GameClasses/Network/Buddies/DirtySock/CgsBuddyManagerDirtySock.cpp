#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.cpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"        // LobbyNameCmp
#include "GameShared/GameClasses/Network/CgsNetworkManager.h"  // NetworkManager::GetActiveControllerPort
#include "GameSource/GameState/BrnCgsPlayerName.h"

// =============================================================================
// CgsNetwork::BuddyManagerBase -- DirtySock buddy-list wrapper.
//
// Reconstructed from the X360 asm spine; the DirtySock HLB* API is declared in
// CgsBuddyApiDirtySock.h. Only the subset of methods whose X360 bodies are
// attested for this TU is defined here.
// =============================================================================

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.cpp".

namespace CgsNetwork
{
    using namespace CgsNetwork::DirtySock;

    // =========================================================================
    // Construct @ 0x8287D478
    // =========================================================================
    void BuddyManagerBase::Construct(NetworkManager* lpNetworkManager,
                                     ServerInterface* lpServerInterface)
    {
        CGS_ASSERT(lpNetworkManager,
            CGS_ASSERT_MESSAGE_AT("lpNetworkManager", __FILE__, 86 /* ARTIST 0x8287D4A8 */));
        CGS_ASSERT(lpServerInterface,
            CGS_ASSERT_MESSAGE_AT("lpServerInterface", __FILE__, 87 /* ARTIST 0x8287D4CC */));

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
                CGS_ASSERT(mpBuddies, CGS_ASSERT_MESSAGE_AT(
                    "mpBuddies",
                    __FILE__, 145 /* ARTIST 0x8288B604 */));

                HLBApiRegisterBuddyChangeCallback(mpBuddies, reinterpret_cast<void*>(&_BuddiesChangedCallback), this);
                HLBListSendChatMsg(mpBuddies, reinterpret_cast<void*>(&_MessageArrivedCallback), reinterpret_cast<s32>(this), 0, 0);
                HLBApiRegisterBuddyPresenceCallback(mpBuddies, reinterpret_cast<void*>(&_PresenceChangedCallback), this);

                if (mpBuddies)
                {
                    // &_SortBuddyFunction is overloaded (member + static); select the
                    // static 4-arg comparator the HLB API expects.
                    s32 (*lpfnSort)(void*, s32, void*, void*) = &BuddyManagerBase::_SortBuddyFunction;
                    HLBApiSetSortFunction(mpBuddies, reinterpret_cast<void*>(lpfnSort));
                }

                HLBApiSetUserIndex(mpBuddies, mpNetworkManager->GetActiveControllerPort());
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
    // GetNumFullBuddies
    //
    // Count the real (not temporary) buddies in the list.
    // =========================================================================
    s32 BuddyManagerBase::GetNumFullBuddies()
    {
        const s32 liBuddyCount = mpBuddies ? HLBListGetBuddyCount(mpBuddies) : 0;

        s32 liNumFullBuddies = 0;
        for (s32 liIndex = 0; liIndex < liBuddyCount; ++liIndex)
        {
            if (IsFullBuddy(liIndex))
            {
                ++liNumFullBuddies;
            }
        }
        return liNumFullBuddies;
    }

    // =========================================================================
    // GetBuddyName
    //
    // Copy the indexed buddy's name into lpOutName; false when there is no such
    // buddy.
    // =========================================================================
    bool BuddyManagerBase::GetBuddyName(s32 liIndex, PlayerName* lpOutName)
    {
        CGS_ASSERT(mpBuddies,
            CGS_ASSERT_MESSAGE_AT("mpBuddies", __FILE__, 399 /* ARTIST 0x8287D62C */));

        HLBBudT* lpBuddy = HLBListGetBuddyByIndex(mpBuddies, liIndex);
        if (lpBuddy == 0)
        {
            return false;
        }

        lpOutName->Construct(HLBBudGetName(lpBuddy));
        return true;
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
        CGS_ASSERT(mpBuddies,
            CGS_ASSERT_MESSAGE_AT("mpBuddies", __FILE__, 627 /* ARTIST 0x8287D7D0 */));
        CGS_ASSERT(lpBuddyName,
            CGS_ASSERT_MESSAGE_AT("lpBuddyName", __FILE__, 628 /* ARTIST 0x8287D7F4 */));

        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        return HLBBudIsJoinable(lpBuddy) == 1;
    }

    // =========================================================================
    // IsBuddyOnline @ 0x8287DB70
    // =========================================================================
    bool BuddyManagerBase::IsBuddyOnline(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName,
            CGS_ASSERT_MESSAGE_AT("lpBuddyName", __FILE__, 1196 /* ARTIST 0x8287DB9C */));

        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        CGS_ASSERT(lpBuddy,
            CGS_ASSERT_MESSAGE_AT("lpBuddy", __FILE__, 1199 /* ARTIST 0x8287DBD0 */));

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
        CGS_ASSERT(lpBuddyName, CGS_ASSERT_MESSAGE_AT(
            "lpBuddyName",
            __FILE__, 1006 /* ARTIST 0x8287DB1C */));

        HLBBudT* lpBuddy = HLBListGetBuddyByName(mpBuddies, lpBuddyName->GetPlayerName());
        CGS_ASSERT(lpBuddy, CGS_ASSERT_MESSAGE_AT(
            "lpBuddy",
            __FILE__, 1009 /* ARTIST 0x8287DB50 */));

        (void)lpBuddy;
        (void)lpcOut;
    }

    // =========================================================================
    // _SortBuddyFunction (static)
    //
    // The buddy-list comparator installed with the buddy API: orders two buddy
    // records by name.
    // =========================================================================
    s32 BuddyManagerBase::_SortBuddyFunction(void* /*lpUserData*/, s32 /*liSortContext*/,
                                             void* lpHLBBuddy1, void* lpHLBBuddy2)
    {
        CGS_ASSERT(lpHLBBuddy1, "lpHLBBuddy1");
        CGS_ASSERT(lpHLBBuddy2, "lpHLBBuddy2");

        const char* lpcName2 = HLBBudGetName(static_cast<HLBBudT*>(lpHLBBuddy2));
        const char* lpcName1 = HLBBudGetName(static_cast<HLBBudT*>(lpHLBBuddy1));
        return LobbyNameCmp(lpcName1, lpcName2);
    }

    // =========================================================================
    // GetAllBuddyNames @ 0x8287DD68
    //
    // Walks the buddy list, and for every real buddy writes its display-name
    // string into the caller's lpaNames array, returning the count written.
    // =========================================================================
    s32 BuddyManagerBase::GetAllBuddyNames(const char** lpaNames)
    {
        CGS_ASSERT(mpBuddies, CGS_ASSERT_MESSAGE_AT(
            "mpBuddies",
            CGS_ARTIST_FILE, 1350 /* ARTIST 0x8287DDA4 */));

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
                CGS_ASSERT(lpcBuddyName, CGS_ASSERT_MESSAGE_AT(
                    "lpcBuddyName",
                    CGS_ARTIST_FILE, 1366 /* ARTIST 0x8287DE20 */));

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
        CGS_ASSERT(lpBuddyName,
            CGS_ASSERT_MESSAGE_AT("lpBuddyName", __FILE__, 742 /* ARTIST 0x8288B6F8 */));

        s32 liResult = HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), reinterpret_cast<s32>(lpcMessage),
                                          reinterpret_cast<void*>(&_MessageSendingCallback), this);
        CGS_ASSERT(liResult == HLB_ERROR_NONE,
            CGS_ASSERT_MESSAGE_AT(
                "DirtySock::HLBListSendChatMsg( mpBuddies, lpBuddyName->GetPlayerName(), lpcMessage, &_MessageSendingCallback, this ) == HLB_ERROR_NONE",
                __FILE__, 744 /* ARTIST 0x8288B738 */));
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
        CGS_ASSERT(mpBuddies, CGS_ASSERT_MESSAGE_AT(
            "mpBuddies",
            CGS_ARTIST_FILE, 1250 /* ARTIST 0x8287DC38 */));
        HLBListSendChatMsg(mpBuddies, 0, 0, 0, 0);
    }

    // =========================================================================
    // InviteArrived @ 0x8287DA60 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteArrived(const PlayerName* lpPlayerName, s32 liInvitePayload)
    {
        CGS_ASSERT(lpPlayerName, CGS_ASSERT_MESSAGE_AT(
            "lpPlayerName",
            __FILE__, 916 /* ARTIST 0x8287DA98 */));
        CGS_ASSERT(lpPlayerName && lpPlayerName->GetPlayerName()[0] != '\0', CGS_ASSERT_MESSAGE_AT(
            "!lpPlayerName->IsEmpty()",
            __FILE__, 917 /* ARTIST 0x8287DAC0 */));

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
        CGS_ASSERT(lpBuddyName,
            CGS_ASSERT_MESSAGE_AT("lpBuddyName", __FILE__, 1417 /* ARTIST 0x8287DE74 */));
        CGS_ASSERT(mpBuddies,
            CGS_ASSERT_MESSAGE_AT("mpBuddies", __FILE__, 1418 /* ARTIST 0x8287DE9C */));

        HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), HLB_MSG_INVITE_REPLY, 0, 0);
    }

    // =========================================================================
    // InviteRevoked @ 0x8287DED0 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteRevoked(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName,
            CGS_ASSERT_MESSAGE_AT("lpBuddyName", __FILE__, 1437 /* ARTIST 0x8287DEFC */));
        CGS_ASSERT(mpBuddies,
            CGS_ASSERT_MESSAGE_AT("mpBuddies", __FILE__, 1438 /* ARTIST 0x8287DF24 */));

        HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), HLB_MSG_INVITE_REVOKE, 0, 0);
    }

    // =========================================================================
    // InviteDeclined @ 0x8287DF58 (virtual)
    // =========================================================================
    void BuddyManagerBase::InviteDeclined(const PlayerName* lpBuddyName)
    {
        CGS_ASSERT(lpBuddyName,
            CGS_ASSERT_MESSAGE_AT("lpBuddyName", __FILE__, 1455 /* ARTIST 0x8287DF84 */));
        CGS_ASSERT(mpBuddies,
            CGS_ASSERT_MESSAGE_AT("mpBuddies", __FILE__, 1456 /* ARTIST 0x8287DFAC */));

        HLBListSendChatMsg(mpBuddies, lpBuddyName->GetPlayerName(), HLB_MSG_INVITE_REPLY, 0, 0);
    }

    // =========================================================================
    // Message queries (virtual). The leaf vtable keeps these base bodies: the
    // buddy API holds no messages for this platform.
    // =========================================================================
    s32 BuddyManagerBase::GetTotalNumberOfMessages(const PlayerName* /*lpPlayerName*/)
    {
        return 0;
    }

    s32 BuddyManagerBase::GetNumberOfUnreadMessages(const PlayerName* /*lpPlayerName*/)
    {
        return 0;
    }

    // =========================================================================
    // Empty notification hooks (virtual): RefreshBuddyList, MessageArrived and
    // InviteSent do nothing in the base.
    // =========================================================================
    void BuddyManagerBase::RefreshBuddyList()
    {
    }

    void BuddyManagerBase::MessageArrived(const char* /*lpcMessage*/)
    {
    }

    void BuddyManagerBase::InviteSent(bool /*lbSuccess*/)
    {
    }

    // =========================================================================
    // MessageSent (virtual)
    //
    // A failed chat send asserts with the error code.
    // =========================================================================
    void BuddyManagerBase::MessageSent(bool lbSuccess, s32 /*liError*/)
    {
        CGS_ASSERT(lbSuccess,
                   "CgsNetwork::BuddyManagerBase::MessageSent" "FAILED TO SEND MESSAGE, error code: ");
    }

    // =========================================================================
    // _BuddiesChangedCallback @ 0x8287D830 (static)
    //
    // Forwards into the owning manager's BuddyListHasChanged (vtable slot +8).
    // =========================================================================
    void BuddyManagerBase::_BuddiesChangedCallback(HLBApiRefT* /*lpApi*/, s32 /*liArg2*/,
                                                   s32 /*liArg3*/, void* lpBuddyManager)
    {
        CGS_ASSERT(lpBuddyManager,
            CGS_ASSERT_MESSAGE_AT("lpBuddyManager", __FILE__, 672 /* ARTIST 0x8287D854 */));
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
        CGS_ASSERT(lpBuddyManager,
            CGS_ASSERT_MESSAGE_AT("lpBuddyManager", __FILE__, 695 /* ARTIST 0x8287D8BC */));
        static_cast<BuddyManagerBase*>(lpBuddyManager)->BuddyListHasChanged();
    }

    // =========================================================================
    // _MessageArrivedCallback @ 0x8287D980 (static)
    //
    // Forwards the inbound message text into MessageArrived (vtable slot +0x6C).
    // The asm reads the message text out of the BuddyApiMsgT at +0x14 (pData).
    // DWARF local lpBuddyManager (CgsBuddyManagerDirtySock.cpp:866).
    // =========================================================================
    void BuddyManagerBase::_MessageArrivedCallback(HLBApiRefT* /*lpRef*/,
                                                   CgsNetwork::DirtySock::BuddyApiMsgT* lpMsg,
                                                   void* lpData)
    {
        BuddyManagerBase* lpBuddyManager = static_cast<BuddyManagerBase*>(lpData);
        lpBuddyManager->MessageArrived(lpMsg->pData);
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
        CGS_ASSERT(lnOperation == HLB_OP_SEND_MSG,
            CGS_ASSERT_MESSAGE_AT(
                "lnOperation == DirtySock::HLB_OP_SEND_MSG", __FILE__, 786 /* ARTIST 0x8287D92C */));
        static_cast<BuddyManagerBase*>(lpBuddyManager)->MessageSent(liError == 0, liError);
    }
}

#undef CGS_ARTIST_FILE
