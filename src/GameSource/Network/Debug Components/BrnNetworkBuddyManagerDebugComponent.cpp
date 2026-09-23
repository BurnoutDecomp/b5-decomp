#include "GameSource/Network/Debug Components/BrnNetworkBuddyManagerDebugComponent.h"

#include <string.h>   // memcpy / strncpy (the X360 bodies call the CRT primitives directly)

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameSource/Network/Managers/BrnNetworkBuddyManagerBase.h"             // BuddyManagerBase (upload scratch, buddy queries)
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"     // ServerInterfaceCustomCommands::OverWriteServerFriendsRecord
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"  // ServerInterface::GetCustomCommandsComponent
#include "GameSource/Resource/BrnResourceAllocator.h"                           // BrnResource::GetDebugAllocator
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                      // NetworkOutBuddyCount / NetworkOutBuddyInformation
#include "GameShared/GameClasses/Development/Log/CgsLogChannelOutput.h"          // CgsDev::Log::LogChannelOutput
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // ServerInterfaceDirtySock::mNetStreamLogChannelOutput

// ===========================================================================
// BrnNetwork::BuddyManagerDebugComponent  --  GameSource/Network/Debug Components/
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX, gated against the DecFIGS DWARF declaration shape.
// This is the buddy-list debug-menu component: its action callbacks build a small network-IN
// event (a BrnNetworkModuleIO::NetworkEvent<N> leaf, BrnNetworkInEventTypeDefs.h) carrying the
// currently-selected buddy's PlayerName and push it onto the buddy manager's NetworkEventQueue.
//
// Every event-type tag and byte size below is X360-AUTHORITATIVE: they are the `li r5,<type>` and
// `li r6,<size>` immediates the AddEvent call sites pass, cross-checked against the DWARF
// NetworkEvent<N> leaves (BrnNetworkInEventTypeDefs.h):
//   N=1  count buddies            (size 1   command-only)
//   N=2  NetworkInGetBuddies      (size 1   command-only)
//   N=3  NetworkInGetChatMessage  (size 20  PlayerName + miMessageIndex)
//   N=4  NetworkInSendChatMessage (size 216 PlayerName + char[200])
//   N=5  NetworkInGetNextUnreadMessage (size 16  PlayerName)
//   N=7  NetworkInSendFeedback    (size 20  PlayerName + feedback type)
//   N=8  NetworkInSendInvite      (size 16  PlayerName)
//   N=10 NetworkInAcceptInvite    (size 16  PlayerName)
//   N=12 NetworkInJoinBuddy       (size 16  PlayerName)
// ===========================================================================

namespace BrnNetwork
{
    namespace
    {
        // Network-IN event-type tags (the X360 `li r5,<type>` immediates at the AddEvent sites; they
        // index the BrnNetworkModuleIO::NetworkEvent<N> leaf table in BrnNetworkInEventTypeDefs.h).
        const s32 KI_EVENT_GET_BUDDY_COUNT          = 1;
        const s32 KI_EVENT_GET_BUDDIES              = 2;
        const s32 KI_EVENT_GET_CHAT_MESSAGE         = 3;
        const s32 KI_EVENT_SEND_CHAT_MESSAGE        = 4;
        const s32 KI_EVENT_GET_NEXT_UNREAD_MESSAGE  = 5;
        const s32 KI_EVENT_SEND_FEEDBACK            = 7;
        const s32 KI_EVENT_SEND_INVITE              = 8;
        const s32 KI_EVENT_ACCEPT_INVITE            = 10;
        const s32 KI_EVENT_JOIN_BUDDY               = 12;

        // Event byte sizes (the X360 `li r6,<size>` immediates -- NOT sizeof, which would differ on the
        // x64 gate target once PlayerName / padding widen). These are the on-wire queue element sizes.
        const s32 KI_EVENT_SIZE_COMMAND             = 1;    // GetBuddyCount / GetBuddies (no payload)
        const s32 KI_EVENT_SIZE_PLAYER_NAME         = 16;   // a bare PlayerName
        const s32 KI_EVENT_SIZE_PLAYER_NAME_AND_INT = 20;   // PlayerName + one s32 (message idx / feedback)
        const s32 KI_EVENT_SIZE_CHAT                = 216;  // PlayerName + char[200] message
        const s32 KI_CHAT_MESSAGE_LENGTH            = 200;  // strncpy cap (the X360 `li r5,0xC8`)

        // The fixed test-message string strncpy'd into the send-chat event (X360 rodata at the
        // SendTestMessage site).
        const char* const KPC_TEST_MESSAGE = "This is a test message sent via the debug menu";
    }

    // -------- Construct  @ 0x82585648 --------
    // Reset the embedded base debug component, store the owning manager, clear the selection /
    // invite-buddy / queue members, then register this component with the debug menu.
    void BuddyManagerDebugComponent::Construct(BuddyManagerBase* lpBuddyManager)
    {
        DebugComponent::Destruct();   // X360: base reset (COMDAT-folded onto an empty Destruct thunk).

        mpBuddyManager  = lpBuddyManager;
        miBuddyIndex    = 0;
        miMessageIndex  = 0;
        miFeedbackIndex = 0;
        mInviteBuddy.macName[0] = '\0';   // X360 stb 0 at +0x2AC (PlayerName "empty" marker).
        mpEventQueue    = NULL;

        DebugComponent::Register();
    }

    // -------- Prepare  @ 0x825856A8 --------
    bool BuddyManagerDebugComponent::Prepare()
    {
        miBuddyIndex    = 0;
        miMessageIndex  = 0;
        miFeedbackIndex = 0;
        mInviteBuddy.macName[0] = '\0';
        mpEventQueue    = NULL;
        return true;
    }

    // -------- Release --------
    // Unregister the menu surface OnActivate registered, clear the selection / invite-buddy
    // members, and hand the event queue (when there is one) back to the debug allocator.
    bool BuddyManagerDebugComponent::Release()
    {
        UnregisterFunction(&BuddyManagerDebugComponent::ForceServerFriendsOverwrite, NULL);
        UnregisterFunction(&BuddyManagerDebugComponent::GetBuddyCount, NULL);
        UnregisterVariable(&miBuddyIndex);
        UnregisterFunction(&BuddyManagerDebugComponent::PrintBuddyInfo, NULL);
        UnregisterFunction(&BuddyManagerDebugComponent::PrintNextUnreadMessage, NULL);
        UnregisterFunction(&BuddyManagerDebugComponent::SendTestMessage, NULL);
        UnregisterFunction(&BuddyManagerDebugComponent::SendInvite, NULL);
        UnregisterVariable(&miMessageIndex);
        UnregisterFunction(&BuddyManagerDebugComponent::PrintMessage, NULL);
        UnregisterFunction(&BuddyManagerDebugComponent::LeaveFeedback, NULL);
        UnregisterVariable(&miFeedbackIndex);
        UnregisterFunction(&BuddyManagerDebugComponent::AcceptInvite, NULL);

        miBuddyIndex    = 0;
        miMessageIndex  = 0;
        miFeedbackIndex = 0;
        mInviteBuddy.macName[0] = '\0';

        if (mpEventQueue != NULL)
        {
            BrnResource::HeapResourceAllocator* lpAllocator = BrnResource::GetDebugAllocator();
            mpEventQueue->Release();
            mpEventQueue->Destruct();
            lpAllocator->Free(mpEventQueue);
            mpEventQueue = NULL;
        }

        return true;
    }

    // -------- Destruct  @ 0x825856D0 --------
    void BuddyManagerDebugComponent::Destruct()
    {
        miBuddyIndex    = 0;
        miMessageIndex  = 0;
        miFeedbackIndex = 0;
        mInviteBuddy.macName[0] = '\0';
        mpEventQueue    = NULL;
        mpBuddyManager  = NULL;

        DebugComponent::Destruct();   // X360 tail-call to the (folded) base Destruct.
    }

    // -------- GetName  @ 0x825856F0 --------
    const char* BuddyManagerDebugComponent::GetName() const
    {
        return "Buddies";
    }

    // -------- GetBuddyCount  @ 0x82594780 --------
    // Post a command-only "count buddies" event (no payload bytes are read; the X360 sends an
    // uninitialised 1-byte stack scratch). Event tag 1, size 1.
    void BuddyManagerDebugComponent::GetBuddyCount(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        // The X360 builds an 8-byte stack scratch and only forwards 1 byte; the payload is unused.
        u8 lScratch[8];
        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lScratch),
            KI_EVENT_GET_BUDDY_COUNT, KI_EVENT_SIZE_COMMAND);
    }

    // -------- PrintBuddyInfo  @ 0x825947B0 --------
    // Post a command-only "get buddies" event (NetworkEvent<2>). Event tag 2, size 1.
    void BuddyManagerDebugComponent::PrintBuddyInfo(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        u8 lScratch[8];
        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lScratch),
            KI_EVENT_GET_BUDDIES, KI_EVENT_SIZE_COMMAND);
    }

    // -------- PrintMessage  @ 0x825947E0 --------
    // Build a get-chat-message event (NetworkEvent<3>): the selected buddy's PlayerName followed by
    // the current message index. Event tag 3, size 20.
    void BuddyManagerDebugComponent::PrintMessage(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        struct
        {
            PlayerName mMessageSender;   // +0x00 (copied from the selected buddy)
            s32        miMessageIndex;   // +0x10
        } lEvent;

        lEvent.mMessageSender = lpBuddyDebug->mBuddyInformation[lpBuddyDebug->miBuddyIndex].mPlayerName;
        lEvent.miMessageIndex = lpBuddyDebug->miMessageIndex;

        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent),
            KI_EVENT_GET_CHAT_MESSAGE, KI_EVENT_SIZE_PLAYER_NAME_AND_INT);
    }

    // -------- PrintNextUnreadMessage  @ 0x82594840 --------
    // Build a get-next-unread-message event (NetworkEvent<5>): just the selected buddy's PlayerName.
    // Event tag 5, size 16.
    void BuddyManagerDebugComponent::PrintNextUnreadMessage(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        PlayerName lMessageSender =
            lpBuddyDebug->mBuddyInformation[lpBuddyDebug->miBuddyIndex].mPlayerName;

        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lMessageSender),
            KI_EVENT_GET_NEXT_UNREAD_MESSAGE, KI_EVENT_SIZE_PLAYER_NAME);
    }

    // -------- SendTestMessage  @ 0x82594898 --------
    // Build a send-chat-message event (NetworkEvent<4>): the selected buddy's PlayerName followed by
    // a fixed 200-byte test message. Event tag 4, size 216.
    void BuddyManagerDebugComponent::SendTestMessage(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        struct
        {
            PlayerName mBuddyToSendTo;                       // +0x00
            char       macMessageBuffer[KI_CHAT_MESSAGE_LENGTH]; // +0x10
        } lEvent;

        lEvent.mBuddyToSendTo = lpBuddyDebug->mBuddyInformation[lpBuddyDebug->miBuddyIndex].mPlayerName;
        strncpy(lEvent.macMessageBuffer, KPC_TEST_MESSAGE, KI_CHAT_MESSAGE_LENGTH);

        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent),
            KI_EVENT_SEND_CHAT_MESSAGE, KI_EVENT_SIZE_CHAT);
    }

    // -------- SendInvite  @ 0x82594908 --------
    // Build a send-invite event (NetworkEvent<8>): the selected buddy's PlayerName. Event tag 8, size 16.
    void BuddyManagerDebugComponent::SendInvite(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        PlayerName lBuddyToSendTo =
            lpBuddyDebug->mBuddyInformation[lpBuddyDebug->miBuddyIndex].mPlayerName;

        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lBuddyToSendTo),
            KI_EVENT_SEND_INVITE, KI_EVENT_SIZE_PLAYER_NAME);
    }

    // -------- AcceptInvite  @ 0x82594960 --------
    // Accept the pending game invite. Asserts a buddy has been set (mInviteBuddy not empty),
    // unregisters this action from the menu, then posts an accept-invite event (NetworkEvent<10>)
    // carrying mInviteBuddy. Event tag 10, size 16.
    void BuddyManagerDebugComponent::AcceptInvite(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        CGS_ASSERT(lpBuddyDebug->mInviteBuddy.macName[0] != '\0',
                   "AcceptInvite: no invite buddy has been set");

        // Remove this one-shot action from the menu (matches the X360 UnregisterFunction(this,
        // &AcceptInvite, NULL)).
        lpBuddyDebug->UnregisterFunction(&BuddyManagerDebugComponent::AcceptInvite, NULL);

        PlayerName lBuddyName = lpBuddyDebug->mInviteBuddy;
        lpBuddyDebug->mpEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lBuddyName),
            KI_EVENT_ACCEPT_INVITE, KI_EVENT_SIZE_PLAYER_NAME);
    }

    // -------- JoinBuddy  @ 0x825949F8 --------
    // Build a join-buddy event (NetworkEvent<12>): the selected buddy's PlayerName. Only posts it if
    // the manager's queue exists. Event tag 12, size 16.
    void BuddyManagerDebugComponent::JoinBuddy(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        PlayerName lBuddyToJoin =
            lpBuddyDebug->mBuddyInformation[lpBuddyDebug->miBuddyIndex].mPlayerName;

        if (lpBuddyDebug->mpEventQueue != NULL)
        {
            lpBuddyDebug->mpEventQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lBuddyToJoin),
                KI_EVENT_JOIN_BUDDY, KI_EVENT_SIZE_PLAYER_NAME);
        }
    }

    // -------- LeaveFeedback  @ 0x82594A58 --------
    // Build a send-feedback event (NetworkEvent<7>): the selected buddy's PlayerName followed by the
    // current feedback-type selection. Only posts it if the queue exists. Event tag 7, size 20.
    void BuddyManagerDebugComponent::LeaveFeedback(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);

        struct
        {
            PlayerName mPlayerName;      // +0x00
            s32        miFeedbackType;   // +0x10  (CgsNetwork::EFeedbackType, stored as raw s32)
        } lEvent;

        lEvent.mPlayerName    = lpBuddyDebug->mBuddyInformation[lpBuddyDebug->miBuddyIndex].mPlayerName;
        lEvent.miFeedbackType = lpBuddyDebug->miFeedbackIndex;

        if (lpBuddyDebug->mpEventQueue != NULL)
        {
            lpBuddyDebug->mpEventQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent),
                KI_EVENT_SEND_FEEDBACK, KI_EVENT_SIZE_PLAYER_NAME_AND_INT);
        }
    }

    // -------- ForceServerFriendsOverwrite  @ 0x82591B78 --------
    // Snapshot the manager's full buddies into its upload scratch (maBuddyListAtUpload /
    // miNumBuddiesAtUpload) and overwrite the server friends record with them through the
    // custom-commands component.
    void BuddyManagerDebugComponent::ForceServerFriendsOverwrite(void* lpData)
    {
        BuddyManagerDebugComponent* lpBuddyDebug = static_cast<BuddyManagerDebugComponent*>(lpData);
        CGS_ASSERT(lpBuddyDebug, "lpBuddyDebug");

        BuddyManagerBase* lpBuddyManager = lpBuddyDebug->mpBuddyManager;
        CGS_ASSERT(lpBuddyManager, "lpBuddyManager");
        CGS_ASSERT(lpBuddyManager->mpServerInterface->GetCustomCommandsComponent(),
                   "lpBuddyManager->mpServerInterface->GetCustomCommandsComponent()");

        ServerInterfaceCustomCommands* lpCustomCommandsComponent = static_cast<ServerInterfaceCustomCommands*>(
            lpBuddyManager->mpServerInterface->GetCustomCommandsComponent());
        CGS_ASSERT(lpCustomCommandsComponent, "lpCustomCommandsComponent");

        lpBuddyManager->miNumBuddiesAtUpload = 0;
        for (s32 liBuddyIndex = 0; liBuddyIndex < lpBuddyManager->GetNumBuddies(); ++liBuddyIndex)
        {
            const bool lbGotName = lpBuddyManager->GetBuddyName(
                liBuddyIndex, &lpBuddyManager->maBuddyListAtUpload[lpBuddyManager->miNumBuddiesAtUpload]);
            CGS_ASSERT(lbGotName,
                       "lpBuddyManager->GetBuddyName( liBuddyIndex, &lpBuddyManager->maBuddyListAtUpload[lpBuddyManager->miNumBuddiesAtUpload] )");

            if (lpBuddyManager->IsFullBuddy(&lpBuddyManager->maBuddyListAtUpload[lpBuddyManager->miNumBuddiesAtUpload]))
            {
                ++lpBuddyManager->miNumBuddiesAtUpload;
            }
        }

        lpCustomCommandsComponent->OverWriteServerFriendsRecord(lpBuddyManager->maBuddyListAtUpload,
                                                                lpBuddyManager->miNumBuddiesAtUpload);
    }

    // Print the buddy manager's outgoing events to the network log stream.
    void BuddyManagerDebugComponent::ProcessOutgoingEvents(BrnNetworkModuleIO::NetworkEventQueue* lpEventQueue)
    {
        CgsDev::Log::LogChannelOutput& lrLog = CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput;

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventType = lpEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent != NULL)
        {
            const u8* lpcEvent = reinterpret_cast<const u8*>(lpEvent);

            switch (liEventType)
            {
            case 1:
            {
                const BrnNetworkModuleIO::NetworkOutBuddyCount* lpCount =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkOutBuddyCount*>(lpEvent);
                lrLog << "We have " << lpCount->miBuddyCount << " buddies!\n";
                break;
            }

            case 2:
            {
                const BrnNetworkModuleIO::NetworkOutBuddyInformation* lpInfo =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkOutBuddyInformation*>(lpEvent);

                for (s32 liBuddy = 0; liBuddy < lpInfo->miNumberOfBuddiesInEvent; ++liBuddy)
                {
                    const BrnNetworkModuleIO::BuddyInformation& lrBuddy = lpInfo->mBuddyInformation[liBuddy];

                    lrLog << "Buddy Name: " << lrBuddy.mPlayerName.macName << "\n";
                    if (lrBuddy.mbIsFullBuddy)
                    {
                        lrLog << "Buddy Presence: " << lrBuddy.macPresenceData << "\n";
                        lrLog << "Can you join buddy: " << (lrBuddy.mbIsJoinable ? "Yes" : "No") << "\n";
                        lrLog << "Messages " << lrBuddy.miUnreadMessages << " of which "
                              << lrBuddy.miTotalMessages << " are unread.\n";
                        if (lrBuddy.miInviteStatus == 2)
                        {
                            lrLog << "You have been invited to his game\n";
                        }
                        else if (lrBuddy.miInviteStatus == 1)
                        {
                            lrLog << "You invited him to your game!\n";
                        }
                    }
                    else
                    {
                        lrLog << "Buddy Request pending\n";
                    }

                    memcpy(&mBuddyInformation[liBuddy], &lrBuddy, sizeof(lrBuddy));
                }

                miBuddyIndex = 0;
                SetRange(&miBuddyIndex, 0, lpInfo->miNumberOfBuddiesInEvent);
                break;
            }

            case 3:
            case 5:
                lrLog << "Buddy: " << reinterpret_cast<const char*>(lpcEvent + 0x04)
                      << " Message index: " << *reinterpret_cast<const s32*>(lpcEvent)
                      << " Message: " << reinterpret_cast<const char*>(lpcEvent + 0x14) << "\n";
                break;

            case 4:
                lrLog << "New Message Arrived.\n";
                break;

            case 7:
                lrLog << "Invite Arrived.\n";
                break;

            default:
                break;
            }

            liEventType = lpEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }
}
