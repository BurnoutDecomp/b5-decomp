#include "GameSource/Network/Managers/BrnNetworkBuddyManagerBase.h"

#include <cstdlib>   // qsort
#include <cstring>   // memcpy, memset
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                  // CgsCore::SPrintf
#include "GameSource/GameState/BrnCgsPlayerName.h"                                       // CgsNetwork::PlayerName
#include "GameSource/Network/BrnServerInterface.h"                                       // BrnNetwork::BrnServerInterface
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"  // IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"       // IsLocalPlayerInGame
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

// X360 LobbyNameCmp -- compares two 16-byte player-name strings (returns 0 when
// they name the same player, <0/>0 for ordering). It is a free function with no
// committed header home yet (the sibling BrnChallengeHighScoreEntry.cpp declares
// it file-locally the same way); declared here at global scope so this TU links
// against the one X360 definition.
int LobbyNameCmp(const char* lpcNameA, const char* lpcNameB);

// ===========================================================================
// BrnNetwork::BuddyManagerBase -- game-side buddy manager. See the header for
// the full description + member-offset map. Reconstructed from the X360 ARTIST
// asm spine; the DecFIGS DWARF supplied the declaration shape.
//
// The buddy lists are CgsNetwork::PlayerName (the type the base buddy queries
// take/return); the (type, byteSize) AddEvent immediates are X360-authoritative.
// ===========================================================================

namespace BrnNetwork
{
    // BrnNetworkBuddyManagerBase.cpp:24 -- delay (seconds) before the manager
    // acts on a detected buddy-list change. The asm stores this float into
    // mfBuddyListChangedTimer (a1[21425]) when a change is already pending.
    const f32 KF_DELAY_HANDLING_BUDDY_CHANGE = 10.0f;

    namespace
    {
        // The "retry get server buddies" hold-off (seconds) the buddy manager arms
        // mfTimeUntilRetryGetServerBuddies with when BuddyListHasChanged fires while
        // offline. X360 BuddyListHasChanged stores 210.0 (0x43520000) into the timer
        // when it currently reads the -1.0 sentinel.
        const f32 KF_RETRY_GET_SERVER_BUDDIES = 210.0f;

        // ---- network IN-event payloads posted to the module event queue -----
        // These are the buddy IN-events the network module dispatches. Each one is
        // posted through NetworkEventQueue::AddEvent(event, type, byteSize); the
        // (type, byteSize) pairs are X360-authoritative immediates. Only the fields
        // the asm stores are named; unknown spans are reserved as pads so the byte
        // size posted to the queue stays exact.

        // type 9, size 24. Two flavours share the 24-byte slot (built on overlapping
        // stack storage in the asm):
        //   - single buddy came online (miNotificationType==1, carries the name)
        //   - several buddies came online summary (miNotificationType==2, count set)
        struct BuddyOnlineNotificationEvent
        {
            s32                    miNotificationType;   // +0x00 (1 == single, 2 == summary)
            s32                    miCount;              // +0x04 (single: 1; summary: GetNumOnlineBuddies)
            CgsNetwork::PlayerName mBuddyName;           // +0x08 (16B -> 24 total)
        };

        // type 14, size 16 -- a buddy went offline / dropped; carries its name.
        struct BuddyOfflineNotificationEvent
        {
            CgsNetwork::PlayerName mBuddyName;           // +0x00 (16B)
        };

        // type 10, size 1 -- "buddy list updated" poke (no payload past one byte).
        struct BuddyListUpdatedEvent
        {
            u8 muUnused;                                 // +0x00
        };

        // type 1, size 4 -- buddy-count reply (debug GetBuddyCount).
        struct BuddyCountEvent
        {
            s32 miNumBuddies;                            // +0x00
        };

        // The per-buddy record carried in the type-2 batch event. Mirrors the 132-byte
        // BuddyInformation laid out in BrnNetworkBuddyManagerDebugComponent.h (stride
        // 0x84; mPlayerName at +0x0C). Only the fields SendBuddyInformation stores are
        // named.
        struct BuddyInformationRecord
        {
            s32                    miInviteType;         // +0x00 ((*vt+60)(this,name))
            s32                    miInviteSubType;      // +0x04 ((*vt+64)(this,name) / 2)
            s32                    miInviteStatus;       // +0x08
            CgsNetwork::PlayerName mPlayerName;          // +0x0C (16B; the GetBuddyName slot)
            bool                   mbIsFullBuddy;        // +0x1C
            bool                   mbIsOnline;           // +0x1D
            bool                   mbIsInSameLobby;      // +0x1E (asm stores 0)
            bool                   mbIsJoinable;         // +0x1F
            char                   macPresenceData[100]; // +0x20 .. +0x84
        };

        // type 2, size 672 -- the buddy-information batch event: up to five
        // BuddyInformationRecord plus a trailing "is-last-batch" flag. Total size is the
        // X360-authoritative 672 (12 + 5*132 == 672).
        struct BuddyInformationEvent
        {
            s32                    miNumBuddiesInBatch;     // +0x000
            s32                    miStartIndex;            // +0x004
            s32                    miPad;                   // +0x008 (record[0] is 16-aligned @ +0x0C)
            BuddyInformationRecord maBuddyInformation[5];   // +0x00C .. +0x294
            s32                    miUnusedA;               // +0x294
            s32                    miUnusedB;               // +0x298
            bool                   mbIsLastBatch;           // +0x29C
        };
    }

    // -- the server interface, viewed as the concrete game-side aggregate (the
    //    base mpServerInterface IS a BrnServerInterface; the X360 reaches its
    //    Brn getters through the derived type).
    static BrnServerInterface* AsBrnServerInterface(CgsNetwork::ServerInterface* lpServerInterface)
    {
        return static_cast<BrnServerInterface*>(lpServerInterface);
    }

    // -- the network module's IN-event queue typed as the concrete instantiation
    //    it actually is (the module's accessor returns its forward-declared
    //    NetworkEventQueue; the driven queue is CgsModule::VariableEventQueue<14000,16>).
    static NetworkEventQueue* GetEventQueue(BrnNetworkModule* lpNetworkModule)
    {
        return reinterpret_cast<NetworkEventQueue*>(lpNetworkModule->GetNetworkEventQueue());
    }

    // =======================================================================
    // Construct @ 0x82553FD0
    // =======================================================================
    void BuddyManagerBase::Construct(BrnNetworkModule* lpNetworkModule,
                                     BrnServerInterface* lpServerInterface)
    {
        CgsNetwork::BuddyManagerX360::Construct(
            reinterpret_cast<CgsNetwork::NetworkManager*>(lpNetworkModule->GetNetworkManager()), lpServerInterface);

        mpNetworkModule = lpNetworkModule;
        mDebugComponent.Construct(this);

        for (s32 li = 0; li < KI_MAX_BUDDIES; ++li)
        {
            maServerBuddyList[li].macName[0] = 0;
        }

        meState = E_BUDDY_MANAGER_STATE_START;
        mTimeUntilRetryAfterFailedBuddyUpload.SetFloatVal(0.0f);

        miNextBuddyToSend = 100;
        mbOverwriteFriendsList = false;

        for (s32 li = 0; li < KI_MAX_BUDDIES; ++li)
        {
            maCachedBuddyListAtLastChange[li].macName[0] = 0;
        }

        miCachedNumBuddiesAtLastChange = 0;
        mfTimeUntilRetryGetServerBuddies = -1.0f;
        mfBuddyListChangedTimer = -1.0f;
    }

    // =======================================================================
    // Destruct @ 0x825540D0
    // =======================================================================
    void BuddyManagerBase::Destruct()
    {
        mfTimeUntilRetryGetServerBuddies = -1.0f;
        mfBuddyListChangedTimer = -1.0f;

        for (s32 li = 0; li < KI_MAX_BUDDIES; ++li)
        {
            maCachedBuddyListAtLastChange[li].macName[0] = 0;
        }

        miCachedNumBuddiesAtLastChange = 0;
        mTimeUntilRetryAfterFailedBuddyUpload.SetFloatVal(0.0f);
        mbOverwriteFriendsList = false;
        meState = E_BUDDY_MANAGER_STATE_START;
        mDebugComponent.Destruct();

        for (s32 li = 0; li < KI_MAX_BUDDIES; ++li)
        {
            maServerBuddyList[li].macName[0] = 0;
        }

        mpNetworkModule = NULL;
        miNextBuddyToSend = 100;

        CgsNetwork::BuddyManagerX360::Destruct();
    }

    // =======================================================================
    // Disconnect @ 0x825541D0
    // =======================================================================
    CgsNetwork::EBuddyErrorCodes BuddyManagerBase::Disconnect()
    {
        mbOverwriteFriendsList = false;

        for (s32 li = 0; li < KI_MAX_BUDDIES; ++li)
        {
            maServerBuddyList[li].macName[0] = 0;
        }
        for (s32 li = 0; li < KI_MAX_BUDDIES; ++li)
        {
            maCachedBuddyListAtLastChange[li].macName[0] = 0;
        }

        miCachedNumBuddiesAtLastChange = 0;
        meState = E_BUDDY_MANAGER_STATE_START;

        return CgsNetwork::BuddyManagerBase::Disconnect();
    }

    // =======================================================================
    // Prepare @ 0x825499B8
    // =======================================================================
    bool BuddyManagerBase::Prepare()
    {
        if (!CgsNetwork::BuddyManagerX360::Prepare())
        {
            return false;
        }

        mbOverwriteFriendsList = false;
        mDebugComponent.Prepare();
        return true;
    }

    // =======================================================================
    // Release @ 0x82549A28
    // =======================================================================
    bool BuddyManagerBase::Release()
    {
        if (!CgsNetwork::BuddyManagerX360::Release())
        {
            return false;
        }

        mbOverwriteFriendsList = false;
        mDebugComponent.Release();
        return true;
    }

    // =======================================================================
    // GetServerFriends @ 0x82554240
    // =======================================================================
    void BuddyManagerBase::GetServerFriends()
    {
        BrnServerInterface* lpServerInterface = AsBrnServerInterface(mpServerInterface);

        CGS_ASSERT(lpServerInterface, "mpServerInterface");
        CGS_ASSERT(lpServerInterface->GetConnectionComponent(), "mpServerInterface->GetConnectionComponent()");

        if (!lpServerInterface->GetConnectionComponent()->IsLoggedIn())
        {
            return;
        }

        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS)
            != BrnServerInterface::E_STATUS_IDLE)
        {
            return;
        }

        CGS_ASSERT(lpServerInterface->GetCustomCommandsComponent(), "mpServerInterface->GetCustomCommandsComponent()");
        ServerInterfaceCustomCommands* lpCustomCommandsComponent = lpServerInterface->GetCustomCommandsComponent();
        CGS_ASSERT(lpCustomCommandsComponent, "lpCustomCommandsComponent");

        mbOverwriteFriendsList = false;
        lpCustomCommandsComponent->GetFriendsListFromServer(_GetFriendsListFromServerComplete, this);
        meState = E_BUDDY_MANAGER_STATE_GETTING_SERVER_BUDDIES;
    }

    // =======================================================================
    // IsLocalPlayersBuddyListAlphabetical @ 0x82549B70
    // =======================================================================
    bool BuddyManagerBase::IsLocalPlayersBuddyListAlphabetical()
    {
        for (s32 liBuddyIndex = 0; liBuddyIndex < GetNumBuddies() - 1; ++liBuddyIndex)
        {
            CgsNetwork::PlayerName lPlayerName1;
            CgsNetwork::PlayerName lPlayerName2;

            CGS_ASSERT(GetBuddyName(liBuddyIndex, &lPlayerName1), "GetBuddyName( liBuddyIndex, &lPlayerName1)");
            CGS_ASSERT(GetBuddyName(liBuddyIndex + 1, &lPlayerName2), "GetBuddyName( liBuddyIndex + 1, &lPlayerName2)");

            if (LobbyNameCmp(lPlayerName1.macName, lPlayerName2.macName) >= 0)
            {
                return false;
            }
        }

        return true;
    }

    // =======================================================================
    // GetBuddiesToAddAndRemoveFromServer @ 0x82554370
    //
    // Diffs the (alphabetically sorted) server buddy list against the live buddy
    // list, producing the names to add to / remove from the server. Sets
    // *lpbOverwriteRequired if the live list is corrupt (leading 0x22 marker).
    // =======================================================================
    void BuddyManagerBase::GetBuddiesToAddAndRemoveFromServer(CgsNetwork::PlayerName* lpaPlayerNamesToAdd,
                                                              CgsNetwork::PlayerName* lpaPlayerNamesToRemove,
                                                              s32* lpiNumToAdd, s32* lpiNumToRemove,
                                                              bool* lpbOverwriteRequired)
    {
        s32 liNumToRemove = 0;
        s32 liNumToAdd = 0;

        miNumBuddiesAtUpload = 0;

        CGS_ASSERT(IsLocalPlayersBuddyListAlphabetical(), "IsLocalPlayersBuddyListAlphabetical()");

        for (s32 liBuildIndex = 0; liBuildIndex < GetNumBuddies(); ++liBuildIndex)
        {
            CGS_ASSERT(GetBuddyName(liBuildIndex, &maBuddyListAtUpload[miNumBuddiesAtUpload]),
                       "GetBuddyName( liBuddyIndex, &maBuddyListAtUpload[miNumBuddiesAtUpload] )");

            if (IsFullBuddy(&maBuddyListAtUpload[miNumBuddiesAtUpload]))
            {
                ++miNumBuddiesAtUpload;
            }
        }

        qsort(maServerBuddyList, miNumServerBuddies, sizeof(CgsNetwork::PlayerName), _SortPlayerNames);

        s32 liBuddyIndex = 0;
        s32 liServerBuddyIndex = 0;

        if (miNumServerBuddies > 0)
        {
            while (liBuddyIndex < miNumBuddiesAtUpload)
            {
                CGS_ASSERT(liServerBuddyIndex < miNumServerBuddies, "liServerBuddyIndex < miNumServerBuddies");
                CGS_ASSERT(liServerBuddyIndex < KI_MAX_BUDDIES, "liServerBuddyIndex < CgsNetwork::KI_MAX_BUDDIES");
                CGS_ASSERT(liBuddyIndex < GetNumBuddies(), "liBuddyIndex < GetNumBuddies()");
                CGS_ASSERT(liBuddyIndex < KI_MAX_BUDDIES, "liBuddyIndex < CgsNetwork::KI_MAX_BUDDIES");

                CgsNetwork::PlayerName lServerBuddyName = maServerBuddyList[liServerBuddyIndex];
                CgsNetwork::PlayerName lBuddyName       = maBuddyListAtUpload[liBuddyIndex];

                if (lServerBuddyName.macName[0] == 0x22)
                {
                    *lpbOverwriteRequired = true;
                    return;
                }

                if (LobbyNameCmp(lServerBuddyName.macName, lBuddyName.macName) == 0)
                {
                    // identical -- present on both sides, keep.
                    ++liServerBuddyIndex;
                    ++liBuddyIndex;
                    continue;
                }

                if (!IsFullBuddy(&lServerBuddyName))
                {
                    // server-side entry that is no longer a full buddy -> remove it.
                    lpaPlayerNamesToRemove[liNumToRemove].Construct(lServerBuddyName.macName);
                    ++liNumToRemove;
                    ++liServerBuddyIndex;
                    continue;
                }

                if (LobbyNameCmp(lBuddyName.macName, lServerBuddyName.macName) < 0)
                {
                    // live list has a name the server lacks -> add it.
                    lpaPlayerNamesToAdd[liNumToAdd].Construct(lBuddyName.macName);
                    ++liNumToAdd;
                    ++liBuddyIndex;
                    continue;
                }

                // the server buddy is missing from the live list -- verify it is a
                // genuine duplicate before recording it for removal.
                bool lbFoundDuplicate = false;
                if (liBuddyIndex > 0)
                {
                    for (s32 li = 0; li < liBuddyIndex; ++li)
                    {
                        if (LobbyNameCmp(maBuddyListAtUpload[li].macName, lServerBuddyName.macName) == 0)
                        {
                            lbFoundDuplicate = true;
                            break;
                        }
                    }
                }

                CGS_ASSERT(lbFoundDuplicate, "Suspected duplicate server buddy but didn't find one\n");

                lpaPlayerNamesToRemove[liNumToRemove].Construct(lServerBuddyName.macName);
                ++liNumToRemove;
                ++liServerBuddyIndex;
            }
        }

        // remaining server buddies (past the live list) -> all removals.
        while (liServerBuddyIndex < miNumServerBuddies)
        {
            CgsNetwork::PlayerName lServerBuddyName = maServerBuddyList[liServerBuddyIndex];
            lpaPlayerNamesToRemove[liNumToRemove].Construct(lServerBuddyName.macName);
            ++liServerBuddyIndex;
            ++liNumToRemove;
        }

        // remaining live buddies (past the server list) -> all additions.
        while (liBuddyIndex < miNumBuddiesAtUpload)
        {
            CgsNetwork::PlayerName lBuddyName = maBuddyListAtUpload[liBuddyIndex];
            lpaPlayerNamesToAdd[liNumToAdd].Construct(lBuddyName.macName);
            ++liBuddyIndex;
            ++liNumToAdd;
        }

        *lpiNumToAdd = liNumToAdd;
        *lpiNumToRemove = liNumToRemove;
    }

    // =======================================================================
    // UploadServerFriends @ 0x8255C418
    // =======================================================================
    void BuddyManagerBase::UploadServerFriends()
    {
        BrnServerInterface* lpServerInterface = AsBrnServerInterface(mpServerInterface);

        CGS_ASSERT(lpServerInterface, "mpServerInterface");
        CGS_ASSERT(lpServerInterface->GetConnectionComponent(), "mpServerInterface->GetConnectionComponent()");

        if (!lpServerInterface->GetConnectionComponent()->IsLoggedIn())
        {
            return;
        }

        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_CUSTOM_COMMANDS)
            != BrnServerInterface::E_STATUS_IDLE)
        {
            return;
        }

        CgsNetwork::PlayerName laPlayerNamesToAdd[KI_MAX_BUDDIES];
        CgsNetwork::PlayerName laPlayerNamesToRemove[KI_MAX_BUDDIES];
        s32  liNumToAdd = 0;
        s32  liNumToRemove = 0;
        bool lbOverwriteRequired = false;

        GetBuddiesToAddAndRemoveFromServer(laPlayerNamesToAdd, laPlayerNamesToRemove,
                                           &liNumToAdd, &liNumToRemove, &lbOverwriteRequired);

        if (lbOverwriteRequired || mbOverwriteFriendsList)
        {
            CGS_ASSERT(lpServerInterface->GetCustomCommandsComponent(), "mpServerInterface->GetCustomCommandsComponent()");
            ServerInterfaceCustomCommands* lpCustomCommandsComponent = lpServerInterface->GetCustomCommandsComponent();
            CGS_ASSERT(lpCustomCommandsComponent, "lpCustomCommandsComponent");

            lpCustomCommandsComponent->OverWriteServerFriendsRecord(maBuddyListAtUpload, miNumBuddiesAtUpload);

            memcpy(maServerBuddyList, maBuddyListAtUpload, sizeof(maBuddyListAtUpload));
            mbOverwriteFriendsList = false;
            miNumServerBuddies = miNumBuddiesAtUpload;
            meState = E_BUDDY_MANAGER_STATE_SERVER_BUDDIES_UPDATED;
        }
        else if (liNumToAdd > 0 || liNumToRemove > 0)
        {
            CGS_ASSERT(!(laPlayerNamesToAdd[0].macName[0] == 0 && laPlayerNamesToRemove[0].macName[0] == 0),
                       "We are trying to change the server buddy list but haven't got any names!");

            CGS_ASSERT(lpServerInterface->GetCustomCommandsComponent(), "mpServerInterface->GetCustomCommandsComponent()");
            ServerInterfaceCustomCommands* lpCustomCommandsComponent = lpServerInterface->GetCustomCommandsComponent();
            CGS_ASSERT(lpCustomCommandsComponent, "lpCustomCommandsComponent");

            lpCustomCommandsComponent->UpdateServerFriendsRecord(
                laPlayerNamesToAdd, laPlayerNamesToRemove,
                liNumToAdd, liNumToRemove,
                _UploadFriendsToServerComplete, this);
            meState = E_BUDDY_MANAGER_STATE_UPLOADING_SERVER_BUDDIES;
        }
        else
        {
            meState = E_BUDDY_MANAGER_STATE_SERVER_BUDDIES_UPDATED;
        }
    }

    // =======================================================================
    // Update @ 0x8256B3C8
    // =======================================================================
    void BuddyManagerBase::Update(bool lbCanBlock, f32 lfTimeStep)
    {
        CgsNetwork::BuddyManagerX360::Update(lbCanBlock);

        switch (meState)
        {
            case E_BUDDY_MANAGER_STATE_FAILED:
            {
                CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");

                CgsSystem::Time lElapsed = mTimeUntilRetryAfterFailedBuddyUpload
                    - mpNetworkModule->GetNetworkManager()->GetTime();

                BrnServerInterface* lpServerInterface = AsBrnServerInterface(mpServerInterface);
                CGS_ASSERT(lpServerInterface, "mpServerInterface");
                CGS_ASSERT(lpServerInterface->GetGameComponent(), "mpServerInterface->GetGameComponent()");

                if (!lpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
                {
                    if (lElapsed.GetFloatVal() < 0.0f)
                    {
                        meState = E_BUDDY_MANAGER_STATE_START_GET_SERVER_BUDDIES;
                    }
                }
                break;
            }

            case E_BUDDY_MANAGER_STATE_START_GET_SERVER_BUDDIES:
                GetServerFriends();
                break;

            case E_BUDDY_MANAGER_STATE_START_UPLOAD_SERVER_BUDDIES:
                UploadServerFriends();
                break;

            default:
                break;
        }

        if (miNextBuddyToSend < GetNumBuddies())
        {
            SendBuddyInformation(GetEventQueue(mpNetworkModule));
        }

        if (mfBuddyListChangedTimer >= 0.0f)
        {
            mfBuddyListChangedTimer -= lfTimeStep;
            if (mfBuddyListChangedTimer < 0.0f)
            {
                HandleBuddyListChange();
                mfBuddyListChangedTimer = -1.0f;
            }
        }

        if (mfTimeUntilRetryGetServerBuddies >= 0.0f)
        {
            mfTimeUntilRetryGetServerBuddies -= lfTimeStep;
            if (mfTimeUntilRetryGetServerBuddies < 0.0f)
            {
                CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager(), "mpNetworkModule->GetNetworkManager()");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetStateManager(),
                           "mpNetworkModule->GetNetworkManager()->GetStateManager()");

                StateManager* lpStateManager = mpNetworkModule->GetNetworkManager()->GetStateManager();
                if (lpStateManager->IsInLimbo())
                {
                    lpStateManager->SetRetryGetServerBuddies(true);
                }
                mfTimeUntilRetryGetServerBuddies = -1.0f;
            }
        }
    }

    // =======================================================================
    // SendEmptyBuddyInformation @ 0x825637E0
    // =======================================================================
    void BuddyManagerBase::SendEmptyBuddyInformation(NetworkEventQueue* lpEventQueue)
    {
        CGS_ASSERT(GetNumBuddies() == 0, "Too Many buddies to send empty info : ");

        BuddyInformationEvent lEvent;
        memset(&lEvent, 0, sizeof(lEvent));

        lEvent.miNumBuddiesInBatch = 0;
        lEvent.miStartIndex = 0;
        lEvent.mbIsLastBatch = AreAnyInvitesOpen();

        lpEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent), 2, 672);
    }

    // =======================================================================
    // SendBuddyInformation @ 0x825638D8
    // =======================================================================
    void BuddyManagerBase::SendBuddyInformation(NetworkEventQueue* lpEventQueue)
    {
        BuddyInformationEvent lEvent;

        s32 liBuddySlotIndex = 0;
        lEvent.miNumBuddiesInBatch = 0;
        lEvent.miStartIndex = miNextBuddyToSend;

        do
        {
            if (miNextBuddyToSend >= GetNumBuddies())
            {
                break;
            }

            BuddyInformationRecord& lRecord = lEvent.maBuddyInformation[liBuddySlotIndex];

            CGS_ASSERT(GetBuddyName(miNextBuddyToSend, &lRecord.mPlayerName),
                       "GetBuddyName( miNextBuddyToSend, &lEvent.mBuddyInformation[liBuddySlotIndex].mPlayerName )");
            CGS_ASSERT(lRecord.mPlayerName.macName[0] != 0,
                       "!lEvent.mBuddyInformation[liBuddySlotIndex].mPlayerName.IsEmpty()");

            lRecord.miInviteType    = GetTotalNumberOfMessages(&lRecord.mPlayerName);
            lRecord.miInviteSubType = GetNumberOfUnreadMessages(&lRecord.mPlayerName);
            lRecord.mbIsJoinable    = IsBuddyJoinable(&lRecord.mPlayerName);
            lRecord.mbIsOnline      = IsBuddyOnline(&lRecord.mPlayerName);
            lRecord.mbIsFullBuddy   = IsFullBuddy(&lRecord.mPlayerName);
            lRecord.mbIsInSameLobby = false;

            CgsCore::SPrintf(lRecord.macPresenceData, 100, "%s", "PRESENCE_OFFLINE");
            GetBuddyPresence(&lRecord.mPlayerName, lRecord.macPresenceData);

            if (HaveIInvitedMyBuddy(&lRecord.mPlayerName))
            {
                lRecord.miInviteStatus = 2;
            }
            else
            {
                lRecord.miInviteStatus = HasBuddyInvitedMe(&lRecord.mPlayerName);
            }

            ++liBuddySlotIndex;
            ++miNextBuddyToSend;
        }
        while (liBuddySlotIndex < 5);

        lEvent.mbIsLastBatch = true;

        lpEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent), 2, 672);
    }

    // =======================================================================
    // ProcessEvent @ 0x8256B6C0
    // =======================================================================
    void BuddyManagerBase::ProcessEvent(const CgsModule::Event* lpEvent, s32 liEventType)
    {
        switch (liEventType)
        {
            case 1:
            {
                BuddyCountEvent lEvent;
                lEvent.miNumBuddies = GetNumBuddies();
                GetEventQueue(mpNetworkModule)->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent), 1, 4);
                break;
            }
            case 2:
            {
                miNextBuddyToSend = 0;
                if (GetNumBuddies() == 0)
                {
                    SendEmptyBuddyInformation(GetEventQueue(mpNetworkModule));
                }
                break;
            }
            case 9:
                SendInvite(NULL);
                break;
            case 10:
                AcceptInvite(NULL);
                break;
            case 11:
                JoinBuddy(NULL);
                break;
            case 12:
                CancelInvites();
                break;
            default:
                break;
        }
    }

    // =======================================================================
    // BuddyListHasChanged @ 0x82563AF0
    // =======================================================================
    void BuddyManagerBase::BuddyListHasChanged()
    {
        CgsNetwork::BuddyManagerBase::BuddyListHasChanged();

        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
        CGS_ASSERT(GetEventQueue(mpNetworkModule), "GetEventQueue(mpNetworkModule)");

        if (meState != E_BUDDY_MANAGER_STATE_START)
        {
            mfBuddyListChangedTimer = KF_DELAY_HANDLING_BUDDY_CHANGE;
        }
        else
        {
            meState = E_BUDDY_MANAGER_STATE_START_GET_SERVER_BUDDIES;
            miCachedNumBuddiesAtLastChange = 0;
            for (s32 liBuddyIndex = 0; liBuddyIndex < GetNumBuddies(); ++liBuddyIndex)
            {
                CgsNetwork::PlayerName lBuddyName;
                GetBuddyName(liBuddyIndex, &lBuddyName);
                if (IsFullBuddy(&lBuddyName))
                {
                    CGS_ASSERT(miCachedNumBuddiesAtLastChange < KI_MAX_BUDDIES,
                               "miCachedNumBuddiesAtLastChange < CgsNetwork::KI_MAX_BUDDIES");
                    maCachedBuddyListAtLastChange[miCachedNumBuddiesAtLastChange] = lBuddyName;
                    ++miCachedNumBuddiesAtLastChange;
                }
            }
        }

        const s32 liNumberOfNewOnlineBuddies = GetNumberOfNewOnlineBuddies();
        if (liNumberOfNewOnlineBuddies == 1)
        {
            const s32 liIndexOfLastBuddyToComeOnline = GetIndexOfLastBuddyToComeOnline();
            CGS_ASSERT(liIndexOfLastBuddyToComeOnline != CgsNetwork::BuddyManagerBase::KI_INVALID_BUDDY_INDEX,
                       "liIndexOfLastBuddyToComeOnline != CgsNetwork::BuddyManager::KI_INVALID_BUDDY_INDEX");

            BuddyOnlineNotificationEvent lBuddyNotificationEvent;
            lBuddyNotificationEvent.miNotificationType = 1;
            lBuddyNotificationEvent.miCount = 1;

            CGS_ASSERT(GetBuddyName(liIndexOfLastBuddyToComeOnline, &lBuddyNotificationEvent.mBuddyName),
                       "GetBuddyName( liIndexOfLastBuddyToComeOnline, &lBuddyNotificationEvent.mBuddyName )");

            GetEventQueue(mpNetworkModule)->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lBuddyNotificationEvent), 9, 24);
        }
        else if (liNumberOfNewOnlineBuddies > 1)
        {
            BuddyOnlineNotificationEvent lBuddyNotificationEvent;
            lBuddyNotificationEvent.miNotificationType = 2;
            lBuddyNotificationEvent.miCount = GetNumOnlineBuddies();
            lBuddyNotificationEvent.mBuddyName.macName[0] = 0;

            GetEventQueue(mpNetworkModule)->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lBuddyNotificationEvent), 9, 24);
        }

        BuddyListUpdatedEvent lUpdatedEvent;
        GetEventQueue(mpNetworkModule)->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUpdatedEvent), 10, 1);

        if (IsConnectedToNetworkService())
        {
            mfTimeUntilRetryGetServerBuddies = -1.0f;
        }
        else if (mfTimeUntilRetryGetServerBuddies == -1.0f)
        {
            mfTimeUntilRetryGetServerBuddies = KF_RETRY_GET_SERVER_BUDDIES;
        }
    }

    // =======================================================================
    // HandleBuddyListChange @ 0x82563530
    // =======================================================================
    void BuddyManagerBase::HandleBuddyListChange()
    {
        bool lbBuddyListChanged = false;

        const s32 liNumFullBuddies = GetNumFullBuddies();
        if (miCachedNumBuddiesAtLastChange < liNumFullBuddies)
        {
            lbBuddyListChanged = true;
        }
        else if (miCachedNumBuddiesAtLastChange > liNumFullBuddies)
        {
            for (s32 li = 0; li < miCachedNumBuddiesAtLastChange; ++li)
            {
                if (!IsFullBuddy(&maCachedBuddyListAtLastChange[li]))
                {
                    BuddyOfflineNotificationEvent lEvent;
                    lEvent.mBuddyName = maCachedBuddyListAtLastChange[li];
                    GetEventQueue(mpNetworkModule)->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lEvent), 14, 16);
                }
            }
            lbBuddyListChanged = true;
        }

        if (meState == E_BUDDY_MANAGER_STATE_SERVER_BUDDIES_UPDATED && lbBuddyListChanged)
        {
            meState = E_BUDDY_MANAGER_STATE_START_GET_SERVER_BUDDIES;
        }

        miCachedNumBuddiesAtLastChange = 0;
        for (s32 liBuddyIndex = 0; liBuddyIndex < GetNumBuddies(); ++liBuddyIndex)
        {
            CgsNetwork::PlayerName lBuddyName;
            GetBuddyName(liBuddyIndex, &lBuddyName);
            if (IsFullBuddy(&lBuddyName))
            {
                CGS_ASSERT(miCachedNumBuddiesAtLastChange < KI_MAX_BUDDIES,
                           "miCachedNumBuddiesAtLastChange < CgsNetwork::KI_MAX_BUDDIES");
                maCachedBuddyListAtLastChange[miCachedNumBuddiesAtLastChange] = lBuddyName;
                ++miCachedNumBuddiesAtLastChange;
            }
        }
    }

    // =======================================================================
    // _GetFriendsListFromServerComplete @ 0x82549C58 (static callback)
    // =======================================================================
    void BuddyManagerBase::_GetFriendsListFromServerComplete(void* lpData, void* lpResult, bool lbSuccess)
    {
        BuddyManagerBase* lpBuddyManager = reinterpret_cast<BuddyManagerBase*>(lpData);
        CGS_ASSERT(lpBuddyManager, "lpBuddyManager");

        BrnServerInterface* lpServerInterface = AsBrnServerInterface(lpBuddyManager->mpServerInterface);
        CGS_ASSERT(lpServerInterface, "lpServerInterface");

        if (lbSuccess)
        {
            lpBuddyManager->PopulateServerBuddyList(reinterpret_cast<char*>(lpResult));
            lpBuddyManager->meState = E_BUDDY_MANAGER_STATE_START_UPLOAD_SERVER_BUDDIES;
        }
        else if (lpServerInterface->GetCustomCommandsComponent()->GetLastError()
                 == CgsNetwork::E_BUDDY_ERROR_CODES_BADPARAMS)
        {
            lpBuddyManager->miNumServerBuddies = 0;
            lpServerInterface->GetCustomCommandsComponent()->ClearLastError();
            lpBuddyManager->mbOverwriteFriendsList = true;
            lpBuddyManager->meState = E_BUDDY_MANAGER_STATE_START_UPLOAD_SERVER_BUDDIES;
        }
        else
        {
            lpBuddyManager->meState = E_BUDDY_MANAGER_STATE_FAILED;

            CGS_ASSERT(lpServerInterface->GetDownloadableConfigComponent(),
                       "lpServerInterface->GetDownloadableConfigComponent()");
            lpBuddyManager->mTimeUntilRetryAfterFailedBuddyUpload.SetFloatVal(
                lpServerInterface->GetDownloadableConfigComponent()->GetBuddyUploadRetryDelay());

            CGS_ASSERT(lpServerInterface->GetCustomCommandsComponent(),
                       "lpServerInterface->GetCustomCommandsComponent()");
            ServerInterfaceCustomCommands* lpCustomCommands = lpServerInterface->GetCustomCommandsComponent();
            if (lpCustomCommands->GetLastError() == CgsNetwork::E_BUDDY_ERROR_CODES_PENDING)
            {
                lpCustomCommands->ClearLastError();
            }
        }
    }

    // =======================================================================
    // _UploadFriendsToServerComplete @ 0x82549DD0 (static callback)
    // =======================================================================
    void BuddyManagerBase::_UploadFriendsToServerComplete(void* lpData, void* lpResult, bool lbSuccess)
    {
        BuddyManagerBase* lpBuddyManager = reinterpret_cast<BuddyManagerBase*>(lpData);
        CGS_ASSERT(lpBuddyManager, "lpBuddyManager");

        if (lbSuccess)
        {
            memcpy(lpBuddyManager->maServerBuddyList, lpBuddyManager->maBuddyListAtUpload,
                   sizeof(lpBuddyManager->maServerBuddyList));
            lpBuddyManager->miNumServerBuddies = lpBuddyManager->miNumBuddiesAtUpload;
        }
        else
        {
            BrnServerInterface* lpServerInterface = AsBrnServerInterface(lpBuddyManager->mpServerInterface);
            CGS_ASSERT(lpServerInterface, "lpServerInterface");
            CGS_ASSERT(lpServerInterface->GetCustomCommandsComponent(),
                       "lpServerInterface->GetCustomCommandsComponent()");

            ServerInterfaceCustomCommands* lpCustomCommands = lpServerInterface->GetCustomCommandsComponent();
            if (lpCustomCommands->GetLastError() == CgsNetwork::E_BUDDY_ERROR_CODES_PENDING)
            {
                lpCustomCommands->ClearLastError();
            }
        }

        lpBuddyManager->meState = E_BUDDY_MANAGER_STATE_SERVER_BUDDIES_UPDATED;
    }

    // -- sort comparator used by GetBuddiesToAddAndRemoveFromServer's qsort
    //    (X360 sub_8287DCF0 -- a LobbyNameCmp wrapper over two PlayerName slots).
    int BuddyManagerBase::_SortPlayerNames(const void* lpA, const void* lpB)
    {
        return LobbyNameCmp(reinterpret_cast<const CgsNetwork::PlayerName*>(lpA)->macName,
                            reinterpret_cast<const CgsNetwork::PlayerName*>(lpB)->macName);
    }

    // =======================================================================
    // _AssertLayout -- never called; pins the recovered member RELATIONSHIPS.
    //
    // The X360 image is 32-bit (4-byte pointers + vtable), so the raw absolute
    // byte offsets the asm uses (mDebugComponent @ +80160, the lists, the
    // scalar tail @ +85668..) assume that pointer width and the 32-bit base
    // sub-object size. The PC gate is 64-bit, so we pin the ABI-stable
    // *relationships* recovered from the asm rather than the raw 32-bit offsets:
    // the three 100-entry PlayerName lists are contiguous (1600 bytes apart),
    // each PlayerName is 16 bytes, and the trailing scalar block keeps the asm's
    // member order/spacing (each scalar 4 bytes apart; the Time is 8). Same
    // approach as the sibling CgsBuddyManagerDirtySockX360::_AssertLayout.
    // =======================================================================
    void BuddyManagerBase::_AssertLayout()
    {
        static_assert(sizeof(CgsNetwork::PlayerName) == 16, "PlayerName is 16 bytes (X360 KI_USERNAME_LENGTH)");

        static_assert(offsetof(BuddyManagerBase, maBuddyListAtUpload)
                      - offsetof(BuddyManagerBase, maServerBuddyList) == 1600,
                      "maServerBuddyList[100] precedes maBuddyListAtUpload");
        static_assert(offsetof(BuddyManagerBase, maCachedBuddyListAtLastChange)
                      - offsetof(BuddyManagerBase, maBuddyListAtUpload) == 1600,
                      "maBuddyListAtUpload[100] precedes maCachedBuddyListAtLastChange");

        static_assert(offsetof(BuddyManagerBase, miNumServerBuddies)
                      - offsetof(BuddyManagerBase, mTimeUntilRetryAfterFailedBuddyUpload) == 8,
                      "mTimeUntilRetryAfterFailedBuddyUpload (Time, 8B) precedes miNumServerBuddies");
        static_assert(offsetof(BuddyManagerBase, miNumBuddiesAtUpload)
                      - offsetof(BuddyManagerBase, miNumServerBuddies) == 4,
                      "miNumServerBuddies precedes miNumBuddiesAtUpload");
        static_assert(offsetof(BuddyManagerBase, miCachedNumBuddiesAtLastChange)
                      - offsetof(BuddyManagerBase, miNumBuddiesAtUpload) == 4,
                      "miNumBuddiesAtUpload precedes miCachedNumBuddiesAtLastChange");
        static_assert(offsetof(BuddyManagerBase, meState)
                      - offsetof(BuddyManagerBase, miCachedNumBuddiesAtLastChange) == 4,
                      "miCachedNumBuddiesAtLastChange precedes meState");
        static_assert(offsetof(BuddyManagerBase, mfTimeUntilRetryGetServerBuddies)
                      - offsetof(BuddyManagerBase, meState) >= 4,
                      "meState + mpNetworkModule precede the float timers");
        static_assert(offsetof(BuddyManagerBase, mfBuddyListChangedTimer)
                      - offsetof(BuddyManagerBase, mfTimeUntilRetryGetServerBuddies) == 4,
                      "mfTimeUntilRetryGetServerBuddies precedes mfBuddyListChangedTimer");
        static_assert(offsetof(BuddyManagerBase, miNextBuddyToSend)
                      - offsetof(BuddyManagerBase, mfBuddyListChangedTimer) == 4,
                      "mfBuddyListChangedTimer precedes miNextBuddyToSend");
        static_assert(offsetof(BuddyManagerBase, mbOverwriteFriendsList)
                      - offsetof(BuddyManagerBase, miNextBuddyToSend) == 4,
                      "miNextBuddyToSend precedes mbOverwriteFriendsList");
    }
}
