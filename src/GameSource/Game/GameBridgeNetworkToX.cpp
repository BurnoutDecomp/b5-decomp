// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeNetworkToX.cpp
//
// The BrnGame::BrnGameModule network-bridge family. Each per-frame bridge reads the
// network module's OUTPUT buffer (BrnNetwork::BrnNetworkModuleIO::OutputBuffer /
// its NetworkToGuiInterface + NetworkEventQueue + InGamePlayerStatusInterface, the
// committed homes) and republishes its contents into the GUI + game-state subsystems --
// the mirror image of the controller bridges in GameBridgeControllerToX.cpp and the
// replay bridges in GameBridgeReplayToX.cpp.
//
// Reconstructed store-for-store from the console build:
//   TranslateNetworkInterfaceToGuiEvents, TranslateNetworkEventsToGameEvents,
//   TranslateNetworkEventsToGuiEvents (with its inlined scoreboard-response arm) and
//   BridgeNetworkToGui.
//
// WHAT IS NAMED AND WHAT IS NOT:
//   * The network side is reached only through the real OutputBuffer accessors
//     (GetNetworkEventQueue / GetNetworkToGuiInterface / GetInGamePlayerStatusInterface /
//     GetOnlineLobbyPlayerStatusInterface / GetGuiEventQueue / IsPlaying / IsConnected /
//     GetPlayerActiveRaceCarIndex) and the interfaces' own members. Nothing is read at a
//     console offset of the IO buffer, whose host layout is pointer-widened.
//   * The records drained from the NetworkEventQueue are the network OUT events of
//     BrnNetworkOutEventTypeDefs.h and are read by member. Three of them still come from
//     homes that do not name every field (NetworkOutPlayerAddedEvent and
//     NetworkOutPlayerRemovedEvent hold their payload as opaque spans, and
//     NetworkOutRecvRoadRulesPBEvent is one opaque block); those fields are read at their
//     record offsets and marked at the read.
//   * The game-state events with a home in BrnGameEvents.h are built through their members
//     and posted with their own size. The rest of the game-state records are still opaque
//     in their homes; they are built at their record offsets inside a buffer of the size the
//     console posts.
//   * The GUI records are queued through the shared BrnGame::PushGuiEvent (the whole object
//     at offset 0, sizeof(T) as the size), byte-for-byte what the console publisher stores.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeNetworkToX.h"
#include "GameSource/Game/GameBridgeGameStateToX.h"    // BrnGame::PushGuiEvent (the shared GUI event push)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<14000/1536/32768/4096,16>
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                 // CgsGui::GuiEvent<N> (tag-only GUI events)
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::StrStreamBase (mugshot debug spew)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameSource/Network/BrnNetworkModuleIO.h"                  // BrnNetwork::BrnNetworkModuleIO::OutputBuffer + interfaces
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"          // the network OUT-event records
#include "GameSource/GameState/BrnGameStateModuleIO.h"              // BrnGameState::GameStateModuleIO::PreWorldInputBuffer + events
#include "GameSource/GameState/BrnCgsPlayerName.h"                  // CgsNetwork::PlayerName (Mole4Avril debug roster)
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsIDCompress (the overlay wait-finish record)
#include "GameSource/GameState/BrnGameEvents.h"                     // the game-state network events (OnlinePlayerAddedEvent ...)
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // BrnGameState::ImageToSaveEvent (event 156)
#include "GameSource/Network/SharedIO/BrnNetworkToGuiIOInterfaces.h" // NetworkToGuiInterface (live-revenge update queue)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"     // InGamePlayerStatusInterface / Data
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // OnlineLobbyPlayerStatusInterface / LobbyPlayerStatusData

#include <cstddef>   // offsetof (record layout pins)
#include <cstring>   // std::memcpy / std::strncpy

namespace
{
    // The three debug switches BridgeNetworkToGui reads. Their real homes are static members of
    // the GUI records it posts: BrnGui::GuiEventNetworkPlayerList::msbWorstCaseHudActive,
    // BrnGui::GuiEventNetworkPlayerStatus::msbWorstCaseHudActive and
    // BrnGui::GuiEventNetworkPlayerStatus::msbStatusSwitch, written only by
    // BrnGui::GuiDebugComponent::UpdateWorstCaseHUD. Those records are still opaque in
    // BrnGuiDemangledEventTypes.h, so the flags are held here under the member names until the
    // statics are declared there. Nothing else stores to them, so they start false.
    bool msbPlayerListWorstCaseHudActive   = false;   // GuiEventNetworkPlayerList::msbWorstCaseHudActive
    bool msbPlayerStatusWorstCaseHudActive = false;   // GuiEventNetworkPlayerStatus::msbWorstCaseHudActive
    bool msbPlayerStatusSwitch             = false;   // GuiEventNetworkPlayerStatus::msbStatusSwitch

    // Layout pins for the game-state records this TU builds by member and posts with sizeof.
    typedef BrnGameState::GameStateModuleIO::OnlinePlayerAddedEvent               PinAdded;
    typedef BrnGameState::GameStateModuleIO::OnlinePlayerRemovedEvent             PinRemoved;
    typedef BrnGameState::GameStateModuleIO::OnlinePlayerFinalisedEvent           PinFinalised;
    typedef BrnGameState::GameStateModuleIO::ChangeNetworkCarEvent                PinChangeCar;
    typedef BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedEvent        PinDisconnected;
    typedef BrnGameState::GameStateModuleIO::BuddyRemovedEvent                    PinBuddyRemoved;
    typedef BrnGameState::GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent PinRoadRulesPB;
    typedef BrnGameState::GameStateModuleIO::OnlineRoadRulesUploadedEvent         PinRoadRulesUploaded;
    typedef BrnGameState::GameStateModuleIO::OnlineRoadRulesDownloadedEvent       PinRoadRulesDownloaded;
    typedef BrnGameState::GameStateModuleIO::OnlineRoadRulesConnectInfoEvent      PinRoadRulesConnect;
    typedef BrnGameState::GameStateModuleIO::FreeburnChallengeActionSuccessEvent  PinActionSuccess;
    typedef BrnGameState::GameStateModuleIO::FreeburnChallengeResetEvent          PinReset;
    typedef BrnGameState::GameStateModuleIO::ActiveFburnChallengeEvent            PinActiveChallenge;
    typedef BrnGameState::GameStateModuleIO::FburnChallengeSuccessUpdateEvent     PinSuccessUpdate;
    typedef BrnGameState::GameStateModuleIO::FburnChallengeSuccessEvent           PinSuccess;
    typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData                PinStatus;
    static_assert(offsetof(PinAdded, mModelID)               == 0x00, "OnlinePlayerAddedEvent mModelID @ +0x00");
    static_assert(offsetof(PinAdded, mWheelID)               == 0x08, "OnlinePlayerAddedEvent mWheelID @ +0x08");
    static_assert(offsetof(PinAdded, mNetworkPlayerID)       == 0x10, "OnlinePlayerAddedEvent mNetworkPlayerID @ +0x10");
    static_assert(sizeof(PinAdded)        == 40, "event 127 is posted as 40 bytes");
    static_assert(sizeof(PinRemoved)      == 8,  "event 129 is posted as 8 bytes");
    static_assert(sizeof(PinFinalised)    == 4,  "event 128 is posted as 4 bytes");
    static_assert(sizeof(PinChangeCar)    == 32, "event 7 is posted as 32 bytes");
    static_assert(sizeof(PinDisconnected) == 4,  "event 121 is posted as 4 bytes");
    static_assert(sizeof(PinBuddyRemoved) == 16, "event 150 is posted as 16 bytes");
    static_assert(offsetof(PinRoadRulesPB, mPersonalBestPlayerID)       == 0x38, "event 130 player id @ +0x38");
    static_assert(offsetof(PinRoadRulesPB, mPersonalBestChallengeIndex) == 0x3C, "event 130 challenge index @ +0x3C");
    static_assert(offsetof(PinRoadRulesPB, mbWasPBByFriend)             == 0x40, "event 130 friend flag @ +0x40");
    static_assert(sizeof(PinRoadRulesPB)         == 72, "event 130 is posted as 72 bytes");
    static_assert(sizeof(PinRoadRulesUploaded)   == 8,  "event 131 is posted as 8 bytes");
    static_assert(sizeof(PinRoadRulesDownloaded) == 4,  "event 132 is posted as 4 bytes");
    static_assert(sizeof(PinRoadRulesConnect)    == 4,  "event 133 is posted as 4 bytes");
    static_assert(sizeof(PinActionSuccess)       == 16, "event 165 is posted as 16 bytes");
    static_assert(sizeof(PinReset)               == 16, "events 166 / 167 are posted as 16 bytes");
    static_assert(sizeof(PinActiveChallenge)     == 48, "event 173 is posted as 48 bytes");
    static_assert(sizeof(PinSuccessUpdate)       == 24, "event 171 is posted as 24 bytes");
    static_assert(sizeof(PinSuccess)             == 20, "event 172 is posted as 20 bytes");
    static_assert(sizeof(BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams) == 0x9C, "event 57 is posted as 156 bytes");
    static_assert(sizeof(PinStatus) == 312, "InGamePlayerStatusData record stride (0x138)");
    static_assert(offsetof(PinStatus, mPlayerName)          == 0x100, "mPlayerName @ +0x100");
    static_assert(offsetof(PinStatus, mNetworkPlayerID)     == 0x110, "mNetworkPlayerID @ +0x110");
    static_assert(offsetof(PinStatus, meActiveRaceCarIndex) == 0x114, "meActiveRaceCarIndex @ +0x114");
    static_assert(offsetof(PinStatus, mbIsInLocalGameWorld) == 0x12F, "mbIsInLocalGameWorld @ +0x12F");
    static_assert(sizeof(BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData) == 56, "LobbyPlayerStatusData record stride (0x38)");

    // Queue one game-state record: the whole object, its own size, under the console event id.
    template <class Q, class T>
    inline void PostGameEvent(Q* lpQueue, const T& lrEvent, s32 liEventType)
    {
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrEvent), liEventType,
                          static_cast<s32>(sizeof(T)));
    }
}

namespace BrnGame
{
    using BrnNetwork::BrnNetworkModuleIO::OutputBuffer;
    using BrnNetwork::BrnNetworkModuleIO::NetworkEventQueue;
    using BrnNetwork::BrnNetworkModuleIO::NetworkToGuiInterface;
    using BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface;
    using BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData;
    using BrnNetwork::BrnNetworkModuleIO::OnlineLobbyPlayerStatusInterface;
    using BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData;

    // Every GUI record below is queued through the shared BrnGame::PushGuiEvent
    // (GameBridgeGameStateToX.h): the whole object at offset 0, with the type's own
    // GetEventType() and sizeof(T) -- byte-for-byte what the console publisher stores.
    // The console reaches it as a member of the CgsGui::GuiModule embedded at +7252512
    // (never dereferenced; the body does not read `this`), which nothing constructs on
    // this build, so the queue is written directly -- the in-tree idiom for this case.

    // The canonical GUI event records model their >= 12-byte, 4-aligned wire records as
    // `CgsGui::GuiEvent<N> + payload`, which puts the payload 12 bytes late. The console
    // store maps below are all record-relative, so the bytes are written from the event
    // object's OWN address.
    template <class T>
    static inline unsigned char* RecordBytes(T& lrEvent)
    {
        return reinterpret_cast<unsigned char*>(&lrEvent);
    }

    // =========================================================================
    // TranslateNetworkInterfaceToGuiEvents
    // Walk the NetworkToGuiInterface's live-revenge update queue (the length is read once,
    // before the loop) and post one BrnGui::GuiLiveRevengeUpdateEvent per update.
    // =========================================================================
    void BrnGameModule::TranslateNetworkInterfaceToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput, const NetworkToGuiInterface* lpNetworkToGuiInterface)
    {
        // The queue accessor is inlined on the console: the length is its miLength, and each
        // element is fetched through BaseEventQueue::GetEvent(index) (with its three asserts).
        const NetworkToGuiInterface::LiveRevengeUpdateQueue* lpUpdateQueue =
            lpNetworkToGuiInterface->GetLiveRevengeUpdateQueue();
        const s32 liCount = lpUpdateQueue->GetLength();

        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            const BrnNetwork::NetworkToGuiLiveRevengeUpdate& lUpdate = lpUpdateQueue->GetEvent(liIndex);

            // The GUI record orders the four fields differently from the network update.
            BrnGui::GuiLiveRevengeUpdateEvent lEvent;
            lEvent.miDifference                  = lUpdate.miDifference;
            lEvent.meNewStatus                   = lUpdate.meNewStatus;
            lEvent.meAggressorActiveRaceCarIndex = lUpdate.meAggressorActiveRaceCarIndex;
            lEvent.meVictimActiveRaceCarIndex    = lUpdate.meVictimActiveRaceCarIndex;

            PushGuiEvent(lEvent, lpGuiInput);
        }
    }

    // =========================================================================
    // TranslateNetworkEventsToGameEvents
    // Drain the network OUTPUT buffer's NetworkEventQueue and translate each game-state-bound
    // network event into the matching game-state event, AddEvent'd into the PreWorld input
    // buffer's game-event queue under the console's event id and record size.
    // =========================================================================
    void BrnGameModule::TranslateNetworkEventsToGameEvents(
        BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpGameStateInput, const OutputBuffer* lpNetworkOutput)
    {
        namespace NetIO = BrnNetwork::BrnNetworkModuleIO;
        namespace GsIO  = BrnGameState::GameStateModuleIO;

        const NetworkEventQueue* lpNetworkEventQueue = lpNetworkOutput->GetNetworkEventQueue();
        CGS_ASSERT(lpNetworkEventQueue != 0, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventType = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            switch (liEventType)
            {
                case NetIO::NetworkOutInviteRequest::KI_EVENT_TYPE:                         // 6
                {
                    const NetIO::NetworkOutInviteRequest* lpInviteRequestEvent =
                        reinterpret_cast<const NetIO::NetworkOutInviteRequest*>(lpEvent);
                    CGS_ASSERT(lpInviteRequestEvent != 0, "lpInviteRequestEvent");
                    NetIO::InviteOrJoinParams lInviteParams;
                    std::memcpy(&lInviteParams, &lpInviteRequestEvent->mInviteParams, sizeof(lInviteParams));
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lInviteParams, 57);
                    break;
                }
                case NetIO::NetworkOutInstantFreeburnEvent::KI_EVENT_TYPE:                  // 12
                {
                    const NetIO::NetworkOutInstantFreeburnEvent* lpInstantFreeburnEvent =
                        reinterpret_cast<const NetIO::NetworkOutInstantFreeburnEvent*>(lpEvent);
                    CGS_ASSERT(lpInstantFreeburnEvent != 0, "lpInstantFreeburnEvent");
                    const bool lbIsDoingInstantFreeburn = lpInstantFreeburnEvent->mbIsDoingInstantFreeburn;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lbIsDoingInstantFreeburn, 151);
                    break;
                }
                case NetIO::NetworkOutBuddyRemovedEvent::KI_EVENT_TYPE:                     // 14
                {
                    const NetIO::NetworkOutBuddyRemovedEvent* lpBuddyRemovedEvent =
                        reinterpret_cast<const NetIO::NetworkOutBuddyRemovedEvent*>(lpEvent);
                    CGS_ASSERT(lpBuddyRemovedEvent != 0, "lpBuddyRemovedEvent");
                    GsIO::BuddyRemovedEvent lBuddyRemoved;
                    lBuddyRemoved.mRemovedBuddyName = lpBuddyRemovedEvent->mRemovedBuddyName;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lBuddyRemoved, GsIO::E_EVENT_BUDDY_REMOVED);
                    break;
                }
                case 15:  // NetworkOutGameParamsChanged -> {game mode, ranked}
                {
                    // The record's home names its +0x1B8 word miMaxPlayers and its +0x1DC byte
                    // mbIsRankedGame; they are the reference's meGameMode and mbRanked (the GUI
                    // translator's copy into GuiEventNetworkGameParams maps them by name).
                    const NetIO::NetworkOutGameParamsChanged* lpGameParamEvent =
                        reinterpret_cast<const NetIO::NetworkOutGameParamsChanged*>(lpEvent);
                    CGS_ASSERT(lpGameParamEvent != 0, "lpEvent");
                    unsigned char lOut[8];
                    lOut[4] = lpGameParamEvent->mbIsRankedGame;                          // mbRanked
                    *reinterpret_cast<s32*>(lOut) = lpGameParamEvent->miMaxPlayers;      // meGameMode
                    CGS_ASSERT(lpGameStateInput != 0, "lpGameStateInput");
                    CGS_ASSERT(lpGameStateInput->GetGameEventQueue() != 0,
                               "lpGameStateInput->GetGameEventQueue()");
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 125, 8);
                    break;
                }
                case 16:  // NetworkOutPlayerAddedEvent -> OnlinePlayerAddedEvent
                {
                    // The record's home names only mNetworkPlayerID (+0x10); its other fields sit
                    // in opaque spans and are read at their offsets:
                    //   +0x00 mModelID, +0x08 mWheelID, +0x14 meTeam, +0x18 the console-only float,
                    //   +0x1C mu16CarColourIndex, +0x1E mu16CarPaintFinishIndex, +0x20 mbIsLocalPlayer.
                    const NetIO::NetworkOutPlayerAddedEvent* lpPlayerAddedEvent =
                        reinterpret_cast<const NetIO::NetworkOutPlayerAddedEvent*>(lpEvent);
                    const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpPlayerAddedEvent);
                    GsIO::OnlinePlayerAddedEvent lAdded;
                    lAdded.SetNetworkPlayerID(lpPlayerAddedEvent->mNetworkPlayerID);
                    lAdded.mModelID                = *reinterpret_cast<const CgsID*>(lpRecord + 0x00);
                    lAdded.mf18                    = *reinterpret_cast<const f32*>(lpRecord + 0x18);
                    lAdded.mWheelID                = *reinterpret_cast<const CgsID*>(lpRecord + 0x08);
                    lAdded.meTeam                  = *reinterpret_cast<const GsIO::EPlayerTeam*>(lpRecord + 0x14);
                    lAdded.mbIsLocalPlayer         = lpRecord[0x20] != 0;
                    lAdded.mu16CarColourIndex      = *reinterpret_cast<const u16*>(lpRecord + 0x1C);
                    lAdded.mu16CarPaintFinishIndex = *reinterpret_cast<const u16*>(lpRecord + 0x1E);
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lAdded, GsIO::E_EVENT_ONLINE_PLAYER_ADDED);
                    break;
                }
                case 20:  // NetworkOutPlayerFinalisedEvent -> OnlinePlayerFinalisedEvent
                {
                    const NetIO::NetworkOutPlayerFinalisedEvent* lpPlayerFinalisedEvent =
                        reinterpret_cast<const NetIO::NetworkOutPlayerFinalisedEvent*>(lpEvent);
                    GsIO::OnlinePlayerFinalisedEvent lFinalised;
                    lFinalised.SetNetworkPlayerID(lpPlayerFinalisedEvent->mNetworkPlayerID);
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lFinalised, GsIO::E_EVENT_ONLINE_PLAYER_FINALISED);
                    break;
                }
                case 17:  // NetworkOutPlayerRemovedEvent -> OnlinePlayerRemovedEvent
                {
                    // mbIsLocalPlayerInGame sits at +0x14 in the record's opaque tail.
                    const NetIO::NetworkOutPlayerRemovedEvent* lpPlayerRemovedEvent =
                        reinterpret_cast<const NetIO::NetworkOutPlayerRemovedEvent*>(lpEvent);
                    const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpPlayerRemovedEvent);
                    GsIO::OnlinePlayerRemovedEvent lRemoved;
                    lRemoved.SetNetworkPlayerID(lpPlayerRemovedEvent->mNetworkPlayerID);
                    lRemoved.mbIsLocalPlayerInGame = lpRecord[0x14] != 0;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lRemoved, GsIO::E_EVENT_ONLINE_PLAYER_REMOVED);
                    break;
                }
                case NetIO::NetworkOutPlayerChangedCarEvent::KI_EVENT_TYPE:                 // 18
                {
                    const NetIO::NetworkOutPlayerChangedCarEvent* lpChangedCarEvent =
                        reinterpret_cast<const NetIO::NetworkOutPlayerChangedCarEvent*>(lpEvent);
                    GsIO::ChangeNetworkCarEvent lChangeCar;
                    lChangeCar.SetNetworkPlayerID(lpChangedCarEvent->GetNetworkPlayerID());
                    lChangeCar.mCarModelId   = lpChangedCarEvent->GetModelID();
                    lChangeCar.mf18          = lpChangedCarEvent->mf10;
                    lChangeCar.mWheelModelId = lpChangedCarEvent->GetWheelID();
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lChangeCar, GsIO::E_EVENT_CHANGE_NETWORK_CAR);
                    break;
                }
                case NetIO::NetworkOutTeamSelectionEvent::KI_EVENT_TYPE:                    // 21
                {
                    const NetIO::NetworkOutTeamSelectionEvent* lpTeamSelectionEvent =
                        reinterpret_cast<const NetIO::NetworkOutTeamSelectionEvent*>(lpEvent);
                    CGS_ASSERT(lpTeamSelectionEvent != 0, "lpTeamSelectionEvent");
                    GsIO::EPlayerTeam laeTeams[8];
                    std::memcpy(laeTeams, lpTeamSelectionEvent->maePlayerTeam, sizeof(laeTeams));
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), laeTeams, 154);
                    break;
                }
                case 23:  // NetworkPlayerDisconnectedEvent -> RemotePlayerDisconnectedEvent (only when DISCONNECTED)
                {
                    const NetIO::NetworkPlayerDisconnectedEvent* lpPlayerDisconnectedEvent =
                        reinterpret_cast<const NetIO::NetworkPlayerDisconnectedEvent*>(lpEvent);
                    CGS_ASSERT(lpPlayerDisconnectedEvent != 0, "lpPlayerDisconnectedEvent");
                    if (lpPlayerDisconnectedEvent->GetDisconnectStatus() !=
                        NetIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_DISCONNECTED)
                        break;
                    GsIO::RemotePlayerDisconnectedEvent lDisconnected;
                    lDisconnected.SetNetworkPlayerID(lpPlayerDisconnectedEvent->GetNetworkPlayerID());
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lDisconnected,
                                  GsIO::E_EVENT_REMOTE_PLAYER_DISCONNECTED);
                    break;
                }
                case NetIO::NetworkOutPlayerLeftGame::KI_EVENT_TYPE:                        // 24
                {
                    // An empty 1-byte signal; the record is not read.
                    const u8 lu8Signal = 0;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lu8Signal, 124);
                    break;
                }
                case NetIO::NetworkOutLaunchedEvent::KI_EVENT_TYPE:                         // 27
                {
                    const NetIO::NetworkOutLaunchedEvent* lpLaunchedEvent =
                        reinterpret_cast<const NetIO::NetworkOutLaunchedEvent*>(lpEvent);
                    CGS_ASSERT(lpLaunchedEvent != 0, "lpLaunchedEvent");
                    unsigned char lOut[8];
                    lOut[5] = lpLaunchedEvent->mbSuccessfulLaunch;
                    *reinterpret_cast<s32*>(lOut) = lpLaunchedEvent->miVehicleClassLimit;
                    lOut[4] = lpLaunchedEvent->mbHostChoiceCarAndNotHost;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 126, 8);
                    break;
                }
                case NetIO::NetworkOutStuntMultiplierEvent::KI_EVENT_TYPE:                  // 34
                {
                    const NetIO::NetworkOutStuntMultiplierEvent* lpStuntMultiplierEvent =
                        reinterpret_cast<const NetIO::NetworkOutStuntMultiplierEvent*>(lpEvent);
                    CGS_ASSERT(lpStuntMultiplierEvent != 0, "lpStuntMultiplierEvent");
                    unsigned char lOut[16];
                    *reinterpret_cast<s32*>(lOut + 0xC) = lpStuntMultiplierEvent->mNetworkPlayerID;
                    *reinterpret_cast<s32*>(lOut + 0x8) = lpStuntMultiplierEvent->miStuntMultiplier;
                    *reinterpret_cast<s64*>(lOut)       = lpStuntMultiplierEvent->mi64MultiplierData;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 143, 16);
                    break;
                }
                case 35:  // NetworkOutRecvRoadRulesPBEvent -> OnlineRoadRulesPersonalBestRecvEvent
                {
                    // The record's home holds it as one opaque 72-byte block: the 56-byte score at
                    // +0x00, then the player id (+0x38), the challenge index (+0x3C) and the friend
                    // flag (+0x40).
                    const NetIO::NetworkOutRecvRoadRulesPBEvent* lpRoadRulesPersonalBest =
                        reinterpret_cast<const NetIO::NetworkOutRecvRoadRulesPBEvent*>(lpEvent);
                    CGS_ASSERT(lpRoadRulesPersonalBest != 0, "lpRoadRulesPersonalBest");
                    const unsigned char* lpRecord = lpRoadRulesPersonalBest->maOpaque;
                    GsIO::OnlineRoadRulesPersonalBestRecvEvent lPersonalBest;
                    std::memcpy(&lPersonalBest.mPersonalBestScore, lpRecord, sizeof(lPersonalBest.mPersonalBestScore));
                    lPersonalBest.mPersonalBestPlayerID       = *reinterpret_cast<const s32*>(lpRecord + 0x38);
                    lPersonalBest.mPersonalBestChallengeIndex = *reinterpret_cast<const s32*>(lpRecord + 0x3C);
                    lPersonalBest.mbWasPBByFriend             = lpRecord[0x40] != 0;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lPersonalBest,
                                  GsIO::E_EVENT_ONLINE_ROAD_RULES_PB_RECV);
                    break;
                }
                case NetIO::NetworkOutRecvRoadRulesUploadedEvent::KI_EVENT_TYPE:            // 36
                {
                    const NetIO::NetworkOutRecvRoadRulesUploadedEvent* lpRoadRulesUploadedEvent =
                        reinterpret_cast<const NetIO::NetworkOutRecvRoadRulesUploadedEvent*>(lpEvent);
                    CGS_ASSERT(lpRoadRulesUploadedEvent != 0, "lpRoadRulesUploadedEvent");
                    GsIO::OnlineRoadRulesUploadedEvent lUploaded;
                    lUploaded.mStartUploadIndex = lpRoadRulesUploadedEvent->mStartUploadIndex;
                    lUploaded.mEndUploadIndex   = lpRoadRulesUploadedEvent->mEndUploadIndex;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lUploaded,
                                  GsIO::E_EVENT_ONLINE_ROAD_RULES_UPLOADED);
                    break;
                }
                case NetIO::NetworkOutRoadRulesDownloadedEvent::KI_EVENT_TYPE:              // 37
                {
                    const NetIO::NetworkOutRoadRulesDownloadedEvent* lpRoadRulesDownloadedEvent =
                        reinterpret_cast<const NetIO::NetworkOutRoadRulesDownloadedEvent*>(lpEvent);
                    CGS_ASSERT(lpRoadRulesDownloadedEvent != 0, "lpRoadRulesDownloadedEvent");
                    GsIO::OnlineRoadRulesDownloadedEvent lDownloaded;
                    lDownloaded.muTimestampOfDownload = lpRoadRulesDownloadedEvent->muTimestampOfDownload;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lDownloaded,
                                  GsIO::E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED);
                    break;
                }
                case NetIO::NetworkOutRoadRulesConnectedOnlineEvent::KI_EVENT_TYPE:         // 38
                {
                    const NetIO::NetworkOutRoadRulesConnectedOnlineEvent* lpNetworkRRConnectEvent =
                        reinterpret_cast<const NetIO::NetworkOutRoadRulesConnectedOnlineEvent*>(lpEvent);
                    CGS_ASSERT(lpNetworkRRConnectEvent != 0, "lpNetworkRRConnectEvent");
                    GsIO::OnlineRoadRulesConnectInfoEvent lConnectInfo;
                    lConnectInfo.muLastRoadRulesResetTime = lpNetworkRRConnectEvent->muLastRoadRulesResetTime;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lConnectInfo,
                                  GsIO::E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO);
                    break;
                }
                case NetIO::NetworkOutLocalPlayerConnected::KI_EVENT_TYPE:                  // 39
                {
                    const NetIO::NetworkOutLocalPlayerConnected* lpPlayerConnectedEvent =
                        reinterpret_cast<const NetIO::NetworkOutLocalPlayerConnected*>(lpEvent);
                    CGS_ASSERT(lpPlayerConnectedEvent != 0, "lpPlayerConnectedEvent");
                    const s32 liPlayerID = lpPlayerConnectedEvent->mPlayerID;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), liPlayerID, 122);
                    break;
                }
                case NetIO::NetworkOutNetworkPlayerCollectableEvent::KI_EVENT_TYPE:         // 44
                {
                    const NetIO::NetworkOutNetworkPlayerCollectableEvent* lpCollectableEvent =
                        reinterpret_cast<const NetIO::NetworkOutNetworkPlayerCollectableEvent*>(lpEvent);
                    CGS_ASSERT(lpCollectableEvent != 0, "lpCollectableEvent");
                    unsigned char lOut[16];
                    *reinterpret_cast<CgsID*>(lOut)      = lpCollectableEvent->mID;
                    *reinterpret_cast<s32*>(lOut + 0xC)  = lpCollectableEvent->meType;
                    *reinterpret_cast<s32*>(lOut + 0x8)  = lpCollectableEvent->mNetworkPlayerID;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 135, 16);
                    break;
                }
                case NetIO::NetworkOutHostChangedEvent::KI_EVENT_TYPE:                      // 45
                {
                    const NetIO::NetworkOutHostChangedEvent* lpNetworkNewHostEvent =
                        reinterpret_cast<const NetIO::NetworkOutHostChangedEvent*>(lpEvent);
                    CGS_ASSERT(lpNetworkNewHostEvent != 0, "lpNetworkNewHostEvent");
                    bool labOut[2];
                    labOut[0] = lpNetworkNewHostEvent->mbIsLocalPlayerNowHost;
                    labOut[1] = lpNetworkNewHostEvent->mbIsFirstHost;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), labOut, 140);
                    break;
                }
                case NetIO::NetworkOutRemotePlayerHitCheckpoint::KI_EVENT_TYPE:             // 46
                {
                    const NetIO::NetworkOutRemotePlayerHitCheckpoint* lpNetworkCheckpointEvent =
                        reinterpret_cast<const NetIO::NetworkOutRemotePlayerHitCheckpoint*>(lpEvent);
                    CGS_ASSERT(lpNetworkCheckpointEvent != 0, "lpNetworkCheckpointEvent");
                    s32 laiOut[2];
                    laiOut[1] = lpNetworkCheckpointEvent->miCheckpointIndex;
                    laiOut[0] = lpNetworkCheckpointEvent->mNetworkPlayerID;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), laiOut, 141);
                    break;
                }
                case NetIO::NetworkOutStuntScoreUpdatedEvent::KI_EVENT_TYPE:                // 47
                {
                    const NetIO::NetworkOutStuntScoreUpdatedEvent* lpNetworkStuntScoreUpdatedEvent =
                        reinterpret_cast<const NetIO::NetworkOutStuntScoreUpdatedEvent*>(lpEvent);
                    CGS_ASSERT(lpNetworkStuntScoreUpdatedEvent != 0, "lpNetworkStuntScoreUpdatedEvent");
                    s32 laiOut[2];
                    laiOut[1] = lpNetworkStuntScoreUpdatedEvent->miStuntScore;
                    laiOut[0] = lpNetworkStuntScoreUpdatedEvent->mNetworkPlayerID;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), laiOut, 142);
                    break;
                }
                case NetIO::NetworkOutRivalCount::KI_EVENT_TYPE:                            // 49
                {
                    const NetIO::NetworkOutRivalCount* lpNetworkRivalCountEvent =
                        reinterpret_cast<const NetIO::NetworkOutRivalCount*>(lpEvent);
                    CGS_ASSERT(lpNetworkRivalCountEvent != 0, "lpNetworkRivalCountEvent");
                    const s32 liNumberOfRivals = lpNetworkRivalCountEvent->miNumberOfRivals;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), liNumberOfRivals, 144);
                    break;
                }
                case NetIO::NetworkOutCaughtFever::KI_EVENT_TYPE:                           // 50
                {
                    const u8 lu8Signal = 0;   // empty 1-byte signal
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lu8Signal, 146);
                    break;
                }
                case NetIO::NetworkOutTargetScoreEvent::KI_EVENT_TYPE:                      // 53
                {
                    const NetIO::NetworkOutTargetScoreEvent* lpTargetScoreEvent =
                        reinterpret_cast<const NetIO::NetworkOutTargetScoreEvent*>(lpEvent);
                    CGS_ASSERT(lpTargetScoreEvent != 0, "lpTargetScoreEvent");
                    unsigned char lOut[40];
                    std::memcpy(lOut, &lpTargetScoreEvent->mPlayerID, sizeof(lpTargetScoreEvent->mPlayerID)); // +0x00 (24)
                    *reinterpret_cast<u64*>(lOut + 0x18) = lpTargetScoreEvent->mScoreboardSlot;
                    *reinterpret_cast<s32*>(lOut + 0x20) = lpTargetScoreEvent->miValue;
                    lOut[0x24] = lpTargetScoreEvent->muChallengeType;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 152, 40);
                    break;
                }
                case NetIO::NetworkOutDldChallengeableEvent::KI_EVENT_TYPE:                 // 54
                {
                    const NetIO::NetworkOutDldChallengeableEvent* lpDldChallengeableEvent =
                        reinterpret_cast<const NetIO::NetworkOutDldChallengeableEvent*>(lpEvent);
                    CGS_ASSERT(lpDldChallengeableEvent != 0, "lpDldChallengeableEvent");
                    const CgsID lID = lpDldChallengeableEvent->mID;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lID, 153);
                    break;
                }
                case NetIO::NetworkOutGetOfflineProgression::KI_EVENT_TYPE:                 // 55
                {
                    const u8 lu8Signal = 0;   // empty 1-byte signal
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lu8Signal, 79);
                    break;
                }
                case NetIO::NetworkOutImageReceivedEvent::KI_EVENT_TYPE:                    // 57
                {
                    const NetIO::NetworkOutImageReceivedEvent* lpImageReceivedEvent =
                        reinterpret_cast<const NetIO::NetworkOutImageReceivedEvent*>(lpEvent);
                    CGS_ASSERT(lpImageReceivedEvent != 0, "lpImageReceivedEvent");
                    unsigned char lOut[16];
                    *reinterpret_cast<s32*>(lOut)         = lpImageReceivedEvent->meImageSenderRaceCarIndex;
                    *reinterpret_cast<s32*>(lOut + 0x4)   = lpImageReceivedEvent->meReceivedImageType;
                    *reinterpret_cast<CgsID*>(lOut + 0x8) = lpImageReceivedEvent->mRoadID;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 136, 16);
                    break;
                }
                case NetIO::NetworkOutMugshotToSaveEvent::KI_EVENT_TYPE:                    // 58
                {
                    // The console copies the 32-byte record straight across; the game-state record
                    // (ImageToSaveEvent) holds the same four fields, with the texture pointer
                    // widened on the host, so it is copied by member.
                    const NetIO::NetworkOutMugshotToSaveEvent* lpMugshotToSaveEvent =
                        reinterpret_cast<const NetIO::NetworkOutMugshotToSaveEvent*>(lpEvent);
                    CGS_ASSERT(lpMugshotToSaveEvent != 0, "lpMugshotToSaveEvent");
                    BrnGameState::ImageToSaveEvent lImageToSave;
                    static_assert(offsetof(BrnGameState::ImageToSaveEvent, meImageType) ==
                                  sizeof(lpMugshotToSaveEvent->mUniquePlayerID),
                                  "the unique player id fills the record up to the image type");
                    std::memcpy(&lImageToSave, &lpMugshotToSaveEvent->mUniquePlayerID,
                                sizeof(lpMugshotToSaveEvent->mUniquePlayerID));
                    lImageToSave.meImageType = lpMugshotToSaveEvent->meImageGalleryType;
                    lImageToSave.mpTexture   = lpMugshotToSaveEvent->mpTexture;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lImageToSave, 156);
                    break;
                }
                case NetIO::NetworkOutAbortImageCaptureEvent::KI_EVENT_TYPE:                // 59
                {
                    const NetIO::NetworkOutAbortImageCaptureEvent* lpAbortCaptureEvent =
                        reinterpret_cast<const NetIO::NetworkOutAbortImageCaptureEvent*>(lpEvent);
                    CGS_ASSERT(lpAbortCaptureEvent != 0, "lpAbortCaptureEvent");
                    const bool lbCapture = lpAbortCaptureEvent->mbCapture;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lbCapture, 138);
                    break;
                }
                case NetIO::NetworkOutSentMugshot::KI_EVENT_TYPE:                           // 60
                {
                    const u8 lu8Signal = 0;   // empty 1-byte signal
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lu8Signal, 147);
                    break;
                }
                case NetIO::NetworkOutSwitchBurningHomeRunRunner::KI_EVENT_TYPE:            // 61
                {
                    const NetIO::NetworkOutSwitchBurningHomeRunRunner* lpNetworkSwitchRunnerEvent =
                        reinterpret_cast<const NetIO::NetworkOutSwitchBurningHomeRunRunner*>(lpEvent);
                    CGS_ASSERT(lpNetworkSwitchRunnerEvent != 0, "lpNetworkSwitchRunnerEvent");
                    const s32 liNewRunnerID = lpNetworkSwitchRunnerEvent->mNewRunnerID;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), liNewRunnerID, 155);
                    break;
                }
                case NetIO::NetworkOutBurnoutSkillzChanged::KI_EVENT_TYPE:                  // 62
                {
                    const NetIO::NetworkOutBurnoutSkillzChanged* lpBurnoutSkillzNetworkEvent =
                        reinterpret_cast<const NetIO::NetworkOutBurnoutSkillzChanged*>(lpEvent);
                    CGS_ASSERT(lpBurnoutSkillzNetworkEvent != 0, "lpBurnoutSkillzNetworkEvent");
                    unsigned char lOut[64];
                    *reinterpret_cast<s32*>(lOut) = lpBurnoutSkillzNetworkEvent->mPlayerID;
                    std::memcpy(lOut + 0x4, &lpBurnoutSkillzNetworkEvent->mNewSkillzData,
                                sizeof(lpBurnoutSkillzNetworkEvent->mNewSkillzData));
                    lOut[0x3C] = lpBurnoutSkillzNetworkEvent->mbInitialData;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 139, 64);
                    break;
                }
                case NetIO::NetworkOutShowtimeUpdate::KI_EVENT_TYPE:                        // 63
                {
                    const NetIO::NetworkOutShowtimeUpdate* lpShowtimeUpdateNetworkEvent =
                        reinterpret_cast<const NetIO::NetworkOutShowtimeUpdate*>(lpEvent);
                    CGS_ASSERT(lpShowtimeUpdateNetworkEvent != 0, "lpShowtimeUpdateNetworkEvent");
                    s32 laiOut[2];
                    laiOut[0] = lpShowtimeUpdateNetworkEvent->mPlayerID;
                    laiOut[1] = lpShowtimeUpdateNetworkEvent->miShowtimeScore;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), laiOut, 49);
                    break;
                }
                case NetIO::NetworkOutShowtimeSwitch::KI_EVENT_TYPE:                        // 64
                {
                    const NetIO::NetworkOutShowtimeSwitch* lpShowtimeModeSwitchNetworkEvent =
                        reinterpret_cast<const NetIO::NetworkOutShowtimeSwitch*>(lpEvent);
                    CGS_ASSERT(lpShowtimeModeSwitchNetworkEvent != 0, "lpShowtimeModeSwitchNetworkEvent");
                    unsigned char lOut[12];
                    *reinterpret_cast<s32*>(lOut)       = lpShowtimeModeSwitchNetworkEvent->mPlayerID;
                    *reinterpret_cast<s32*>(lOut + 0x4) = lpShowtimeModeSwitchNetworkEvent->miFinalShowtimeScore;
                    lOut[0x8] = lpShowtimeModeSwitchNetworkEvent->mbEnteringShowtime;
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 50, 12);
                    break;
                }
                case NetIO::NetworkOutFreeburnChallenge::KI_EVENT_TYPE:                     // 65
                {
                    // One game-state event per challenge-event type: SELECTED (0) -> 163 and
                    // TRIGGERED (1) -> 164 carry only the challenge id; RESET (3) -> 166,
                    // 4 -> 167 and ACTION_SUCCESS (2) -> 165 add the action index; anything else
                    // -> 168 with the challenge status.
                    const NetIO::NetworkOutFreeburnChallenge* lpFreeburnChallengeNetworkEvent =
                        reinterpret_cast<const NetIO::NetworkOutFreeburnChallenge*>(lpEvent);
                    CGS_ASSERT(lpFreeburnChallengeNetworkEvent != 0, "lpFreeburnChallengeNetworkEvent");
                    const s32 leEventType = lpFreeburnChallengeNetworkEvent->meEventType;
                    if (leEventType == 0)
                    {
                        const CgsID lChallengeID = lpFreeburnChallengeNetworkEvent->mChallengeID;
                        PostGameEvent(lpGameStateInput->GetGameEventQueue(), lChallengeID, 163);
                    }
                    else if (leEventType == 1)
                    {
                        const CgsID lChallengeID = lpFreeburnChallengeNetworkEvent->mChallengeID;
                        PostGameEvent(lpGameStateInput->GetGameEventQueue(), lChallengeID, 164);
                    }
                    else if (leEventType == 3)
                    {
                        GsIO::FreeburnChallengeResetEvent lReset;
                        lReset.mChallengeID  = lpFreeburnChallengeNetworkEvent->mChallengeID;
                        lReset.miActionIndex = lpFreeburnChallengeNetworkEvent->miActionIndex;
                        PostGameEvent(lpGameStateInput->GetGameEventQueue(), lReset, GsIO::E_EVENT_FREEBURN_CHALLENGE_RESET);
                    }
                    else if (leEventType == 4)
                    {
                        GsIO::FreeburnChallengeResetEvent lResetAll;
                        lResetAll.mChallengeID  = lpFreeburnChallengeNetworkEvent->mChallengeID;
                        lResetAll.miActionIndex = lpFreeburnChallengeNetworkEvent->miActionIndex;
                        PostGameEvent(lpGameStateInput->GetGameEventQueue(), lResetAll,
                                      GsIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS);
                    }
                    else if (leEventType == 2)
                    {
                        GsIO::FreeburnChallengeActionSuccessEvent lActionSuccess;
                        lActionSuccess.mChallengeID  = lpFreeburnChallengeNetworkEvent->mChallengeID;
                        lActionSuccess.miActionIndex = lpFreeburnChallengeNetworkEvent->miActionIndex;
                        PostGameEvent(lpGameStateInput->GetGameEventQueue(), lActionSuccess,
                                      GsIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS);
                    }
                    else
                    {
                        unsigned char lOut[16];
                        *reinterpret_cast<CgsID*>(lOut)     = lpFreeburnChallengeNetworkEvent->mChallengeID;
                        *reinterpret_cast<s32*>(lOut + 0x8) = lpFreeburnChallengeNetworkEvent->meChallengeStatus;
                        lpGameStateInput->GetGameEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(lOut), 168, 16);
                    }
                    break;
                }
                case NetIO::NetworkOutActiveFburnChallengeEvent::KI_EVENT_TYPE:             // 66
                {
                    const NetIO::NetworkOutActiveFburnChallengeEvent* lpActiveFreeburnChallengeEvent =
                        reinterpret_cast<const NetIO::NetworkOutActiveFburnChallengeEvent*>(lpEvent);
                    CGS_ASSERT(lpActiveFreeburnChallengeEvent != 0, "lpActiveFreeburnChallengeEvent");
                    GsIO::ActiveFburnChallengeEvent lActiveChallenge;
                    lActiveChallenge.mChallengeID            = lpActiveFreeburnChallengeEvent->mChallengeID;
                    lActiveChallenge.miNumPlayersInChallenge = lpActiveFreeburnChallengeEvent->miNumPlayersInChallenge;
                    std::memcpy(lActiveChallenge.maePlayersInChallengeARCI,
                                lpActiveFreeburnChallengeEvent->maePlayersInChallengeARCI,
                                sizeof(lActiveChallenge.maePlayersInChallengeARCI));
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lActiveChallenge,
                                  GsIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE);
                    break;
                }
                case NetIO::NetworkOutFburnChallengeSuccessUpdateEvent::KI_EVENT_TYPE:      // 67
                {
                    const NetIO::NetworkOutFburnChallengeSuccessUpdateEvent* lpFburnChallengeSuccessUpdateEvent =
                        reinterpret_cast<const NetIO::NetworkOutFburnChallengeSuccessUpdateEvent*>(lpEvent);
                    CGS_ASSERT(lpFburnChallengeSuccessUpdateEvent != 0, "lpFburnChallengeSuccessUpdateEvent");
                    GsIO::FburnChallengeSuccessUpdateEvent lSuccessUpdate;
                    lSuccessUpdate.mChallengeSuccessUpdate = lpFburnChallengeSuccessUpdateEvent->mChallengeSuccessUpdate;
                    lSuccessUpdate.meActiveRaceCarIndex    = lpFburnChallengeSuccessUpdateEvent->meActiveRaceCarIndex;
                    lSuccessUpdate.miChallengeUpdateFrame  = lpFburnChallengeSuccessUpdateEvent->miChallengeUpdateFrame;
                    lSuccessUpdate.miActionIndex           = lpFburnChallengeSuccessUpdateEvent->miActionIndex;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lSuccessUpdate,
                                  GsIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE);
                    break;
                }
                case NetIO::NetworkOutFburnChallengeSuccessEvent::KI_EVENT_TYPE:            // 68
                {
                    const NetIO::NetworkOutFburnChallengeSuccessEvent* lpFburnChallengeSuccessEvent =
                        reinterpret_cast<const NetIO::NetworkOutFburnChallengeSuccessEvent*>(lpEvent);
                    CGS_ASSERT(lpFburnChallengeSuccessEvent != 0, "lpFburnChallengeSuccessEvent");
                    GsIO::FburnChallengeSuccessEvent lSuccess;
                    lSuccess.miChallengeUpdateFrame = lpFburnChallengeSuccessEvent->miChallengeUpdateFrame;
                    std::memcpy(lSuccess.mafActionScores, lpFburnChallengeSuccessEvent->mafActionScores,
                                sizeof(lSuccess.mafActionScores));
                    std::memcpy(lSuccess.mabSuccessfulActions, lpFburnChallengeSuccessEvent->mabSuccessfulActions,
                                sizeof(lSuccess.mabSuccessfulActions));
                    std::memcpy(lSuccess.mabAccumulationThisFrame, lpFburnChallengeSuccessEvent->mabAccumulationThisFrame,
                                sizeof(lSuccess.mabAccumulationThisFrame));
                    lSuccess.meActiveRaceCarIndex = lpFburnChallengeSuccessEvent->meActiveRaceCarIndex;
                    PostGameEvent(lpGameStateInput->GetGameEventQueue(), lSuccess,
                                  GsIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS);
                    break;
                }
                case NetIO::NetworkOutUploadedModeScoresEvent::KI_EVENT_TYPE:               // 74
                {
                    const NetIO::NetworkOutUploadedModeScoresEvent* lpUploadedModeScoresEvent =
                        reinterpret_cast<const NetIO::NetworkOutUploadedModeScoresEvent*>(lpEvent);
                    CGS_ASSERT(lpUploadedModeScoresEvent != 0, "lpUploadedModeScoresEvent");
                    unsigned char lOut[128];
                    std::memcpy(lOut, &lpUploadedModeScoresEvent->maUploadedModeScores, sizeof(lOut));
                    lpGameStateInput->GetGameEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lOut), 175, 128);
                    break;
                }
                default:
                    break;
            }

            liEventType = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // -------------------------------------------------------------------------
    // Scoreboard-response string copy guard (X360 CgsStringUtils inline): scan the source
    // name to its NUL, assert length < 31 ("String too long: " verbatim, HARD RULE A), then
    // strncpy 31 bytes into the destination name slot.
    // -------------------------------------------------------------------------
    static void CopyScoreboardName(unsigned char* lpDst, const char* lpcName)
    {
        const char* lpcScan = lpcName;
        while (*lpcScan++)
        {
        }
        CGS_ASSERT((lpcScan - lpcName - 1) < 31, "String too long: ");
        std::strncpy(reinterpret_cast<char*>(lpDst), lpcName, 31);
    }

    // =========================================================================
    // TranslateScoreboardResponse -- the heading-list arm (inlined into
    // TranslateNetworkEventsToGuiEvents). The record's heading type selects which scoreboard GUI
    // event its count and 31-byte names are copied into.
    // =========================================================================
    void BrnGameModule::TranslateScoreboardResponse(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnNetwork::BrnNetworkModuleIO::NetworkOutScoreboardHeadingList* lpHeadingList)
    {
        const u32 luHeadingType = static_cast<u32>(lpHeadingList->GetHeadingType());
        const s32 liCount       = lpHeadingList->miLength;

        if (luHeadingType == BrnNetwork::BrnNetworkModuleIO::E_HEADING_CATEGORY)
        {
            // category (cap KI_MAX_CATEGORIES == 15)
            BrnGui::GuiEventScoreboardResponseCategoryEvent lEvent;
            unsigned char* lpDst = RecordBytes(lEvent);
            *reinterpret_cast<s32*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 15,
                "lCategoryEvent.miNumberOfCategories <= BrnGui::GuiEventScoreboardResponseCategoryEvent::KI_MAX_CATEGORIES");
            for (s32 i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpHeadingList->maHeadings[i]);
            PushGuiEvent(lEvent, lpGuiInput);
        }
        else if (luHeadingType == BrnNetwork::BrnNetworkModuleIO::E_HEADING_INDEX)
        {
            // index (cap KI_MAX_INDEXES == 10)
            BrnGui::GuiEventScoreboardResponseIndexEvent lEvent;
            unsigned char* lpDst = RecordBytes(lEvent);
            *reinterpret_cast<s32*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 10,
                "lIndexEvent.miNumberOfIndexes <= BrnGui::GuiEventScoreboardResponseIndexEvent::KI_MAX_INDEXES");
            for (s32 i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpHeadingList->maHeadings[i]);
            PushGuiEvent(lEvent, lpGuiInput);
        }
        else if (luHeadingType < BrnNetwork::BrnNetworkModuleIO::E_HEADING_COUNT)
        {
            // variation (cap KI_MAX_VARIATIONS == 66). The two bytes after the name table
            // (+0x806 / +0x807: the reference's mbIsPerRoad and the byte after it; the record's
            // home holds them in its tail pad) land right after the event's last name slot,
            // +2050 / +2051.
            BrnGui::GuiEventScoreboardResponseVariationEvent lEvent;
            unsigned char* lpDst = RecordBytes(lEvent);
            *reinterpret_cast<s32*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 66,
                "lVariationEvent.miNumberOfVariations <= BrnGui::GuiEventScoreboardResponseVariationEvent::KI_MAX_VARIATIONS");
            for (s32 i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpHeadingList->maHeadings[i]);
            lpDst[2050] = lpHeadingList->maReservedPadTo0x808[0];
            lpDst[2051] = lpHeadingList->maReservedPadTo0x808[1];
            PushGuiEvent(lEvent, lpGuiInput);
        }
        else
        {
            // Invalid heading type (the console builds "Invalid heading type : <n>\n"; collapsed
            // to the rodata prefix per HARD RULE A).
            CGS_ASSERT(false, "Invalid heading type : ");
        }
    }

    // =========================================================================
    // TranslateNetworkEventsToGuiEvents  (X360 0x823E0900)
    // Drain the network event queue (VariableEventQueue<14000,16>) and translate each GUI-bound
    // network event into the matching CgsGui::GuiModule event (this + 7252512).
    // =========================================================================
    void BrnGameModule::TranslateNetworkEventsToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput, const OutputBuffer* lpNetworkOutput)
    {
        namespace NetIO = BrnNetwork::BrnNetworkModuleIO;

        const NetworkEventQueue* lpNetworkEventQueue = lpNetworkOutput->GetNetworkEventQueue();
        CGS_ASSERT(lpNetworkEventQueue != 0, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventType = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            switch (liEventType)
            {
                case NetIO::NetworkOutBuddyCount::KI_EVENT_TYPE:                            // 1
                {
                    const NetIO::NetworkOutBuddyCount* lpBuddyCount =
                        reinterpret_cast<const NetIO::NetworkOutBuddyCount*>(lpEvent);
                    BrnGui::GuiEventOnlineNumFriendsCount lEvent;
                    *reinterpret_cast<s32*>(&lEvent) = lpBuddyCount->miBuddyCount;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutBuddyInformation::KI_EVENT_TYPE:                      // 2
                {
                    const NetIO::NetworkOutBuddyInformation* lpBuddyInformation =
                        reinterpret_cast<const NetIO::NetworkOutBuddyInformation*>(lpEvent);
                    BrnGui::GuiEventOnlineReceiveFriendInfo lEvent;
                    lEvent.miNumberOfBuddiesInEvent           = lpBuddyInformation->miNumberOfBuddiesInEvent;
                    lEvent.miIndexOfFirstBuddyInFullBuddyList = lpBuddyInformation->miIndexOfFirstBuddyInFullBuddyList;
                    lEvent.mbConnected                        = lpBuddyInformation->mbConnected;
                    std::memcpy(lEvent.mBuddyInformation, lpBuddyInformation->mBuddyInformation,
                                sizeof(lEvent.mBuddyInformation));
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutInviteFailed::KI_EVENT_TYPE:                          // 8
                {
                    const NetIO::NetworkOutInviteFailed* lpInviteFailed =
                        reinterpret_cast<const NetIO::NetworkOutInviteFailed*>(lpEvent);
                    BrnGui::GuiEventInviteFailed lEvent;
                    *reinterpret_cast<s32*>(&lEvent) = lpInviteFailed->meFailReason;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutBuddyNotification::KI_EVENT_TYPE:                     // 9
                {
                    const NetIO::NetworkOutBuddyNotification* lpBuddyNotification =
                        reinterpret_cast<const NetIO::NetworkOutBuddyNotification*>(lpEvent);
                    BrnGui::GuiEventBuddyNotification lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    std::memcpy(lpDst + 0x8, &lpBuddyNotification->mBuddyName, sizeof(lpBuddyNotification->mBuddyName));
                    *reinterpret_cast<s32*>(lpDst)       = lpBuddyNotification->meBuddyNotification;
                    *reinterpret_cast<s32*>(lpDst + 0x4) = lpBuddyNotification->miNumBuddies;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutBuddyListChanged::KI_EVENT_TYPE:                      // 10
                {
                    CgsGui::GuiEvent<103> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutInviteSent::KI_EVENT_TYPE:                            // 11
                {
                    CgsGui::GuiEvent<95> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutInstantFreeburnComplete::KI_EVENT_TYPE:               // 13
                {
                    const NetIO::NetworkOutInstantFreeburnComplete* lpFreeburnComplete =
                        reinterpret_cast<const NetIO::NetworkOutInstantFreeburnComplete*>(lpEvent);
                    CGS_ASSERT(lpFreeburnComplete != 0, "lpFreeburnComplete");
                    if (!lpFreeburnComplete->mbSuccess)
                    {
                        CgsGui::GuiEvent<285> lEventA;
                        PushGuiEvent(lEventA, lpGuiInput);
                    }
                    CgsGui::GuiEvent<95> lEventB;
                    PushGuiEvent(lEventB, lpGuiInput);
                    break;
                }
                case 15:  // NetworkOutGameParamsChanged -> GuiEventNetworkGameParams
                {
                    // The ten round events copy straight across; the scalar tail is re-ordered by
                    // name. The record's home spells the tail by offset (miMaxPlayers, miGameMode,
                    // mi1C0 ...); the reference name of each is given at the read.
                    const NetIO::NetworkOutGameParamsChanged* lpParamsChangedEvent =
                        reinterpret_cast<const NetIO::NetworkOutGameParamsChanged*>(lpEvent);
                    CGS_ASSERT(lpParamsChangedEvent != 0, "lpParamsChangedEvent");
                    BrnGui::GuiEventNetworkGameParams lEvent;
                    lEvent.mbInfiniteBoost     = lpParamsChangedEvent->mbFlag1D;        // mbInfiniteBoost
                    lEvent.miVehicleClass      = lpParamsChangedEvent->mi1D0;           // miVehicleClass
                    lEvent.mbRanked            = lpParamsChangedEvent->mbIsRankedGame;  // mbRanked
                    lEvent.meSecurity          = lpParamsChangedEvent->mi1C0;           // meSecurity
                    lEvent.meGameMode          = lpParamsChangedEvent->miMaxPlayers;    // meGameMode
                    lEvent.mePreviousGameMode  = lpParamsChangedEvent->miGameMode;      // mePreviousGameMode
                    lEvent.miNumRounds         = lpParamsChangedEvent->mi1CC;           // miNumRounds
                    lEvent.mbTrafficOn         = lpParamsChangedEvent->mbFlag1E;        // mbTrafficOn
                    lEvent.meBoostType         = lpParamsChangedEvent->mi1C4;           // meBoostType
                    lEvent.miNumRunnerCrashes  = lpParamsChangedEvent->mi1D4;           // miNumRunnerCrashes
                    lEvent.mbTrafficCheckingOn = lpParamsChangedEvent->mbFlag1F;        // mbTrafficCheckingOn
                    lEvent.meVehicleChoice     = lpParamsChangedEvent->mi1C8;           // meVehicleChoice
                    lEvent.miTimeLimit         = lpParamsChangedEvent->mi1D8;           // miTimeLimit
                    static_assert(sizeof(lEvent.maEvents) == 440, "the ten round events span 440 bytes");
                    std::memcpy(lEvent.maEvents, lpParamsChangedEvent, sizeof(lEvent.maEvents));
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 17:  // NetworkOutPlayerRemovedEvent -> GuiEventNetworkPlayerLeftLobby (only when not leaving)
                {
                    // The record's opaque tail holds the player name (+0x04),
                    // mbIsLocalPlayerInGame (+0x14) and mbLocalPlayerLeavingGame (+0x15).
                    const NetIO::NetworkOutPlayerRemovedEvent* lpPlayerRemovedEvent =
                        reinterpret_cast<const NetIO::NetworkOutPlayerRemovedEvent*>(lpEvent);
                    CGS_ASSERT(lpPlayerRemovedEvent != 0, "lpPlayerRemovedEvent");
                    const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpPlayerRemovedEvent);
                    if (lpRecord[0x15] == 0)
                    {
                        BrnGui::GuiEventNetworkPlayerLeftLobby lEvent;
                        lEvent.mNetworkPlayerID = lpPlayerRemovedEvent->mNetworkPlayerID;
                        std::memcpy(&lEvent.mPlayerName, lpRecord + 0x04, sizeof(lEvent.mPlayerName));
                        lEvent.mbIsLocalPlayerInGame = lpRecord[0x14] != 0;
                        PushGuiEvent(lEvent, lpGuiInput);
                    }
                    break;
                }
                case NetIO::NetworkOutPostEventScalps::KI_EVENT_TYPE:                       // 22
                {
                    const NetIO::NetworkOutPostEventScalps* lpPostEventScalps =
                        reinterpret_cast<const NetIO::NetworkOutPostEventScalps*>(lpEvent);
                    BrnGui::GuiEventOnlinePostEventScalps lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    *reinterpret_cast<s32*>(lpDst + 0x40) = lpPostEventScalps->miNumScalpsWon;
                    std::memcpy(lpDst, lpPostEventScalps->maOnlineScalps, sizeof(lpPostEventScalps->maOnlineScalps));
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutPlayerLeftGame::KI_EVENT_TYPE:                        // 24
                {
                    const NetIO::NetworkOutPlayerLeftGame* lpPlayerLeftGame =
                        reinterpret_cast<const NetIO::NetworkOutPlayerLeftGame*>(lpEvent);
                    CGS_ASSERT(lpPlayerLeftGame != 0, "lpEvent");
                    BrnGui::GuiEventNetworkLeftGame lEvent;
                    reinterpret_cast<s32*>(&lEvent)[0] = lpPlayerLeftGame->meLeftGameReason;
                    reinterpret_cast<s32*>(&lEvent)[1] = lpPlayerLeftGame->meKickReason;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutPostGameProcessingFinished::KI_EVENT_TYPE:            // 25
                {
                    const NetIO::NetworkOutPostGameProcessingFinished* lpPostGameProcessingFinished =
                        reinterpret_cast<const NetIO::NetworkOutPostGameProcessingFinished*>(lpEvent);
                    CGS_ASSERT(lpPostGameProcessingFinished != 0, "lpPostGameProcessingFinished");
                    BrnGui::GuiEventNetworkPostGameProcessingFinished lEvent;
                    RecordBytes(lEvent)[0] = lpPostGameProcessingFinished->mbIsStillInGame;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutLaunchEvent::KI_EVENT_TYPE:                           // 26
                {
                    const NetIO::NetworkOutLaunchEvent* lpLaunchEvent =
                        reinterpret_cast<const NetIO::NetworkOutLaunchEvent*>(lpEvent);
                    CGS_ASSERT(lpLaunchEvent != 0, "lpLaunchEvent");
                    CgsGui::GuiEventNetworkLaunching lEvent;
                    reinterpret_cast<s32*>(&lEvent)[0] = lpLaunchEvent->meGameModeType;
                    reinterpret_cast<s32*>(&lEvent)[1] = lpLaunchEvent->mHostPlayerID;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutLaunchedEvent::KI_EVENT_TYPE:                         // 27
                {
                    // Launch overlay wait-finish requests (two request names); the record is not
                    // read. GuiOverlayWaitFinishRequest::Construct is one CgsIDCompress stored as
                    // the record's single 8-byte word.
                    BrnGui::GuiOverlayWaitFinishRequest lRequest;
                    lRequest.Construct("CNOnlLchGame");
                    PushGuiEvent(lRequest, lpGuiInput);
                    lRequest.Construct("CNOnlLchGmH");
                    PushGuiEvent(lRequest, lpGuiInput);
                    break;
                }
                case 28:  // GuiEvent<271>
                {
                    CgsGui::GuiEvent<271> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutLiveRevengeProfileData::KI_EVENT_TYPE:                // 29
                {
                    // FLAG: the GUI record is a 4-byte opaque slot on the console (one pointer); the
                    // host pointer is 8 bytes, so only its first 4 bytes reach the GUI until the
                    // GUI record gets a pointer member.
                    const NetIO::NetworkOutLiveRevengeProfileData* lpProfileData =
                        reinterpret_cast<const NetIO::NetworkOutLiveRevengeProfileData*>(lpEvent);
                    BrnGui::GuiEventLiveRevengeProfileData lEvent;
                    std::memcpy(&lEvent, &lpProfileData->mpLiveRevengeProfile, sizeof(lEvent));
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 30:  // GuiEvent<280>
                {
                    CgsGui::GuiEvent<280> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutEventTimeRemaining::KI_EVENT_TYPE:                    // 31
                {
                    // The console adds the fraction and the whole seconds as floats: the time's
                    // float value.
                    const NetIO::NetworkOutEventTimeRemaining* lpTimeRemaining =
                        reinterpret_cast<const NetIO::NetworkOutEventTimeRemaining*>(lpEvent);
                    BrnGui::GuiEventOnlineTimeout lEvent;
                    *reinterpret_cast<f32*>(&lEvent) = lpTimeRemaining->mTimeRemaining.GetFloatVal();
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutAutosaveProfile::KI_EVENT_TYPE:                       // 32
                {
                    BrnGui::GuiAutosaveRequestEvent lEvent;
                    RecordBytes(lEvent)[0] = 0;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 42:  // GuiEvent<109>
                {
                    CgsGui::GuiEvent<109> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutPlayerCarSelectStatus::KI_EVENT_TYPE:                 // 48
                {
                    const NetIO::NetworkOutPlayerCarSelectStatus* lpPlayerCarSelectStatus =
                        reinterpret_cast<const NetIO::NetworkOutPlayerCarSelectStatus*>(lpEvent);
                    CGS_ASSERT(lpPlayerCarSelectStatus != 0, "lpPlayerCarSelectStatus");
                    BrnGui::GuiOnlineCarStatusEvent lEvent;
                    *reinterpret_cast<s32*>(&lEvent) = lpPlayerCarSelectStatus->meActiveRaceCarIndex;
                    RecordBytes(lEvent)[4] = lpPlayerCarSelectStatus->mbInCarSelect;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutScoreboardEvent::KI_EVENT_TYPE:                       // 51
                {
                    const NetIO::NetworkOutScoreboardEvent* lpScoreboardEvent =
                        reinterpret_cast<const NetIO::NetworkOutScoreboardEvent*>(lpEvent);
                    BrnGui::GuiEventScoreboardResponseTableEvent lEvent;
                    std::memcpy(RecordBytes(lEvent), lpScoreboardEvent->GetScoreboard(), sizeof(lEvent));
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 52:  // NetworkOutScoreboardHeadingList (category / index / variation heading sub-switch)
                    TranslateScoreboardResponse(
                        lpGuiInput, reinterpret_cast<const NetIO::NetworkOutScoreboardHeadingList*>(lpEvent));
                    break;
                case NetIO::NetworkOutCapturingTheirImageEvent::KI_EVENT_TYPE:              // 56
                {
                    // CapturingImage -> MugshotControlEvent (+ filter-gated debug spew)
                    const NetIO::NetworkOutCapturingTheirImageEvent* lpCapturingImageEvent =
                        reinterpret_cast<const NetIO::NetworkOutCapturingTheirImageEvent*>(lpEvent);
                    CGS_ASSERT(lpCapturingImageEvent != 0, "lpCapturingImageEvent");
                    const s32   leImageType  = lpCapturingImageEvent->meImageType;
                    const CgsID lRoadId      = lpCapturingImageEvent->mRoadId;
                    const s32   leVictimARCI = lpCapturingImageEvent->meVictimARCI;
                    BrnGui::GuiMugshotControlEvent lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    *reinterpret_cast<s32*>(lpDst + 8)    = 1;
                    *reinterpret_cast<s32*>(lpDst + 12)   = leImageType;
                    *reinterpret_cast<CgsID*>(lpDst)      = lRoadId;
                    *reinterpret_cast<s32*>(lpDst + 16)   = leVictimARCI;
                    lpDst[20] = 1;
                    // FLAG: sub_821F0E50/sub_8229F668/sub_82203EE8 are un-homed StrStreamBase
                    // insertion overloads; modelled by their operand-typed committed overload.
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE NETWORK TO X (1)\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "IMAGE TYPE : " << leImageType << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "AGG ARCI   : " << leVictimARCI << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "ROAD?      : " << static_cast<u64>(lRoadId) << "\n";
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutAbortImageCaptureEvent::KI_EVENT_TYPE:                // 59
                {
                    // AbortCapture -> MugshotControlEvent (+ filter-gated debug spew)
                    const NetIO::NetworkOutAbortImageCaptureEvent* lpAbortEvent =
                        reinterpret_cast<const NetIO::NetworkOutAbortImageCaptureEvent*>(lpEvent);
                    CGS_ASSERT(lpAbortEvent != 0, "lpEvent");
                    BrnGui::GuiMugshotControlEvent lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    const s32 liResponse = lpAbortEvent->mbCapture ? 4 : 5;
                    *reinterpret_cast<u64*>(lpDst) = 0;                           // qword @ +0
                    *reinterpret_cast<s32*>(lpDst + 8) = liResponse;
                    *reinterpret_cast<s32*>(lpDst + 12) = 1;
                    *reinterpret_cast<s32*>(lpDst + 16) = -1;
                    lpDst[20] = 0;
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE NETWORK TO X (2)\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "IMAGE TYPE : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << static_cast<s32>(0) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "AGG ARCI   : " << static_cast<s32>(-1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << liResponse << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "ROAD?      : " << static_cast<u64>(0) << "\n";
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutActiveFburnChallengeEvent::KI_EVENT_TYPE:             // 66
                {
                    // ActiveFreeburnChallenge -> ChallengeNotActiveStartEvent: the challenge id at
                    // +0x00, the seven player slots at +0x08, the player count at +0x24.
                    const NetIO::NetworkOutActiveFburnChallengeEvent* lpActiveFreeburnChallengeEvent =
                        reinterpret_cast<const NetIO::NetworkOutActiveFburnChallengeEvent*>(lpEvent);
                    CGS_ASSERT(lpActiveFreeburnChallengeEvent != 0, "lpActiveFreeburnChallengeEvent");
                    BrnGui::GuiChallengeNotActiveStartEvent lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    *reinterpret_cast<CgsID*>(lpDst)     = lpActiveFreeburnChallengeEvent->mChallengeID;
                    *reinterpret_cast<s32*>(lpDst + 36)  = lpActiveFreeburnChallengeEvent->miNumPlayersInChallenge;
                    std::memcpy(lpDst + 8, lpActiveFreeburnChallengeEvent->maePlayersInChallengeARCI,
                                sizeof(lpActiveFreeburnChallengeEvent->maePlayersInChallengeARCI));
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutAccountSettings::KI_EVENT_TYPE:                       // 70
                {
                    const NetIO::NetworkOutAccountSettings* lpAccountSettings =
                        reinterpret_cast<const NetIO::NetworkOutAccountSettings*>(lpEvent);
                    CGS_ASSERT(lpAccountSettings != 0, "lpAccountSettings");
                    BrnGui::GuiEventOnlineAccountSettings lEvent;
                    RecordBytes(lEvent)[0] = lpAccountSettings->mbAgreeToShareInfoEA;
                    RecordBytes(lEvent)[1] = lpAccountSettings->mbAgreeToShareInfoPartners;
                    RecordBytes(lEvent)[2] = lpAccountSettings->mbTelemetryEnable;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 71:  // GuiEvent<127>
                {
                    CgsGui::GuiEvent<127> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case NetIO::NetworkOutCamPicCompressedEvent::KI_EVENT_TYPE:                 // 72
                {
                    // FLAG: the GUI record holds the pixel pointer in a 4-byte slot (console
                    // width); only the first 4 bytes of the host pointer reach it until the GUI
                    // record gets a pointer member.
                    const NetIO::NetworkOutCamPicCompressedEvent* lpCamPicCompressedEvent =
                        reinterpret_cast<const NetIO::NetworkOutCamPicCompressedEvent*>(lpEvent);
                    CGS_ASSERT(lpCamPicCompressedEvent != 0, "lpCamPicCompressedEvent");
                    BrnGui::GuiEventCamPicCompressed lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    *reinterpret_cast<s32*>(lpDst + 4) = lpCamPicCompressedEvent->meCompressedFormat;
                    *reinterpret_cast<s32*>(lpDst)     = lpCamPicCompressedEvent->miCompressedPixelSize;
                    std::memcpy(lpDst + 8, &lpCamPicCompressedEvent->mpcCompressedPixels, 4);
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 73:  // GuiEvent<571>
                {
                    CgsGui::GuiEvent<571> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                default:
                    break;
            }

            liEventType = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // =========================================================================
    // BridgeNetworkToGui
    // Top-level per-frame network->GUI bridge: run both translators, bulk-append the network
    // GUI event queue into the GUI input queue, then synthesise the three player-snapshot GUI
    // events (player-list / player-status / lobby-player-list). Called by DoUpdate_GUI /
    // LoadingScriptedState::Update.
    // =========================================================================
    void BrnGameModule::BridgeNetworkToGui(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                                           const OutputBuffer* lpNetworkOutput)
    {
        const NetworkToGuiInterface*       lpNetworkToGuiInterface = lpNetworkOutput->GetNetworkToGuiInterface();
        const InGamePlayerStatusInterface* lpStatusInterface       = lpNetworkOutput->GetInGamePlayerStatusInterface();

        CGS_ASSERT(lpNetworkOutput->GetInGamePlayerStatusInterface() != 0,
                   "lpNetworkOutput->GetInGamePlayerStatusInterface()");
        CGS_ASSERT(lpNetworkToGuiInterface != 0, "lpNetworkToGuiInterface");
        CGS_ASSERT(lpNetworkOutput->GetGuiEventQueue() != 0, "lpNetworkOutput->GetGuiEventQueue()");

        TranslateNetworkInterfaceToGuiEvents(lpGuiInput, lpNetworkToGuiInterface);
        TranslateNetworkEventsToGuiEvents(lpGuiInput, lpNetworkOutput);

        // Bulk-append the network output's GUI event queue into the GUI input buffer's queue.
        lpGuiInput->GetGuiEvents()->Append(*lpNetworkOutput->GetGuiEventQueue());

        // Read once; the debug roster below replaces it for all three snapshots.
        s32 liNumPlayers = lpStatusInterface->GetNumPlayers();

        // -------- GuiEventNetworkPlayerList --------------------------------------------------
        // Record: PlayerNameInfo[8] {NetworkPlayerID +0x0, PlayerName +0x4} (20-byte stride),
        // miNumPlayers +0xA0, total player count +0xA4. The GUI home is still opaque, so the
        // record is built at those offsets.
        {
            BrnGui::GuiEventNetworkPlayerList lListEvent;
            unsigned char* lpList = RecordBytes(lListEvent);
            bool lbBuiltList = false;

            if (msbPlayerListWorstCaseHudActive)
            {
                liNumPlayers = 8;
                CgsNetwork::PlayerName lName;
                lName.Construct("Mole4Avril");
                for (s32 i = 0; i < 8; ++i)
                {
                    unsigned char* lpEntry = lpList + 20 * i;
                    std::memcpy(lpEntry + 4, &lName, 16);
                    *reinterpret_cast<s32*>(lpEntry) = i;
                }
                *reinterpret_cast<s32*>(lpList + 0xA0) = 8;
                *reinterpret_cast<s32*>(lpList + 0xA4) = 8;
                lbBuiltList = true;
            }
            else if (!lpNetworkOutput->IsPlaying() && lpNetworkOutput->IsConnected())
            {
                s32 i = 0;
                for (; i < liNumPlayers; ++i)
                {
                    const InGamePlayerStatusData* lpPlayer = lpStatusInterface->GetPlayerStatusData(i);
                    unsigned char* lpEntry = lpList + 20 * i;
                    std::memcpy(lpEntry + 4, &lpPlayer->mPlayerName, 16);
                    *reinterpret_cast<s32*>(lpEntry) = lpPlayer->mNetworkPlayerID;
                }
                for (; i < 8; ++i)
                    *reinterpret_cast<s32*>(lpList + 20 * i) = -1;
                *reinterpret_cast<s32*>(lpList + 0xA0) = liNumPlayers;
                *reinterpret_cast<s32*>(lpList + 0xA4) = lpStatusInterface->GetTotalNumberPlayers();
                lbBuiltList = true;
            }

            if (lbBuiltList)
                PushGuiEvent(lListEvent, lpGuiInput);
        }

        // -------- GuiEventNetworkPlayerStatus ------------------------------------------------
        // Record: InGamePlayerStatusData maPlayerInfo[8], macGameName[36] +0x9C4 (after the
        // count), miNumPlayers +0x9C0, mbLocalPlayerIsHost +0x9E8.
        {
            BrnGui::GuiEventNetworkPlayerStatus lStatusEvent;
            unsigned char* lpBuf = RecordBytes(lStatusEvent);
            InGamePlayerStatusData* laPlayerInfo = reinterpret_cast<InGamePlayerStatusData*>(lpBuf);

            // The event's construction: each record's NetworkPlayerStats::mTimeStamp (+0x60, a
            // CgsSystem::Time, private) is default-constructed to {0, 0.0f}; then the scalar
            // tail is zeroed.
            for (s32 i = 0; i < 8; ++i)
            {
                unsigned char* lpTimeStamp = reinterpret_cast<unsigned char*>(&laPlayerInfo[i].mPlayerStats) + 0x60;
                *reinterpret_cast<s32*>(lpTimeStamp)     = 0;
                *reinterpret_cast<f32*>(lpTimeStamp + 4) = 0.0f;
            }
            lpBuf[0x9C4] = 0;
            lpBuf[0x9E8] = 0;
            *reinterpret_cast<s32*>(lpBuf + 0x9C0) = 0;
            for (s32 i = 0; i < 8; ++i)
                laPlayerInfo[i].Clear();

            s32 liFilled;
            if (msbPlayerStatusWorstCaseHudActive)
            {
                CgsNetwork::PlayerName lName;
                lName.Construct("Mole4Avril");
                s32 i = 0;
                for (; i < liNumPlayers; ++i)
                {
                    InGamePlayerStatusData& lrPlayer = laPlayerInfo[i];
                    lrPlayer.meActiveRaceCarIndex = static_cast<BrnNetwork::EActiveRaceCarIndex>(i);
                    std::memcpy(&lrPlayer.mPlayerName, &lName, 16);
                    lrPlayer.mNetworkPlayerID               = i;
                    lrPlayer.meDistrict                     = 8;
                    lrPlayer.mMarkedManPlayerID             = -1;
                    lrPlayer.meMarkedManActiveRaceCarIndex  = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
                    lrPlayer.mbMarkedMan                    = false;
                    lrPlayer.mbIsHost                       = false;
                    lrPlayer.mbIsLocalPlayer                = false;
                    lrPlayer.mbIsInLocalGameWorld           = true;
                    lrPlayer.meVOIPStatus                   = msbPlayerStatusSwitch;
                    lrPlayer.meCameraStatus                 = static_cast<BrnNetwork::ECameraStatus>(msbPlayerStatusSwitch);
                    if (i == static_cast<s32>(lpNetworkOutput->GetPlayerActiveRaceCarIndex()))
                    {
                        lrPlayer.mbIsHost        = true;
                        lrPlayer.mbIsLocalPlayer = true;
                    }
                }
                liFilled = i;
            }
            else
            {
                s32 i = 0;
                for (; i < liNumPlayers; ++i)
                {
                    // A raw 312-byte block copy, not InGamePlayerStatusData::operator=.
                    std::memcpy(&laPlayerInfo[i], lpStatusInterface->GetPlayerStatusData(i),
                                sizeof(InGamePlayerStatusData));
                }
                liFilled = i;
            }

            // Empty slots carry no race car.
            for (s32 i = liFilled; i < 8; ++i)
                laPlayerInfo[i].meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;

            // Game name (CgsStringUtils length guard, 36-byte slot), host flag, count.
            const char* lpcName = lpStatusInterface->GetGameName();
            const char* lpcScan = lpcName;
            while (*lpcScan++)
            {
            }
            CGS_ASSERT((lpcScan - lpcName - 1) < 0x24, "String too long: ");
            std::strncpy(reinterpret_cast<char*>(lpBuf + 0x9C4), lpcName, 36);
            lpBuf[0x9E8] = lpStatusInterface->GetLocalPlayerIsHost();
            *reinterpret_cast<s32*>(lpBuf + 0x9C0) = liNumPlayers;

            PushGuiEvent(lStatusEvent, lpGuiInput);
        }

        // -------- GuiEventNetworkLobbyPlayerList ---------------------------------------------
        // Record: LobbyPlayerStatusData[8] then the player count (+0x1C0).
        {
            BrnGui::GuiEventNetworkLobbyPlayerList lLobbyEvent;
            unsigned char* lpLobby = RecordBytes(lLobbyEvent);
            LobbyPlayerStatusData* laLobbyData = reinterpret_cast<LobbyPlayerStatusData*>(lpLobby);

            s32 i = 0;
            for (; i < liNumPlayers; ++i)
            {
                // The interface accessor runs once per player.
                const OnlineLobbyPlayerStatusInterface* lpLobbyInterface =
                    lpNetworkOutput->GetOnlineLobbyPlayerStatusInterface();
                std::memcpy(&laLobbyData[i], lpLobbyInterface->GetPlayerLobbyData(i),
                            sizeof(LobbyPlayerStatusData));
            }
            for (; i < 8; ++i)
            {
                LobbyPlayerStatusData& lrSlot = laLobbyData[i];
                lrSlot.mPlayerID             = -1;
                lrSlot.mbLocalPlayer         = false;
                lrSlot.mbIsHost              = false;
                lrSlot.mbFinalSelection      = false;
                lrSlot.mbIsCriterion         = false;
                lrSlot.meReadyStatus         = LobbyPlayerStatusData::E_READY_STATUS_COUNT;
                lrSlot.mePlayerTeam          = 0;
                lrSlot.meGameConnectionType  = 0;
                lrSlot.meVoipConnectionType  = 0;
                lrSlot.miPlayerColourIndex   = -1;
            }
            *reinterpret_cast<s32*>(lpLobby + 8 * sizeof(LobbyPlayerStatusData)) = liNumPlayers;

            PushGuiEvent(lLobbyEvent, lpGuiInput);
        }
    }
}
