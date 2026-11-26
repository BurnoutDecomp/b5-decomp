// GameSource/Network/Managers/BrnNetworkLiveRevengeManager.cpp
//
// BrnNetwork::LiveRevengeManager -- live-revenge rival tracking. The manager owns the
// 250-entry relationship table (the saved profile), maps each remote player in the session to
// a table row, turns takedowns / paybacks / marks / round results into relationship stats,
// keeps the top-ten rival list and uploads its changed rows, and syncs each rival's copy of the
// relationship over a reliable message.
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkLiveRevengeManager.cpp"
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"

#include <cstdlib>   // std::qsort

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"                 // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                  // CgsNetwork::UsernameCompare
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // EStatus
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"              // TimeManager::GetCurrentFrameNumber
#include "GameSource/GameState/BrnGameActions.h"                             // OnlineRoundResults::GetPosition
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"                           // OutputBuffer, PostSimulationInputBuffer, NetworkEventQueue
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                    // NetworkInPaybackIntialised / NetworkInPaybackSucceeded
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                   // NetworkOutRivalCount / LiveRevengeProfileData / AutosaveProfile
#include "GameSource/Network/Components/BrnServerInterfaceCustomCommands.h"  // ServerInterfaceCustomCommands
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                     // BrnNetwork::PlayerMenuData::mMarkedManID
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameSource/Network/SharedIO/BrnNetworkToGuiIOInterfaces.h"         // NetworkToGuiInterface::AddLiveRevengeUpdate
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"         // PlayerParams (GetUniqueIDByName)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h" // GetPlayerParametersByPlayerName
#include <cstring>                                                            // strlen / strncpy (rival name copy)

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkLiveRevengeManager.cpp".
static const char KAC_ARTIST_ASSERT_HEADER[] =
    "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkLiveRevengeManager.h";

namespace BrnNetwork
{
    // The "no table row" value of a mapping slot's player id.
    const s32 KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE = -1;

    // ============================================================================
    // Lifecycle
    // ============================================================================

    void LiveRevengeManager::Construct(BrnNetworkModule* lpNetworkModule)
    {
        mbAreWeInOnlineGame  = false;
        mpLiveRevengeProfile = 0;
        miNumberOfTopRivals  = 0;

        mTakedownEventQueue.Construct();

        CGS_ASSERT_AT(lpNetworkModule,
            "lpNetworkModule",
            __FILE__, 67 /* ARTIST 0x8255FB10 */);
        mpNetworkModule  = lpNetworkModule;
        mpNetworkManager = lpNetworkModule->GetNetworkManager();
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            __FILE__, 70 /* ARTIST 0x8255FB44 */);

        // The member stores precede ClearTopRivals (the PS3 body 0x5EDCB0 makes them before
        // its out-of-line call; ARTIST 0x8255FB50..68 before the inlined loop).
        miNumberOfTopRivals = 0;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        ClearTopRivals();

        mDebugComponent.Construct(this);
    }

    bool LiveRevengeManager::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
    {
        CGS_ASSERT_AT(lpHeapMalloc,
            "lpHeapMalloc",
            CGS_ARTIST_FILE, 100 /* ARTIST 0x8255A8F0 */);
        mpAllocator = lpHeapMalloc;

        CGS_ASSERT_AT(lpHeapMalloc->GetAllocator()->ValidateHeap( EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
            "lpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
            CGS_ARTIST_FILE, 102 /* ARTIST 0x8255A930 */);
        mpLiveRevengeProfile = static_cast<LiveRevengeProfile*>(lpHeapMalloc->Malloc(sizeof(LiveRevengeProfile), 4));
        CGS_ASSERT_AT(lpHeapMalloc->GetAllocator()->ValidateHeap( EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
            "lpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
            CGS_ARTIST_FILE, 104 /* ARTIST 0x8255A978 */);
        CGS_ASSERT_AT(mpLiveRevengeProfile,
            "mpLiveRevengeProfile",
            CGS_ARTIST_FILE, 105 /* ARTIST 0x8255A9A0 */);

        mpLiveRevengeProfile->Clear();
        ResetRevengeTableMappings();

        miNumberOfTopRivals = 0;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        ClearTopRivals();

        mDebugComponent.Prepare();
        return true;
    }

    bool LiveRevengeManager::Release()
    {
        CGS_ASSERT_AT(mpLiveRevengeProfile,
            "mpLiveRevengeProfile", __FILE__, 140 /* ARTIST 0x8255AA54 */);
        CGS_ASSERT_AT(mpAllocator,
            "mpAllocator", __FILE__, 141 /* ARTIST 0x8255AA7C */);

        mpLiveRevengeProfile->Clear();
        mpAllocator->Free(mpLiveRevengeProfile);

        mpLiveRevengeProfile = 0;
        mpAllocator          = 0;
        mpNetworkManager     = 0;
        ResetRevengeTableMappings();

        miNumberOfTopRivals = 0;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        ClearTopRivals();

        mDebugComponent.Release();
        return true;
    }

    void LiveRevengeManager::Destruct()
    {
        mbAreWeInOnlineGame  = false;
        mpLiveRevengeProfile = 0;
        mpAllocator          = 0;

        miNumberOfTopRivals = 0;
        meLiveRevengeUploadStatus          = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
        mbProfileIsDirty                   = false;
        mbNeedToUpdateMarksForCurrentRound = false;
        ClearTopRivals();

        mDebugComponent.Destruct();
    }

    // ============================================================================
    // Per-frame
    // ============================================================================

    void LiveRevengeManager::ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        ProcessTakedownQueue(lpOutputBuffer);

        if (meLiveRevengeUploadStatus == E_LIVE_REVENGE_UPLOAD_STATUS_PENDING)
        {
            SendLiveRevengeRivalsToServer();
        }

        if (meLiveRevengeUploadStatus == E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS)
        {
            CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
            CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");
            CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent(),
                       "mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent()");

            ServerInterfaceCustomCommands* lpCustomCommands =
                mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent();
            switch (lpCustomCommands->GetStatus())
            {
            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_ERROR:
                // FLAG: the error code is also written to the network log stream ("Could not
                // upload live revenge data, error code: <n>"), which has no home in this tree.
                lpCustomCommands->GetAndClearLastError();
                lpCustomCommands->ClearLastError();
                meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
                mpNetworkManager->OnAutoLoginProcessComplete(1);
                break;

            case CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE:
                meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
                mpNetworkManager->OnAutoLoginProcessComplete(1);
                break;

            default:
                break;
            }
        }

        {
            BrnNetworkModuleIO::NetworkOutLiveRevengeProfileData lLiveRevengeProfileEvent;
            lLiveRevengeProfileEvent.mpLiveRevengeProfile = mpLiveRevengeProfile;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lLiveRevengeProfileEvent);
        }

        if (mbNeedToUpdateMarksForCurrentRound)
        {
            const s32 liNumberOfMarksRecieved =
                mpNetworkManager->GetMarkedManManager()->GetNumberOfPlayersFinishedMarking();
            const s32 liNumberOfPlayersInGame =
                mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                    CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
            if (liNumberOfMarksRecieved == liNumberOfPlayersInGame)
            {
                UpdateMarkedManInfo();
            }
        }

        if (mbProfileIsDirty)
        {
            CGS_ASSERT(IsTableValid(),
                       "If this assert fires then it the live revenge manager is corrupting the profile. Deleting your save game will fix this for now (tell Alex Veal).\n");

            BrnNetworkModuleIO::NetworkOutAutosaveProfile lAutosaveEvent;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lAutosaveEvent);
            mbProfileIsDirty = false;
        }
    }

    void LiveRevengeManager::ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer)
    {
        CGS_ASSERT_AT(lpInputBuffer,
            "lpInputBuffer", __FILE__, 294 /* ARTIST 0x82568C3C */);
        mTakedownEventQueue.Append(*lpInputBuffer->GetTakedownEventInputQueue());
        ProcessGameDirtyTrickInterface();
    }

    // Feed this frame's takedowns into the relationships (only in the modes that keep live
    // revenge, and never for a player inside a freeburn challenge), then report a grown rival
    // count to the game.
    void LiveRevengeManager::ProcessTakedownQueue(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkModule, "mpNetworkModule");

        const BrnNetworkModuleIO::GameStateToNetworkInterface* lpGameStateToNetworkInterface =
            mpNetworkModule->GetGameStateToNetworkInterface();
        CGS_ASSERT(lpGameStateToNetworkInterface, "lpGameStateToNetworkInterface");

        const s32 liNumberOfTakedowns = mTakedownEventQueue.GetLength();
        if (liNumberOfTakedowns == 0)
        {
            return;
        }

        const BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameModeType =
            lpGameStateToNetworkInterface->GetCurrentGameMode();
        const s32 liOldNumberOfRivals = GetNumberOfRivals();

        if ((leCurrentGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
             leCurrentGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) ||
            leCurrentGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE)
        {
            for (s32 liEventCounter = 0; liEventCounter < mTakedownEventQueue.GetLength(); ++liEventCounter)
            {
                const BrnGameState::TakedownEvent lEvent = mTakedownEventQueue.GetEvent(liEventCounter);
                const EActiveRaceCarIndex leVictimActiveRaceCarIndex = lEvent.meVictimIndex;
                const EActiveRaceCarIndex leAggressorActiveCarIndex  = lEvent.meAggressorIndex;

                CGS_ASSERT(lEvent.meAggressorIndex != lEvent.meVictimIndex,
                           "lEvent.meAggressorIndex != lEvent.meVictimIndex");

                if (!lpGameStateToNetworkInterface->GetPlayerInFreeburnChallenge(leAggressorActiveCarIndex) &&
                    !lpGameStateToNetworkInterface->GetPlayerInFreeburnChallenge(leVictimActiveRaceCarIndex))
                {
                    UpdateLiveRevengeRelationShip(leAggressorActiveCarIndex, leVictimActiveRaceCarIndex,
                                                  lEvent.mbMarkedManTakeDown, lpOutputBuffer);
                }
            }
        }

        mTakedownEventQueue.Clear();

        const s32 liNewNumberOfRivals = GetNumberOfRivals();
        if (liNewNumberOfRivals > liOldNumberOfRivals)
        {
            BrnNetworkModuleIO::NetworkOutRivalCount lRivalCountMessage;
            lRivalCountMessage.miNumberOfRivals = liNewNumberOfRivals;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(lRivalCountMessage);
        }
    }

    // Turn this frame's payback (dirty-trick) events into relationship stats.
    void LiveRevengeManager::ProcessGameDirtyTrickInterface()
    {
        const BrnNetworkModuleIO::GameStateToNetworkInterface::DirtyTrickQueue* lpDirtyTrickEventQueue =
            mpNetworkModule->GetGameStateToNetworkInterface()->GetDirtyTrickQueue();
        CGS_ASSERT_AT(lpDirtyTrickEventQueue,
            "lpDirtyTrickEventQueue", __FILE__, 318 /* ARTIST 0x82562FAC */);


        for (s32 liIndex = 0; liIndex < lpDirtyTrickEventQueue->GetLength(); ++liIndex)
        {
            const BrnNetworkModuleIO::DirtyTrickEvent lDirtyTrickEvent = lpDirtyTrickEventQueue->GetEvent(liIndex);
            UpdatePaybacksData(lDirtyTrickEvent.meAggressorActiveRaceCarIndex,
                               lDirtyTrickEvent.meVictimActiveRaceCarIndex,
                               lDirtyTrickEvent.meDirtyTrickStatus);
        }
    }

    // ============================================================================
    // Session and round edges
    // ============================================================================

    void LiveRevengeManager::Disconnected()
    {
        ResetRevengeTableMappings();
    }

    void LiveRevengeManager::OnRoundStart()
    {
        mbAreWeInOnlineGame = true;
        if (!mpNetworkManager->IsDoingFreeBurnLobby())
        {
            mbNeedToUpdateMarksForCurrentRound = true;
        }
    }

    // Both are empty bodies on the console.
    void LiveRevengeManager::OnLeaveGame()
    {
    }

    // Every mapped rival gets one more shared event; the profile is then due a save.
    void LiveRevengeManager::OnRoundFinish()
    {
        mbAreWeInOnlineGame = false;

        for (s32 liMappingIndex = 0; liMappingIndex < KI_MAX_NETWORK_PLAYERS; ++liMappingIndex)
        {
            if (maPlayerToTableIndexData[liMappingIndex].mPlayerID == KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE)
            {
                continue;
            }

            LiveRevengeMappingEntry* lpEntry = FindMappingEntry(maPlayerToTableIndexData[liMappingIndex].mPlayerID);
            if (lpEntry)
            {
                LiveRevengeProfile* lpProfile = GetProfile();
                CGS_ASSERT_AT(lpProfile,
                    "lpProfile",
                    __FILE__, 375 /* ARTIST 0x8255AC10 */);
                lpProfile->maRelationshipTable[static_cast<u32>(lpEntry->miRevengeTableIndex)].OnRoundFinish();
            }
        }

        AutoSaveLiveRevengeProfile();
    }

    void LiveRevengeManager::OnGameFinish()
    {
    }

    // Credit the round's finishing order: a rival who finished behind the local player (or
    // did not finish) is a local win, one who finished ahead is a rival win.
    void LiveRevengeManager::HandleRoundResults(const BrnGameState::GameStateModuleIO::OnlineRoundResults* lpResults)
    {
        CGS_ASSERT_AT(lpResults,
            "lpResults",
            __FILE__, 1704 /* ARTIST 0x8255B678 */);
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            __FILE__, 1707 /* ARTIST 0x8255B6A0 */);
        CGS_ASSERT_AT(mpNetworkManager->GetPlayerManager(),
            "mpNetworkManager->GetPlayerManager()",
            __FILE__, 1708 /* ARTIST 0x8255B6C8 */);

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        if (!mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID))
        {
            return;
        }

        const s32 liLocalPlayersPosition = lpResults->GetPosition(lLocalPlayerID);
        for (s32 liMappingIndex = 0; liMappingIndex < KI_MAX_NETWORK_PLAYERS; ++liMappingIndex)
        {
            const NetworkPlayerID lRivalPlayerID = maPlayerToTableIndexData[liMappingIndex].mPlayerID;
            if (lRivalPlayerID == KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE)
            {
                continue;
            }

            const s32 liRivalPosition = lpResults->GetPosition(lRivalPlayerID);
            LiveRevengeRelationship* lpLiveRevengeRelationship = GetNonConstRevengeRelationship(lRivalPlayerID);
            CGS_ASSERT_AT(lpLiveRevengeRelationship,
                "lpLiveRevengeRelationship",
                __FILE__, 1728 /* ARTIST 0x8255B75C */);

            if (liLocalPlayersPosition == BrnGameState::GameStateModuleIO::OnlineRoundResults::KI_POSITION_DISCONNECTED)
            {
                continue;
            }

            if (liRivalPosition == BrnGameState::GameStateModuleIO::OnlineRoundResults::KI_POSITION_DISCONNECTED ||
                liRivalPosition > liLocalPlayersPosition)
            {
                lpLiveRevengeRelationship->AddWinByLocalPlayer();
            }
            else if (liRivalPosition < liLocalPlayersPosition)
            {
                lpLiveRevengeRelationship->AddWinByRival();
            }
        }
    }

    // Who marked whom this round: a local mark on a mapped rival, or a rival's mark on the
    // local player, counts as a mark in that relationship.
    void LiveRevengeManager::UpdateMarkedManInfo()
    {
        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);

        NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerMenuData* lpPlayerMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT_AT(lpPlayerMenuData,
                "lpPlayerMenuData",
                __FILE__, 1789 /* ARTIST 0x8255B87C */);
            if (!lpPlayerMenuData || lpPlayerMenuData->mMarkedManID == CgsNetwork::K_INVALID_PLAYER_ID)
            {
                continue;
            }

            CGS_ASSERT_AT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 1801 /* ARTIST 0x8255B8B0 */);
            CGS_ASSERT_AT(lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
                __FILE__, 1802 /* ARTIST 0x8255B8D0 */);

            if (lPlayerID == lLocalPlayerID)
            {
                LiveRevengeRelationship* lpRelationship = GetNonConstRevengeRelationship(lpPlayerMenuData->mMarkedManID);
                if (lpRelationship)
                {
                    lpRelationship->AddMarkByPlayer();
                    AutoSaveLiveRevengeProfile();
                }
                else
                {
                    CGS_ASSERT_AT(mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpPlayerMenuData->mMarkedManID) == 0,
                        "mpNetworkManager->GetPlayerManager()->GetPlayerByID( lpPlayerMenuData->mMarkedManID ) == NULL",
                        __FILE__, 1817 /* ARTIST 0x8255B934 */);
                }
            }
            else if (lpPlayerMenuData->mMarkedManID == lLocalPlayerID)
            {
                LiveRevengeRelationship* lpRelationship = GetNonConstRevengeRelationship(lPlayerID);
                CGS_ASSERT_AT(lpRelationship,
                    "lpRelationship",
                    __FILE__, 1824 /* ARTIST 0x8255B970 */);
                if (lpRelationship)
                {
                    lpRelationship->AddMarkByRival();
                    AutoSaveLiveRevengeProfile();
                }
            }
        }

        mbNeedToUpdateMarksForCurrentRound = false;
    }

    // ============================================================================
    // Players
    // ============================================================================

    // Find (or create) the new player's table row by name, map the player to it, publish it to
    // the debug menu and register the per-player sync message pair.
    void LiveRevengeManager::AddPlayer(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            __FILE__, 410 /* ARTIST 0x8256DACC */);
        CGS_ASSERT_AT(mpNetworkManager->GetPlayerManager(),
            "mpNetworkManager->GetPlayerManager()",
            __FILE__, 411 /* ARTIST 0x8256DAF4 */);
        CGS_ASSERT_AT(IsTableValid(),
            "IsTableValid()",
            __FILE__, 412 /* ARTIST 0x8256DB28 */);

        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID);
        if (lpNetworkPlayer)
        {
            // The reference constant is the global KI_MAX_PLAYERS (8 network players), which
            // has no home in this tree yet; the active race-car count has the same value.
            CGS_ASSERT_AT(NetworkPlayerIDToActiveRaceCarIndex(lNetworkPlayerID) < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "NetworkPlayerIDToActiveRaceCarIndex(lNetworkPlayerID) < KI_MAX_PLAYERS",
                __FILE__, 426 /* ARTIST 0x8256DB74 */);

            CgsNetwork::PlayerName lPlayerName;
            lPlayerName.Construct(lpNetworkPlayer->GetName());
            // PlayerName::IsEmpty (inline): the first name character is the terminator.
            CGS_ASSERT_AT(lPlayerName.GetPlayerName()[0] != '\0',
                "!lPlayerName.IsEmpty()",
                __FILE__, 430 /* ARTIST 0x8256DBA8 */);

            s32 liPlayerIndexInTable = FindPlayerInTableByName(lPlayerName.GetPlayerName());
            if (liPlayerIndexInTable == -1)
            {
                LiveRevengeRelationship::UniquePlayerID lUniquePlayerID;
                GetUniqueIDByName(&lPlayerName, &lUniquePlayerID);
                // UniquePlayerID::IsValid (inline): a non-empty name and a non-zero XUID.
                CGS_ASSERT_AT(lUniquePlayerID.GetPlayerName()[0] != '\0' && lUniquePlayerID.mqXuid != 0,
                    "lUniquePlayerID.IsValid()",
                    __FILE__, 442 /* ARTIST 0x8256DC18 */);

                liPlayerIndexInTable = AddNewTableEntry(&lUniquePlayerID);
                CGS_ASSERT_AT(liPlayerIndexInTable != -1,
                    "liPlayerIndexInTable != -1",
                    __FILE__, 454 /* ARTIST 0x8256DC4C */);
            }

            AddMappingEntry(lNetworkPlayerID, liPlayerIndexInTable);
            AddRelationshipToDebugMenu(liPlayerIndexInTable);

            LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
            lpNetworkPlayer->RegisterMessageType(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE,
                                                 sizeof(LiveRevengeSyncMessage),
                                                 &lpEntry->mSendMessage, &lpEntry->mRecvMessage,
                                                 &LiveRevengeManager::_SyncMessageArrivedCallback,
                                                 &LiveRevengeManager::_SyncMessageDeliveredCallback,
                                                 this);
        }

        CGS_ASSERT_AT(IsTableValid(),
            "IsTableValid()",
            __FILE__, 479 /* ARTIST 0x8256DCF8 */);
    }

    void LiveRevengeManager::RemovePlayer(NetworkPlayerID lPlayerID)
    {
        CgsNetwork::NetworkPlayer* lpNetworkPlayer = mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer)
        {
            // The mapping is looked up unchecked (a missing entry is not guarded on the console).
            RemoveRelationshipFromDebugMenu(FindMappingEntry(lPlayerID)->miRevengeTableIndex);
            RemoveMappingEntry(lPlayerID);
            lpNetworkPlayer->UnRegisterMessageType(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE);
        }
    }

    // A remote player finished joining: send them our side of the relationship.
    void LiveRevengeManager::RemotePlayerFinalised(NetworkPlayerID lNetworkPlayerID)
    {
        CgsNetwork::NetworkPlayer* lpNetworkPlayer =
            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID);
        CGS_ASSERT_AT(lpNetworkPlayer,
            "lpNetworkPlayer",
            __FILE__, 1420 /* ARTIST 0x825602A8 */);

        LiveRevengeSyncMessage* lpLiveRevengeSyncMessage = static_cast<LiveRevengeSyncMessage*>(
            lpNetworkPlayer->GetRegisteredSendMessage(KI_LIVE_REVENGE_SYNC_MESSAGE_TYPE));
        CGS_ASSERT_AT(lpLiveRevengeSyncMessage,
            "lpLiveRevengeSyncMessage",
            __FILE__, 1422 /* ARTIST 0x825602DC */);

        const LiveRevengeRelationship* lpRevengeRelationship = GetRevengeRelationship(lNetworkPlayerID);
        CGS_ASSERT_AT(lpRevengeRelationship,
            "lpRevengeRelationship",
            __FILE__, 1425 /* ARTIST 0x82560310 */);

        lpLiveRevengeSyncMessage->PrepareForSend(lpRevengeRelationship,
                                                 mpNetworkManager->GetTimeManager()->GetCurrentFrameNumber());
    }

    // ============================================================================
    // Relationship table
    // ============================================================================

    s32 LiveRevengeManager::FindPlayerInTableByName(const char* lpcName)
    {
        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT_AT(lpProfile, "lpProfile", CGS_ARTIST_FILE, 525);

        const s32 liNumberOfRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liRivalIndex = 0; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        {
            if (CgsNetwork::UsernameCompare(
                    lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetRivalName()->GetPlayerName(),
                    lpcName) == 0)
            {
                return liRivalIndex;
            }
        }
        return -1;
    }

    // Append a fresh relationship for the new rival, or, with the table full, overwrite the
    // row that changed longest ago.
    s32 LiveRevengeManager::AddNewTableEntry(const LiveRevengeRelationship::UniquePlayerID* lpUniqueID)
    {
        LiveRevengeRelationship lRelationship;
        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");
        CGS_ASSERT(lpUniqueID, "lpUniqueID");
        // UniquePlayerID::IsValid (inline): a non-empty name and a non-zero XUID.
        CGS_ASSERT(lpUniqueID->GetPlayerName()[0] != '\0' && lpUniqueID->mqXuid != 0, "lpUniqueID->IsValid()");

        lRelationship.Construct();
        lRelationship.Prepare(lpUniqueID);

        if (lpProfile->maRelationshipTable.GetLength() < static_cast<u32>(LiveRevengeProfile::KI_MAX_REVENGE_HISTORY))
        {
            lpProfile->maRelationshipTable.Append(lRelationship);
            return static_cast<s32>(lpProfile->maRelationshipTable.GetLength()) - 1;
        }

        CgsSystem::DateAndTime lOldestDateAndTime = lpProfile->maRelationshipTable[0].GetLastChangedTime();
        s32 liOldestIndex = 0;
        const s32 liNumberOfRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liRivalIndex = 1; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        {
            if (lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetLastChangedTime() < lOldestDateAndTime)
            {
                lOldestDateAndTime = lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetLastChangedTime();
                liOldestIndex      = liRivalIndex;
            }
        }

        lpProfile->maRelationshipTable[static_cast<u32>(liOldestIndex)] = lRelationship;
        return liOldestIndex;
    }

    s32 LiveRevengeManager::GetNumberOfRelationships()
    {
        LiveRevengeProfile* lpProfile = GetProfile();
        if (!lpProfile)
        {
            return 0;
        }
        return static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
    }

    // A rival is a relationship with at least one takedown either way.
    s32 LiveRevengeManager::GetNumberOfRivals()
    {
        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        s32 liRivalCount = 0;
        const s32 liNumberOfRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        for (s32 liRivalIndex = 0; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        {
            if (lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetTotalTakedowns() > 0)
            {
                ++liRivalCount;
            }
        }
        return liRivalCount;
    }

    bool LiveRevengeManager::IsTableValid()
    {
        for (u32 luTableIndex = 0; luTableIndex < mpLiveRevengeProfile->maRelationshipTable.GetLength(); ++luTableIndex)
        {
            if (!mpLiveRevengeProfile->maRelationshipTable[luTableIndex].Validate())
            {
                return false;
            }
        }
        return true;
    }

    void LiveRevengeManager::AutoSaveLiveRevengeProfile()
    {
        mbProfileIsDirty = true;
    }

    // The game state loaded the profile into the buffer this manager owns: validate it, drop every
    // per-player mapping and the top-rival list, then rebuild the top rivals from the loaded table.
    void LiveRevengeManager::HandleLiveRevengeProfileLoadedEvent(const LiveRevengeProfile* lpLiveRevengeProfile)
    {
        CGS_ASSERT(lpLiveRevengeProfile, "lpLiveRevengeProfile");
        CGS_ASSERT(mpLiveRevengeProfile->ValidateProfile(), "mpLiveRevengeProfile->ValidateProfile()");
        CGS_ASSERT(IsTableValid(),
                   "If this fires then save/load has probably corrupted the profile. Deleting your save game will fix this for now (tell Alex Veal)\n");

        ResetRevengeTableMappings();
        ClearTopRivals();
        UpdateTopRivals();
        mbProfileIsDirty = false;
    }

    // ============================================================================
    // Per-player mapping
    // ============================================================================

    void LiveRevengeManager::ResetRevengeTableMappings()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            maPlayerToTableIndexData[liIndex].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
        }
    }

    LiveRevengeMappingEntry* LiveRevengeManager::FindMappingEntry(NetworkPlayerID lNetworkPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_NETWORK_PLAYERS; ++liIndex)
        {
            if (maPlayerToTableIndexData[liIndex].mPlayerID == lNetworkPlayerID)
            {
                return &maPlayerToTableIndexData[liIndex];
            }
        }
        return 0;
    }

    // Claim a free slot for the player, construct its sync messages, and flag the row for
    // re-upload if it is one of the top rivals.
    void LiveRevengeManager::AddMappingEntry(NetworkPlayerID lNetworkPlayerID, s32 liRevengeTableIndex)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        LiveRevengeMappingEntry* lpEntry = FindMappingEntry(KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE);
        if (!lpEntry)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Could not find PlayerID " << lNetworkPlayerID << " in " << __FUNCTION__;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return;
        }

        lpEntry->mPlayerID           = lNetworkPlayerID;
        lpEntry->miRevengeTableIndex = liRevengeTableIndex;
        lpEntry->mSendMessage.Construct();
        lpEntry->mRecvMessage.Construct();

        const s32 liTopRivalIndex = GetRivalTopIndex(liRevengeTableIndex);
        if (liTopRivalIndex != -1)
        {
            maDirtyTopRivals.SetBit(static_cast<u32>(liTopRivalIndex));
        }
    }

    void LiveRevengeManager::RemoveMappingEntry(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
        if (!lpEntry)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Could not find PlayerID " << lNetworkPlayerID << " in " << __FUNCTION__;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return;
        }

        lpEntry->mPlayerID           = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
        lpEntry->miRevengeTableIndex = -1;
        lpEntry->mSendMessage.Release();
        lpEntry->mSendMessage.Destruct();
        lpEntry->mRecvMessage.Release();
        lpEntry->mRecvMessage.Destruct();
    }

    LiveRevengeRelationship* LiveRevengeManager::GetNonConstRevengeRelationship(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        const LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
        if (!lpEntry)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Could not find PlayerID " << lNetworkPlayerID << " in " << __FUNCTION__;
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            return 0;
        }

        CGS_ASSERT(lpEntry->miRevengeTableIndex >= 0, "lpEntry->miRevengeTableIndex >= 0");
        CGS_ASSERT(lpEntry->miRevengeTableIndex < LiveRevengeProfile::KI_MAX_REVENGE_HISTORY,
                   "lpEntry->miRevengeTableIndex < LiveRevengeProfile::KI_MAX_REVENGE_HISTORY");

        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");
        return &lpProfile->maRelationshipTable[static_cast<u32>(lpEntry->miRevengeTableIndex)];
    }

    EActiveRaceCarIndex LiveRevengeManager::NetworkPlayerIDToActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            CGS_ARTIST_FILE, 1111 /* ARTIST 0x825477B8 */);
        return mpNetworkModule->GetActiveRaceCarIndex(lNetworkPlayerID);
    }

    void LiveRevengeManager::AddRelationshipToDebugMenu(s32 liLiveRevengeTableIndex)
    {
        mDebugComponent.AddPlayer(&mpLiveRevengeProfile->maRelationshipTable[static_cast<u32>(liLiveRevengeTableIndex)]);
    }

    void LiveRevengeManager::RemoveRelationshipFromDebugMenu(s32 liLiveRevengeTableIndex)
    {
        mDebugComponent.RemovePlayer(&GetProfile()->maRelationshipTable[static_cast<u32>(liLiveRevengeTableIndex)]);
    }

    // ============================================================================
    // Takedowns, paybacks and the GUI
    // ============================================================================

    // A takedown between the local player and a mapped rival: update the relationship, show
    // the live-revenge message (not for marked-man takedowns) and report a changed rival count.
    void LiveRevengeManager::UpdateLiveRevengeRelationShip(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                           EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                                           bool lbMarkedManTakedown,
                                                           BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer)
    {
        const s32 liNumberOfRivals = GetNumberOfRivals();
        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            __FILE__, 731 /* ARTIST 0x82563088 */);
        CGS_ASSERT_AT(lpOutputBuffer,
            "lpOutputBuffer",
            __FILE__, 732 /* ARTIST 0x825630AC */);

        if (mpNetworkManager->GetPlayerManager()->IsLocalPlayer(mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex)))
        {
            LiveRevengeRelationship* lpRelationship =
                GetNonConstRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex));
            if (lpRelationship)
            {
                lpRelationship->AddTakedownByLocalPlayer(lbMarkedManTakedown);
            }
            if (!lbMarkedManTakedown)
            {
                DisplayPlayerTakedownMessage(lpOutputBuffer, leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex);
            }
            AutoSaveLiveRevengeProfile();
        }
        else if (mpNetworkManager->GetPlayerManager()->IsLocalPlayer(mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex)))
        {
            LiveRevengeRelationship* lpRelationship =
                GetNonConstRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex));
            if (lpRelationship)
            {
                lpRelationship->AddTakedownByRival(lbMarkedManTakedown);
            }
            if (!lbMarkedManTakedown)
            {
                DisplayRivalTakedownMessage(lpOutputBuffer, leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex);
            }
            AutoSaveLiveRevengeProfile();
        }

        if (GetNumberOfRivals() != liNumberOfRivals)
        {
            mpNetworkManager->GetStatsManager()->HandleNewNumberOfRivals();
        }
    }

    // A payback between the local player and a rival: an initialised payback counts as dealt,
    // a crashed (succeeded) one as scored, credited to whichever side dealt it.
    void LiveRevengeManager::UpdatePaybacksData(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                                EDirtyTrickStatus leDirtyTrickStatus)
    {
        CGS_ASSERT_AT(leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
            "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0",
            __FILE__, 812 /* ARTIST 0x8255FC78 */);
        CGS_ASSERT_AT(leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
            "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
            __FILE__, 813 /* ARTIST 0x8255FC9C */);
        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
            "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0",
            __FILE__, 815 /* ARTIST 0x8255FCC0 */);
        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
            "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
            __FILE__, 816 /* ARTIST 0x8255FCE4 */);

        NetworkPlayerID lLocalPlayerNetworkID = CgsNetwork::K_INVALID_PLAYER_ID;
        CGS_ASSERT_AT(mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerNetworkID),
            "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerNetworkID )",
            __FILE__, 820 /* ARTIST 0x8255FD34 */);

        const bool lbLocalPlayerWasPaybackVictim =
            mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerNetworkID) == leVictimActiveRaceCarIndex;
        const NetworkPlayerID lRivalPlayerID = lbLocalPlayerWasPaybackVictim
            ? mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex)
            : mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex);
        CGS_ASSERT_AT(lRivalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
            "lRivalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            __FILE__, 833 /* ARTIST 0x8255FD9C */);

        LiveRevengeRelationship* lpRelationship = GetNonConstRevengeRelationship(lRivalPlayerID);
        CGS_ASSERT_AT(lpRelationship,
            "lpRelationship",
            __FILE__, 837 /* ARTIST 0x8255FDD0 */);

        // The status enumerators are E_DIRTY_TRICK_STATUS_INITIALISED (2) and
        // E_DIRTY_TRICK_STATUS_CRASHED (4); the enum's home names only its zero value so far.
        switch (static_cast<s32>(leDirtyTrickStatus))
        {
        case 2:
            if (lbLocalPlayerWasPaybackVictim)
            {
                lpRelationship->AddPaybackDealtByRival();
            }
            else
            {
                lpRelationship->AddPaybackDealtByLocalPlayer();
            }
            AutoSaveLiveRevengeProfile();
            break;

        case 4:
            if (lbLocalPlayerWasPaybackVictim)
            {
                lpRelationship->AddPaybackScoredByRival();
            }
            else
            {
                lpRelationship->AddPaybackScoredByLocalPlayer();
            }
            AutoSaveLiveRevengeProfile();
            break;

        default:
            break;
        }
    }

    // Header inline on the console (see the class declaration).
    void LiveRevengeManager::HandlePaybackInitialisedEvent(const BrnNetworkModuleIO::NetworkInPaybackIntialised* lpPaybackEvent)
    {
        CGS_ASSERT_AT(lpPaybackEvent,
            "lpPaybackEvent", KAC_ARTIST_ASSERT_HEADER, 581 /* ARTIST 0x8256195C */);
        UpdatePaybacksData(lpPaybackEvent->mePaybackAggressorIndex, lpPaybackEvent->mePaybackVictimIndex,
                           static_cast<EDirtyTrickStatus>(2));   // E_DIRTY_TRICK_STATUS_INITIALISED
    }

    // Header inline on the console (see the class declaration).
    void LiveRevengeManager::HandlePaybackSucceededEvent(const BrnNetworkModuleIO::NetworkInPaybackSucceeded* lpPaybackEvent)
    {
        CGS_ASSERT_AT(lpPaybackEvent,
            "lpPaybackEvent", KAC_ARTIST_ASSERT_HEADER, 595 /* ARTIST 0x825619CC */);
        UpdatePaybacksData(lpPaybackEvent->mePaybackAggressorIndex, lpPaybackEvent->mePaybackVictimIndex,
                           static_cast<EDirtyTrickStatus>(4));   // E_DIRTY_TRICK_STATUS_CRASHED
    }

    // The local player took the rival down: classify the relationship change for the HUD
    // (a new rivalry, a reignited one, extending a lead, or settling the score).
    void LiveRevengeManager::DisplayPlayerTakedownMessage(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                                          EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                          EActiveRaceCarIndex leVictimActiveRaceCarIndex)
    {
        CGS_ASSERT(leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        const EActiveRaceCarIndex leLocalActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerID);
        CGS_ASSERT(leAggressorActiveRaceCarIndex == leLocalActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex == leLocalActiveRaceCarIndex");

        const LiveRevengeRelationship* lpRelationship =
            GetRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leVictimActiveRaceCarIndex));
        if (!lpRelationship)
        {
            return;
        }

        NetworkToGuiLiveRevengeUpdate::LiveRevengeStatus leLiveRevengeStatus;
        if (lpRelationship->GetTotalTakedowns() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_NEWRIVALRY;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->GetCurrentScoreForLocalPlayer() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_REIGNITED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->IsPlayerAheadInCurrentRelationship())
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_AHEAD;
        }
        else
        {
            CGS_ASSERT(lpRelationship->IsCurrentRelationshipEqual(), "lpRelationship->IsCurrentRelationshipEqual()");
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_SETTLED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_REMOVED, GetNumberOfRivals());
        }

        lpOutputBuffer->GetNetworkToGuiInterface()->AddLiveRevengeUpdate(
            leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex, leLiveRevengeStatus,
            lpRelationship->GetCurrentScoreForLocalPlayer());
    }

    // The rival took the local player down: the mirror of DisplayPlayerTakedownMessage, scored
    // from the rival's side (falling behind instead of extending a lead).
    void LiveRevengeManager::DisplayRivalTakedownMessage(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                                         EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                                         EActiveRaceCarIndex leVictimActiveRaceCarIndex)
    {
        CGS_ASSERT_AT(lpOutputBuffer,
            "lpOutputBuffer",
            __FILE__, 1003 /* ARTIST 0x82560060 */);

        NetworkPlayerID lLocalPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        const EActiveRaceCarIndex leLocalActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lLocalPlayerID);
        CGS_ASSERT_AT(leVictimActiveRaceCarIndex == leLocalActiveRaceCarIndex,
            "leVictimActiveRaceCarIndex == leLocalActiveRaceCarIndex",
            __FILE__, 1012 /* ARTIST 0x825600A8 */);
        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
            "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0",
            __FILE__, 1015 /* ARTIST 0x825600CC */);
        CGS_ASSERT_AT(leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
            "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
            __FILE__, 1016 /* ARTIST 0x825600F0 */);

        const LiveRevengeRelationship* lpRelationship =
            GetRevengeRelationship(mpNetworkModule->GetNetworkPlayerID(leAggressorActiveRaceCarIndex));
        if (!lpRelationship)
        {
            return;
        }

        NetworkToGuiLiveRevengeUpdate::LiveRevengeStatus leLiveRevengeStatus;
        if (lpRelationship->GetTotalTakedowns() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_NEWRIVALRY;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->GetCurrentScoreForRival() == 1)
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_REIGNITED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_ADDED, GetNumberOfRivals());
        }
        else if (lpRelationship->IsRivalAheadInCurrentRelationship())
        {
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_BEHIND;
        }
        else
        {
            CGS_ASSERT_AT(lpRelationship->IsCurrentRelationshipEqual(),
                "lpRelationship->IsCurrentRelationshipEqual()",
                __FILE__, 1057 /* ARTIST 0x82560188 */);
            leLiveRevengeStatus = NetworkToGuiLiveRevengeUpdate::E_LIVEREVENGESTATUS_SETTLED;
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_RIVAL_REMOVED, GetNumberOfRivals());
        }

        lpOutputBuffer->GetNetworkToGuiInterface()->AddLiveRevengeUpdate(
            leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex, leLiveRevengeStatus,
            lpRelationship->GetCurrentScoreForRival());
    }

    // ============================================================================
    // Top rivals and the server upload
    // ============================================================================

    // qsort comparator: descending rival score.
    int LiveRevengeManager::_SortTopRivals(const void* lpRival1, const void* lpRival2)
    {
        const LiveRevengeQSortData* lpRivalDetails1 = static_cast<const LiveRevengeQSortData*>(lpRival1);
        const LiveRevengeQSortData* lpRivalDetails2 = static_cast<const LiveRevengeQSortData*>(lpRival2);

        if (lpRivalDetails2->miRivalScore > lpRivalDetails1->miRivalScore)
        {
            return 1;
        }
        if (lpRivalDetails2->miRivalScore < lpRivalDetails1->miRivalScore)
        {
            return -1;
        }
        return 0;
    }

    // Rank every relationship by total takedowns and keep the best ten; a slot whose row
    // changed is flagged for re-upload.
    void LiveRevengeManager::UpdateTopRivals()
    {
        LiveRevengeQSortData laRivals[LiveRevengeProfile::KI_MAX_REVENGE_HISTORY];

        LiveRevengeProfile* lpProfile = GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const s32 liTotalRivals = static_cast<s32>(lpProfile->maRelationshipTable.GetLength());
        // rw::core::stdc::Min, spelled in place.
        miNumberOfTopRivals = (liTotalRivals < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER)
                                  ? liTotalRivals : KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER;

        for (s32 liRivalIndex = 0; liRivalIndex < liTotalRivals; ++liRivalIndex)
        {
            laRivals[liRivalIndex].miTableIndex = liRivalIndex;
            laRivals[liRivalIndex].miRivalScore =
                lpProfile->maRelationshipTable[static_cast<u32>(liRivalIndex)].GetTotalTakedowns();
        }

        std::qsort(laRivals, static_cast<size_t>(liTotalRivals), sizeof(LiveRevengeQSortData),
                   &LiveRevengeManager::_SortTopRivals);

        for (s32 liRivalIndex = 0; liRivalIndex < miNumberOfTopRivals; ++liRivalIndex)
        {
            if (maTopIndexes[liRivalIndex] != laRivals[liRivalIndex].miTableIndex)
            {
                maTopIndexes[liRivalIndex] = laRivals[liRivalIndex].miTableIndex;
                maDirtyTopRivals.SetBit(static_cast<u32>(liRivalIndex));
            }
        }
    }

    // The top-ten slot holding table row liTableIndex, or -1.
    s32 LiveRevengeManager::GetRivalTopIndex(s32 liTableIndex) const
    {
        for (s32 liRivalIndex = 0; liRivalIndex < miNumberOfTopRivals; ++liRivalIndex)
        {
            if (maTopIndexes[liRivalIndex] == liTableIndex)
            {
                return liRivalIndex;
            }
        }
        return -1;
    }

    void LiveRevengeManager::ClearTopRivals()
    {
        for (s32 liRivalIndex = 0; liRivalIndex < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++liRivalIndex)
        {
            maTopIndexes[liRivalIndex] = -1;
        }
        maDirtyTopRivals.UnSetAll();
    }

    // Upload every dirty top rival to the server in one custom command. While the custom-commands
    // component is busy the upload stays pending; with nothing dirty the auto-login step is done.
    void LiveRevengeManager::SendLiveRevengeRivalsToServer()
    {
        if (mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent()->GetStatus()
            != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
        {
            meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_PENDING;
            return;
        }

        LiveRevengeProfile* lpProfile = mpLiveRevengeProfile;
        CGS_ASSERT(lpProfile, "lpProfile");

        UpdateTopRivals();

        ServerGeneratedTypes::RivalDataT laRivalData[KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER];
        s32                              laiRivalIDs[KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER];
        s32                              liNumberToUpload = 0;

        for (CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::Iterator lIterator =
                 maDirtyTopRivals.Begin();
             lIterator != maDirtyTopRivals.End(); ++lIterator)
        {
            const s32 liTopIndex = lIterator.GetIndex();
            laiRivalIDs[liNumberToUpload] = liTopIndex;

            const LiveRevengeRelationship* lpRelationship =
                &lpProfile->maRelationshipTable[static_cast<u32>(maTopIndexes[liTopIndex])];
            const CommonRelationship*        lpStats     = lpRelationship->GetOverallStats();
            ServerGeneratedTypes::RivalDataT* lpRivalData = &laRivalData[liNumberToUpload];

            const char* lpcRivalName = lpRelationship->GetRivalName()->GetPlayerName();
            CGS_ASSERT(strlen(lpcRivalName) < sizeof(lpRivalData->macPers), "String too long");
            strncpy(lpRivalData->macPers, lpcRivalName, sizeof(lpRivalData->macPers));
            ++liNumberToUpload;

            lpRivalData->miRivalryStatus         = lpRelationship->GetCurrentScoreForLocalPlayer();
            lpRivalData->miTimesBattled          = lpRelationship->GetTotalEvents();
            lpRivalData->miTakedownsFor          = lpStats->mPlayerStats.miTakedowns;
            lpRivalData->miTakedownsAgainst      = lpStats->mRivalStats.miTakedowns;
            lpRivalData->miMugshotsFor           = lpStats->mPlayerStats.miScalps;
            lpRivalData->miMugshotsAgainst       = lpStats->mRivalStats.miScalps;
            lpRivalData->miMarksFor              = lpStats->mPlayerStats.miMarks;
            lpRivalData->miMarksAgainst          = lpStats->mRivalStats.miMarks;
            lpRivalData->miPaybacksDealtFor      = lpStats->mPlayerStats.miPaybacksDealt;
            lpRivalData->miPaybacksDealtAgainst  = lpStats->mRivalStats.miPaybacksDealt;
            lpRivalData->miPaybacksScoredFor     = lpStats->mPlayerStats.miPaybacksScored;
            lpRivalData->miPaybacksScoredAgainst = lpStats->mRivalStats.miPaybacksScored;
        }

        if (liNumberToUpload > 0)
        {
            mpNetworkManager->GetServerInterface()->GetCustomCommandsComponent()->UploadLiveRevengeData(
                laRivalData, laiRivalIDs, liNumberToUpload);
            maDirtyTopRivals.UnSetAll();
            meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS;
        }
        else
        {
            mpNetworkManager->OnAutoLoginProcessComplete(1);
        }
    }

    // Look the named player up in the current game and build the unique id (name + XUID) from its
    // player parameters.
    void LiveRevengeManager::GetUniqueIDByName(CgsNetwork::PlayerName* lpPlayerName,
                                               LiveRevengeRelationship::UniquePlayerID* lpUniqueID)
    {
        PlayerParams lPlayerParams;

        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager",
            __FILE__, 1443 /* ARTIST 0x82568F38 */);
        lPlayerParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerName(
            lpPlayerName->GetPlayerName(), &lPlayerParams);
        lpUniqueID->Construct(&lPlayerParams);
    }

    // ============================================================================
    // Relationship sync messages
    // ============================================================================

    // A rival's copy of our relationship arrived (already flipped to our side): merge it.
    void LiveRevengeManager::SyncMessageArrivedCallback(LiveRevengeSyncMessage* lpMessage, NetworkPlayerID lRemotePlayerID)
    {
        LiveRevengeRelationship lRemoteRelationship;

        CGS_ASSERT_AT(lpMessage,
            "lpMessage", __FILE__, 1364 /* ARTIST 0x8255B2A8 */);
        CGS_ASSERT_AT(lpMessage->Retrieve(&lRemoteRelationship),
            "lpMessage->Retrieve(&lRemoteRelationship)", __FILE__, 1366 /* ARTIST 0x8255B2DC */);


        LiveRevengeRelationship* lpLocalRelationship = GetNonConstRevengeRelationship(lRemotePlayerID);
        CGS_ASSERT_AT(lpLocalRelationship,
            "lpLocalRelationship", __FILE__, 1369 /* ARTIST 0x8255B310 */);

        lpLocalRelationship->Merge(&lRemoteRelationship);
        AutoSaveLiveRevengeProfile();
    }

    void LiveRevengeManager::_SyncMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                         NetworkPlayerID lSendingPlayerID, void* lpData)
    {
        LiveRevengeManager*     lpManager            = static_cast<LiveRevengeManager*>(lpData);
        LiveRevengeSyncMessage* lpLiveRevengeMessage = static_cast<LiveRevengeSyncMessage*>(lpMessage);
        CGS_ASSERT_AT(lpManager,
            "lpManager", __FILE__, 1343 /* ARTIST 0x82560210 */);
        CGS_ASSERT_AT(lpMessage,
            "lpMessage", __FILE__, 1344 /* ARTIST 0x82560234 */);

        lpManager->SyncMessageArrivedCallback(lpLiveRevengeMessage, lSendingPlayerID);
    }

    void LiveRevengeManager::_SyncMessageDeliveredCallback(bool /*lbSuccess*/, bool lbFakeNack,
                                                           CgsNetwork::SignalMessage* /*lpAck*/,
                                                           NetworkPlayerID /*lRecvingPlayerID*/, void* /*lpData*/)
    {
        if (lbFakeNack)
        {
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "WARNING: Fack Nack found in ";
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << __FUNCTION__;
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "\n";
        }
    }
} // namespace BrnNetwork

#undef CGS_ARTIST_FILE
