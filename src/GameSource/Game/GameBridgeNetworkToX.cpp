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
//   * The game-state events with a home in BrnGameEvents.h (OnlinePlayerAdded / Removed /
//     Finalised, ChangeNetworkCar, RemotePlayerDisconnected) are built through their real
//     setters and members. Where the home does not carry a member yet, the remaining bytes
//     are written at their record offset and marked at the store.
//   * The records drained from the NetworkEventQueue (the network OUT events) have no header
//     in the tree, and most of the game-state / GUI records the translators build are still
//     opaque in their homes. Those are pointer-free wire images (host layout == console
//     layout), so each is read / built at its record offsets inside a buffer of the size the
//     console posts. No field names are invented for them.
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
#include "GameSource/GameState/BrnGameStateModuleIO.h"              // BrnGameState::GameStateModuleIO::PreWorldInputBuffer + events
#include "GameSource/GameState/BrnGameStateModule.h"                // GameStateModule::GetPreWorldInputBuffer
#include "GameSource/GameState/BrnCgsPlayerName.h"                  // CgsNetwork::PlayerName (Mole4Avril debug roster)
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsIDCompress (the overlay wait-finish record)
#include "GameSource/GameState/BrnGameEvents.h"                     // the game-state network events (OnlinePlayerAddedEvent ...)
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

    // NetworkOutScoreboardHeadingList::GetHeadingType() (network OUT event 52): returns the
    // record's meHeadingType (+0x4) after asserting it is not E_HEADING_COUNT (3). The record
    // type has no header in the tree, so its accessor is reproduced here, file-local.
    u32 GetScoreboardHeadingType(const unsigned char* lpHeadingList)
    {
        const s32 leHeadingType = *reinterpret_cast<const s32*>(lpHeadingList + 0x4);
        CGS_ASSERT(leHeadingType != 3, "meHeadingType != E_HEADING_COUNT");
        return static_cast<u32>(leHeadingType);
    }

    // Layout pins for the records this TU writes by member name into console-sized buffers.
    // All are pointer-free, so the host offsets equal the console ones.
    typedef BrnGameState::GameStateModuleIO::OnlinePlayerAddedEvent        PinAdded;
    typedef BrnGameState::GameStateModuleIO::OnlinePlayerRemovedEvent      PinRemoved;
    typedef BrnGameState::GameStateModuleIO::OnlinePlayerFinalisedEvent    PinFinalised;
    typedef BrnGameState::GameStateModuleIO::ChangeNetworkCarEvent         PinChangeCar;
    typedef BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedEvent PinDisconnected;
    typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData         PinStatus;
    static_assert(offsetof(PinAdded, mModelID)               == 0x00, "OnlinePlayerAddedEvent mModelID @ +0x00");
    static_assert(offsetof(PinAdded, mWheelID)               == 0x08, "OnlinePlayerAddedEvent mWheelID @ +0x08");
    static_assert(offsetof(PinAdded, mNetworkPlayerID)       == 0x10, "OnlinePlayerAddedEvent mNetworkPlayerID @ +0x10");
    static_assert(offsetof(PinRemoved, mNetworkPlayerID)      == 0, "OnlinePlayerRemovedEvent mNetworkPlayerID @ +0x0");
    static_assert(offsetof(PinFinalised, mNetworkPlayerID)    == 0, "OnlinePlayerFinalisedEvent mNetworkPlayerID @ +0x0");
    static_assert(offsetof(PinChangeCar, mNetworkPlayerID)    == 0, "ChangeNetworkCarEvent mNetworkPlayerID @ +0x0");
    static_assert(offsetof(PinDisconnected, mNetworkPlayerID) == 0, "RemotePlayerDisconnectedEvent mNetworkPlayerID @ +0x0");
    static_assert(sizeof(PinStatus) == 312, "InGamePlayerStatusData record stride (0x138)");
    static_assert(offsetof(PinStatus, mPlayerName)          == 0x100, "mPlayerName @ +0x100");
    static_assert(offsetof(PinStatus, mNetworkPlayerID)     == 0x110, "mNetworkPlayerID @ +0x110");
    static_assert(offsetof(PinStatus, meActiveRaceCarIndex) == 0x114, "meActiveRaceCarIndex @ +0x114");
    static_assert(offsetof(PinStatus, mbIsInLocalGameWorld) == 0x12F, "mbIsInLocalGameWorld @ +0x12F");
    static_assert(sizeof(BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData) == 56, "LobbyPlayerStatusData record stride (0x38)");
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
    int BrnGameModule::TranslateNetworkInterfaceToGuiEvents(
        void* lpGuiBuffer, const void* lpNetworkToGuiInterfaceIn)
    {
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput =
            static_cast<CgsGui::CgsGuiModuleIO::InputBuffer*>(lpGuiBuffer);
        const NetworkToGuiInterface* lpNetworkToGuiInterface =
            static_cast<const NetworkToGuiInterface*>(lpNetworkToGuiInterfaceIn);

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
        // The console function returns nothing (the declaration's int is not the console's
        // void); the caller discards it.
        return 0;
    }

    // =========================================================================
    // TranslateNetworkEventsToGameEvents
    // Drain the network OUTPUT buffer's NetworkEventQueue and translate each game-state-bound
    // network event into the matching game-state event, AddEvent'd into the PreWorld input
    // buffer's game-event queue under the console's event id and record size.
    // =========================================================================
    int BrnGameModule::TranslateNetworkEventsToGameEvents(
        BrnGameState::GameStateModule* lpGameStateModule, const OutputBuffer* lpNetworkOutput)
    {
        // The console function receives the PreWorld input buffer itself; the declaration hands
        // over its owning module, so the buffer is fetched once here.
        BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpGameStateInput =
            lpGameStateModule->GetPreWorldInputBuffer();

        const NetworkEventQueue* lpNetworkEventQueue = lpNetworkOutput->GetNetworkEventQueue();
        CGS_ASSERT(lpNetworkEventQueue != 0, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        int result = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            // The network OUT event records have no header in the tree; they are read at their
            // record offsets (pointer-free, host == console).
            const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpEvent);
            const int* lpW = reinterpret_cast<const int*>(lpRecord);

            alignas(16) unsigned char lOut[160];   // widest record built below is 156 bytes (case 6)
            s32 liOutSize = 0;
            s32 liOutType = 0;
            bool lbEmit = false;

            switch (result)
            {
                case 6:   // InviteRequest -> 156-byte record
                    CGS_ASSERT(lpRecord != 0, "lpInviteRequestEvent");
                    std::memcpy(lOut, lpRecord, 156);
                    liOutSize = 156; liOutType = 57; lbEmit = true;
                    break;
                case 12:  // InstantFreeburn -> 1-byte flag
                    CGS_ASSERT(lpRecord != 0, "lpInstantFreeburnEvent");
                    lOut[0] = lpRecord[0];
                    liOutSize = 1; liOutType = 151; lbEmit = true;
                    break;
                case 14:  // BuddyRemoved -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpBuddyRemovedEvent");
                    std::memcpy(lOut, lpRecord, 16);
                    liOutSize = 16; liOutType = 150; lbEmit = true;
                    break;
                case 15:  // player-car-select status -> 8-byte {status,flag}
                    CGS_ASSERT(lpRecord != 0, "lpEvent");
                    lOut[4] = lpRecord[0x1DC];                                              // flag byte
                    reinterpret_cast<s32*>(lOut)[0] = *reinterpret_cast<const s32*>(lpRecord + 0x1B8); // status word
                    CGS_ASSERT(lpGameStateInput != 0, "lpGameStateInput");
                    CGS_ASSERT(lpGameStateInput->GetGameEventQueue() != 0,
                               "lpGameStateInput->GetGameEventQueue()");
                    liOutSize = 8; liOutType = 125; lbEmit = true;
                    break;
                case 16:  // OnlinePlayerAdded -> OnlinePlayerAddedEvent (40-byte record)
                {
                    BrnGameState::GameStateModuleIO::OnlinePlayerAddedEvent* lpAdded =
                        reinterpret_cast<BrnGameState::GameStateModuleIO::OnlinePlayerAddedEvent*>(lOut);
                    lpAdded->SetNetworkPlayerID(*reinterpret_cast<const s32*>(lpRecord + 0x10));
                    lpAdded->mModelID = *reinterpret_cast<const CgsID*>(lpRecord + 0x00);
                    lpAdded->mWheelID = *reinterpret_cast<const CgsID*>(lpRecord + 0x08);
                    // +0x14..+0x20 have no member in BrnGameEvents.h yet (meTeam, a float, the car
                    // colour and paint-finish indices, mbIsLocalPlayer); stored at their offsets.
                    *reinterpret_cast<s32*>(lOut + 0x14) = *reinterpret_cast<const s32*>(lpRecord + 0x14);
                    *reinterpret_cast<f32*>(lOut + 0x18) = *reinterpret_cast<const f32*>(lpRecord + 0x18);
                    *reinterpret_cast<u16*>(lOut + 0x1C) = *reinterpret_cast<const u16*>(lpRecord + 0x1C);
                    *reinterpret_cast<u16*>(lOut + 0x1E) = *reinterpret_cast<const u16*>(lpRecord + 0x1E);
                    lOut[0x20] = lpRecord[0x20];
                    liOutSize = 40; liOutType = 127; lbEmit = true;
                    break;
                }
                case 17:  // OnlinePlayerRemoved -> OnlinePlayerRemovedEvent (8-byte record)
                {
                    BrnGameState::GameStateModuleIO::OnlinePlayerRemovedEvent* lpRemoved =
                        reinterpret_cast<BrnGameState::GameStateModuleIO::OnlinePlayerRemovedEvent*>(lOut);
                    lpRemoved->SetNetworkPlayerID(lpW[0]);
                    lOut[0x4] = lpRecord[0x14];   // mbIsLocalPlayerInGame (+0x4): no member in BrnGameEvents.h yet
                    liOutSize = 8; liOutType = 129; lbEmit = true;
                    break;
                }
                case 18:  // ChangeNetworkCar -> ChangeNetworkCarEvent (32-byte record)
                {
                    BrnGameState::GameStateModuleIO::ChangeNetworkCarEvent* lpChange =
                        reinterpret_cast<BrnGameState::GameStateModuleIO::ChangeNetworkCarEvent*>(lOut);
                    lpChange->SetNetworkPlayerID(*reinterpret_cast<const s32*>(lpRecord + 0x14));
                    // mCarModelId (+0x08), mWheelModelId (+0x10) and a trailing float (+0x18) have no
                    // member in BrnGameEvents.h yet; stored at their offsets.
                    *reinterpret_cast<u64*>(lOut + 0x08) = *reinterpret_cast<const u64*>(lpRecord + 0x00);
                    *reinterpret_cast<u64*>(lOut + 0x10) = *reinterpret_cast<const u64*>(lpRecord + 0x08);
                    *reinterpret_cast<f32*>(lOut + 0x18) = *reinterpret_cast<const f32*>(lpRecord + 0x10);
                    liOutSize = 32; liOutType = 7; lbEmit = true;
                    break;
                }
                case 20:  // OnlinePlayerFinalised -> OnlinePlayerFinalisedEvent (4-byte record)
                    reinterpret_cast<BrnGameState::GameStateModuleIO::OnlinePlayerFinalisedEvent*>(lOut)
                        ->SetNetworkPlayerID(lpW[0]);
                    liOutSize = 4; liOutType = 128; lbEmit = true;
                    break;
                case 21:  // TeamSelection -> 32-byte record
                    CGS_ASSERT(lpRecord != 0, "lpTeamSelectionEvent");
                    std::memcpy(lOut, lpRecord, 32);
                    liOutSize = 32; liOutType = 154; lbEmit = true;
                    break;
                case 23:  // PlayerDisconnected (only when subtype==2) -> RemotePlayerDisconnectedEvent
                    CGS_ASSERT(lpRecord != 0, "lpPlayerDisconnectedEvent");
                    if (lpW[1] != 2)
                        break;
                    reinterpret_cast<BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedEvent*>(lOut)
                        ->SetNetworkPlayerID(lpW[0]);
                    liOutSize = 4; liOutType = 121; lbEmit = true;
                    break;
                case 24:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 124; lbEmit = true;
                    break;
                case 27:  // Launched -> 8-byte {id, flagA, flagB}
                    CGS_ASSERT(lpRecord != 0, "lpLaunchedEvent");
                    lOut[5] = lpRecord[5];
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    lOut[4] = lpRecord[4];
                    liOutSize = 8; liOutType = 126; lbEmit = true;
                    break;
                case 34:  // StuntMultiplier -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpStuntMultiplierEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    liOutSize = 16; liOutType = 143; lbEmit = true;
                    break;
                case 35:  // RoadRulesPersonalBest -> 72-byte record
                    CGS_ASSERT(lpRecord != 0, "lpRoadRulesPersonalBest");
                    std::memcpy(lOut, lpRecord, 56);
                    reinterpret_cast<s32*>(lOut)[14] = lpW[14];
                    reinterpret_cast<s32*>(lOut)[15] = lpW[15];
                    lOut[64] = lpRecord[64];
                    liOutSize = 72; liOutType = 130; lbEmit = true;
                    break;
                case 36:  // RoadRulesUploaded -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpRoadRulesUploadedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    liOutSize = 8; liOutType = 131; lbEmit = true;
                    break;
                case 37:  // RoadRulesDownloaded -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpRoadRulesDownloadedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 132; lbEmit = true;
                    break;
                case 38:  // NetworkRRConnect -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpNetworkRRConnectEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 133; lbEmit = true;
                    break;
                case 39:  // PlayerConnected -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpPlayerConnectedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 122; lbEmit = true;
                    break;
                case 44:  // Collectable -> 16-byte record
                    CGS_ASSERT(lpRecord != 0, "lpCollectableEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    liOutSize = 16; liOutType = 135; lbEmit = true;
                    break;
                case 45:  // NetworkNewHost -> 2-byte record (two adjacent bytes)
                    CGS_ASSERT(lpRecord != 0, "lpNetworkNewHostEvent");
                    lOut[0] = lpRecord[0];
                    lOut[1] = lpRecord[1];
                    liOutSize = 2; liOutType = 140; lbEmit = true;
                    break;
                case 46:  // NetworkCheckpoint -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpNetworkCheckpointEvent");
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 8; liOutType = 141; lbEmit = true;
                    break;
                case 47:  // NetworkStuntScoreUpdated -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpNetworkStuntScoreUpdatedEvent");
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 8; liOutType = 142; lbEmit = true;
                    break;
                case 49:  // NetworkRivalCount -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpNetworkRivalCountEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 144; lbEmit = true;
                    break;
                case 50:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 146; lbEmit = true;
                    break;
                case 53:  // TargetScore -> 40-byte record: a straight copy of its first 0x25 bytes
                    CGS_ASSERT(lpRecord != 0, "lpTargetScoreEvent");
                    std::memcpy(lOut, lpRecord, 0x20);                      // three + one qwords
                    reinterpret_cast<s32*>(lOut)[8] = lpW[8];               // +0x20
                    lOut[0x24] = lpRecord[0x24];
                    liOutSize = 40; liOutType = 152; lbEmit = true;
                    break;
                case 54:  // DldChallengeable -> 8-byte record
                    CGS_ASSERT(lpRecord != 0, "lpDldChallengeableEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    liOutSize = 8; liOutType = 153; lbEmit = true;
                    break;
                case 55:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 79; lbEmit = true;
                    break;
                case 57:  // ImageReceived -> 16-byte record: {rec+0x8, rec+0xC, qword rec+0x0}
                    CGS_ASSERT(lpRecord != 0, "lpImageReceivedEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[3];
                    *reinterpret_cast<u64*>(lOut + 0x8) = *reinterpret_cast<const u64*>(lpRecord);
                    liOutSize = 16; liOutType = 136; lbEmit = true;
                    break;
                case 58:  // MugshotToSave -> 32-byte record: a straight copy
                    CGS_ASSERT(lpRecord != 0, "lpMugshotToSaveEvent");
                    std::memcpy(lOut, lpRecord, 0x18);                      // three qwords
                    reinterpret_cast<s32*>(lOut)[6] = lpW[6];               // +0x18
                    reinterpret_cast<s32*>(lOut)[7] = lpW[7];               // +0x1C
                    liOutSize = 32; liOutType = 156; lbEmit = true;
                    break;
                case 59:  // AbortCapture -> 1-byte record
                    CGS_ASSERT(lpRecord != 0, "lpAbortCaptureEvent");
                    lOut[0] = lpRecord[0];
                    liOutSize = 1; liOutType = 138; lbEmit = true;
                    break;
                case 60:  // empty 1-byte notify
                    liOutSize = 1; liOutType = 147; lbEmit = true;
                    break;
                case 61:  // NetworkSwitchRunner -> 4-byte word
                    CGS_ASSERT(lpRecord != 0, "lpNetworkSwitchRunnerEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    liOutSize = 4; liOutType = 155; lbEmit = true;
                    break;
                case 62:  // BurnoutSkillz -> 64-byte record {word, 56-byte block, byte}
                    CGS_ASSERT(lpRecord != 0, "lpBurnoutSkillzNetworkEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    std::memcpy(lOut + 4, lpRecord + 4, 56);
                    lOut[0x3C] = lpRecord[0x3C];
                    liOutSize = 64; liOutType = 139; lbEmit = true;
                    break;
                case 63:  // ShowtimeUpdate -> 8-byte {w0,w1}
                    CGS_ASSERT(lpRecord != 0, "lpShowtimeUpdateNetworkEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    liOutSize = 8; liOutType = 49; lbEmit = true;
                    break;
                case 64:  // ShowtimeModeSwitch -> 12-byte {w0,w1,flag}
                    CGS_ASSERT(lpRecord != 0, "lpShowtimeModeSwitchNetworkEvent");
                    reinterpret_cast<s32*>(lOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lOut)[1] = lpW[1];
                    lOut[8] = lpRecord[8];
                    liOutSize = 12; liOutType = 50; lbEmit = true;
                    break;
                case 65:  // FreeburnChallenge -> variant sub-switch on the +0x10 word
                {
                    CGS_ASSERT(lpRecord != 0, "lpFreeburnChallengeNetworkEvent");
                    const int liVariant = lpW[4];
                    if (liVariant == 0)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        liOutSize = 8; liOutType = 163;
                    }
                    else if (liVariant == 1)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        liOutSize = 8; liOutType = 164;
                    }
                    else if (liVariant == 3)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[6];
                        liOutSize = 16; liOutType = 166;
                    }
                    else if (liVariant == 4)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[6];
                        liOutSize = 16; liOutType = 167;
                    }
                    else if (liVariant == 2)
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[6];
                        liOutSize = 16; liOutType = 165;
                    }
                    else
                    {
                        reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[1];
                        reinterpret_cast<s32*>(lOut)[2] = lpW[5];
                        liOutSize = 16; liOutType = 168;
                    }
                    lbEmit = true;
                    break;
                }
                case 66:  // ActiveFreeburnChallenge -> 48-byte record
                    CGS_ASSERT(lpRecord != 0, "lpActiveFreeburnChallengeEvent");
                    *reinterpret_cast<u64*>(lOut + 0x20) = *reinterpret_cast<const u64*>(lpRecord + 0x20);
                    *reinterpret_cast<s32*>(lOut + 0x28) = *reinterpret_cast<const s32*>(lpRecord + 0x28);
                    std::memcpy(lOut, lpRecord, 28);
                    liOutSize = 48; liOutType = 173; lbEmit = true;
                    break;
                case 67:  // FburnChallengeSuccessUpdate -> 24-byte record
                    CGS_ASSERT(lpRecord != 0, "lpFburnChallengeSuccessUpdateEvent");
                    reinterpret_cast<u64*>(lOut)[0] = reinterpret_cast<const u64*>(lpW)[0];
                    reinterpret_cast<s32*>(lOut)[2] = lpW[2];
                    reinterpret_cast<s32*>(lOut)[3] = lpW[3];
                    reinterpret_cast<s32*>(lOut)[4] = lpW[4];
                    liOutSize = 24; liOutType = 171; lbEmit = true;
                    break;
                case 68:  // FburnChallengeSuccess -> 20-byte record
                    CGS_ASSERT(lpRecord != 0, "lpFburnChallengeSuccessEvent");
                    reinterpret_cast<s32*>(lOut)[3] = lpW[2];               // +0xC <- +0x8
                    std::memcpy(lOut, lpRecord, 8);
                    std::memcpy(lOut + 0x8, lpRecord + 0x10, 2);
                    std::memcpy(lOut + 0xA, lpRecord + 0x12, 2);
                    reinterpret_cast<s32*>(lOut)[4] = lpW[3];               // +0x10 <- +0xC
                    liOutSize = 20; liOutType = 172; lbEmit = true;
                    break;
                case 74:  // UploadedModeScores -> 128-byte record
                    CGS_ASSERT(lpRecord != 0, "lpUploadedModeScoresEvent");
                    std::memcpy(lOut, lpRecord, 128);
                    liOutSize = 128; liOutType = 175; lbEmit = true;
                    break;
                default:
                    break;
            }

            if (lbEmit)
            {
                lpGameStateInput->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lOut), liOutType, liOutSize);
            }

            result = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        return result;
    }

    // -------------------------------------------------------------------------
    // Scoreboard-response string copy guard (X360 CgsStringUtils inline): scan the source
    // name to its NUL, assert length < 31 ("String too long: " verbatim, HARD RULE A), then
    // strncpy 31 bytes into the destination name slot.
    // -------------------------------------------------------------------------
    static void CopyScoreboardName(unsigned char* lpDst, const unsigned char* lpcName)
    {
        const char* lpcScan = reinterpret_cast<const char*>(lpcName);
        while (*lpcScan++)
        {
        }
        CGS_ASSERT((lpcScan - reinterpret_cast<const char*>(lpcName) - 1) < 31, "String too long: ");
        std::strncpy(reinterpret_cast<char*>(lpDst), reinterpret_cast<const char*>(lpcName), 31);
    }

    // =========================================================================
    // TranslateScoreboardResponse -- the case-52 heading-type sub-switch (inlined into
    // TranslateNetworkEventsToGuiEvents). The scoreboard heading-list record carries a count
    // word @ +0 and a packed list of 31-byte names @ +8. Its heading type
    // (NetworkOutScoreboardHeadingList::GetHeadingType) selects which scoreboard GUI event the
    // names are copied into.
    // =========================================================================
    void BrnGameModule::TranslateScoreboardResponse(void* lpGuiBuffer, const unsigned char* lpRecord)
    {
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput =
            static_cast<CgsGui::CgsGuiModuleIO::InputBuffer*>(lpGuiBuffer);
        const unsigned int luHeadingType = GetScoreboardHeadingType(lpRecord);
        const int liCount = *reinterpret_cast<const int*>(lpRecord);

        if (luHeadingType == 0)
        {
            // category (cap KI_MAX_CATEGORIES == 15)
            BrnGui::GuiEventScoreboardResponseCategoryEvent lEvent;
            unsigned char* lpDst = RecordBytes(lEvent);
            *reinterpret_cast<int*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 15,
                "lCategoryEvent.miNumberOfCategories <= BrnGui::GuiEventScoreboardResponseCategoryEvent::KI_MAX_CATEGORIES");
            for (int i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpRecord + 8 + 31 * i);
            PushGuiEvent(lEvent, lpGuiInput);
        }
        else if (luHeadingType == 1)
        {
            // index (cap KI_MAX_INDEXES == 10)
            BrnGui::GuiEventScoreboardResponseIndexEvent lEvent;
            unsigned char* lpDst = RecordBytes(lEvent);
            *reinterpret_cast<int*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 10,
                "lIndexEvent.miNumberOfIndexes <= BrnGui::GuiEventScoreboardResponseIndexEvent::KI_MAX_INDEXES");
            for (int i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpRecord + 8 + 31 * i);
            PushGuiEvent(lEvent, lpGuiInput);
        }
        else if (luHeadingType < 3)
        {
            // variation (cap KI_MAX_VARIATIONS == 66); two trailing source bytes @ record +2054/+2055
            // land at event-buffer +2050/+2051 (X360: lbz r11,0x806/0x807(r26) -> stb var_C1E/var_C1D
            // == buffer 0x802/0x803), immediately after the last 31-byte name, NOT at +2054/+2055.
            BrnGui::GuiEventScoreboardResponseVariationEvent lEvent;
            unsigned char* lpDst = RecordBytes(lEvent);
            *reinterpret_cast<int*>(lpDst) = liCount;
            CGS_ASSERT(liCount <= 66,
                "lVariationEvent.miNumberOfVariations <= BrnGui::GuiEventScoreboardResponseVariationEvent::KI_MAX_VARIATIONS");
            for (int i = 0; i < liCount; ++i)
                CopyScoreboardName(lpDst + 4 + 31 * i, lpRecord + 8 + 31 * i);
            lpDst[2050] = lpRecord[2054];
            lpDst[2051] = lpRecord[2055];
            PushGuiEvent(lEvent, lpGuiInput);
        }
        else
        {
            // Invalid heading type (X360 builds "Invalid heading type : <n>\n"; collapsed to the
            // rodata prefix per HARD RULE A).
            CGS_ASSERT(false, "Invalid heading type : ");
        }
    }

    // =========================================================================
    // TranslateNetworkEventsToGuiEvents  (X360 0x823E0900)
    // Drain the network event queue (VariableEventQueue<14000,16>) and translate each GUI-bound
    // network event into the matching CgsGui::GuiModule event (this + 7252512).
    // =========================================================================
    int BrnGameModule::TranslateNetworkEventsToGuiEvents(void* lpGuiBuffer, const OutputBuffer* lpNetworkOutput)
    {
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput =
            static_cast<CgsGui::CgsGuiModuleIO::InputBuffer*>(lpGuiBuffer);

        const NetworkEventQueue* lpNetworkEventQueue = lpNetworkOutput->GetNetworkEventQueue();
        CGS_ASSERT(lpNetworkEventQueue != 0, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        int result = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent)
        {
            const unsigned char* lpRecord = reinterpret_cast<const unsigned char*>(lpEvent);
            const int* lpW = reinterpret_cast<const int*>(lpRecord);

            switch (result)
            {
                case 1:   // OnlineNumFriendsCount -> word0
                {
                    BrnGui::GuiEventOnlineNumFriendsCount lEvent;
                    *reinterpret_cast<int*>(&lEvent) = lpW[0];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 2:   // OnlineReceiveFriendInfo -> 660-byte copy + {word,word,byte} tail
                {
                    BrnGui::GuiEventOnlineReceiveFriendInfo lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    std::memcpy(lpDst, lpRecord, 660);
                    *reinterpret_cast<int*>(lpDst + 660) = *reinterpret_cast<const int*>(lpRecord + 660);
                    *reinterpret_cast<int*>(lpDst + 664) = *reinterpret_cast<const int*>(lpRecord + 664);
                    lpDst[668] = lpRecord[668];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 8:   // InviteFailed -> word0
                {
                    BrnGui::GuiEventInviteFailed lEvent;
                    *reinterpret_cast<int*>(&lEvent) = lpW[0];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 9:   // BuddyNotification -> qword + 16 bytes (24 bytes total)
                {
                    BrnGui::GuiEventBuddyNotification lEvent;
                    std::memcpy(RecordBytes(lEvent), lpRecord, 24);
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 10:  // GuiEvent<103>
                {
                    CgsGui::GuiEvent<103> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 11:  // GuiEvent<95>
                {
                    CgsGui::GuiEvent<95> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 13:  // FreeburnComplete -> conditional GuiEvent<285> then GuiEvent<95>
                {
                    CGS_ASSERT(lpRecord != 0, "lpFreeburnComplete");
                    if (lpRecord[0] == 0)
                    {
                        CgsGui::GuiEvent<285> lEventA;
                        PushGuiEvent(lEventA, lpGuiInput);
                    }
                    CgsGui::GuiEvent<95> lEventB;
                    PushGuiEvent(lEventB, lpGuiInput);
                    break;
                }
                case 15:  // NetworkGameParams -> 440-byte copy + reshuffled 40-byte tail
                {
                    CGS_ASSERT(lpRecord != 0, "lpParamsChangedEvent");
                    BrnGui::GuiEventNetworkGameParams lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    std::memcpy(lpDst, lpRecord, 440);
                    *reinterpret_cast<int*>(lpDst + 440) = *reinterpret_cast<const int*>(lpRecord + 440);
                    *reinterpret_cast<int*>(lpDst + 444) = *reinterpret_cast<const int*>(lpRecord + 444);
                    *reinterpret_cast<int*>(lpDst + 448) = *reinterpret_cast<const int*>(lpRecord + 448);
                    *reinterpret_cast<int*>(lpDst + 452) = *reinterpret_cast<const int*>(lpRecord + 452);
                    *reinterpret_cast<int*>(lpDst + 456) = *reinterpret_cast<const int*>(lpRecord + 456);
                    *reinterpret_cast<int*>(lpDst + 460) = *reinterpret_cast<const int*>(lpRecord + 472);
                    *reinterpret_cast<int*>(lpDst + 464) = *reinterpret_cast<const int*>(lpRecord + 460);
                    *reinterpret_cast<int*>(lpDst + 468) = *reinterpret_cast<const int*>(lpRecord + 464);
                    *reinterpret_cast<int*>(lpDst + 472) = *reinterpret_cast<const int*>(lpRecord + 468);
                    lpDst[476] = lpRecord[477];
                    lpDst[477] = lpRecord[478];
                    lpDst[478] = lpRecord[479];
                    lpDst[479] = lpRecord[476];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 17:  // PlayerRemoved -> NetworkPlayerLeftLobby (only when record[21]==0)
                {
                    CGS_ASSERT(lpRecord != 0, "lpPlayerRemovedEvent");
                    if (lpRecord[21] == 0)
                    {
                        BrnGui::GuiEventNetworkPlayerLeftLobby lEvent;
                        unsigned char* lpDst = RecordBytes(lEvent);
                        *reinterpret_cast<int*>(lpDst) = lpW[0];
                        std::memcpy(lpDst + 4, lpRecord + 4, 16);
                        lpDst[20] = lpRecord[20];
                        PushGuiEvent(lEvent, lpGuiInput);
                    }
                    break;
                }
                case 22:  // OnlinePostEventScalps -> 64-byte copy + word tail
                {
                    BrnGui::GuiEventOnlinePostEventScalps lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    std::memcpy(lpDst, lpRecord, 64);
                    *reinterpret_cast<int*>(lpDst + 64) = *reinterpret_cast<const int*>(lpRecord + 64);
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 24:  // NetworkLeftGame -> {word0, word1}
                {
                    CGS_ASSERT(lpRecord != 0, "lpEvent");
                    BrnGui::GuiEventNetworkLeftGame lEvent;
                    reinterpret_cast<int*>(&lEvent)[0] = lpW[0];
                    reinterpret_cast<int*>(&lEvent)[1] = lpW[1];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 25:  // NetworkPostGameProcessingFinished -> byte0
                {
                    CGS_ASSERT(lpRecord != 0, "lpPostGameProcessingFinished");
                    BrnGui::GuiEventNetworkPostGameProcessingFinished lEvent;
                    RecordBytes(lEvent)[0] = lpRecord[0];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 26:  // NetworkLaunching -> {word0, word1}
                {
                    CGS_ASSERT(lpRecord != 0, "lpLaunchEvent");
                    CgsGui::GuiEventNetworkLaunching lEvent;
                    reinterpret_cast<int*>(&lEvent)[0] = lpW[0];
                    reinterpret_cast<int*>(&lEvent)[1] = lpW[1];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 27:  // Launch overlay wait-finish requests (two request names)
                {
                    // GuiOverlayWaitFinishRequest::Construct is one CgsIDCompress stored as the
                    // record's single 8-byte word.
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
                case 29:  // LiveRevengeProfileData -> word0
                {
                    BrnGui::GuiEventLiveRevengeProfileData lEvent;
                    *reinterpret_cast<int*>(&lEvent) = lpW[0];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 30:  // GuiEvent<280>
                {
                    CgsGui::GuiEvent<280> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 31:  // OnlineTimeout -> single float: record[4](float) + (float)record[0](int)
                {
                    BrnGui::GuiEventOnlineTimeout lEvent;
                    const float lfBase = *reinterpret_cast<const float*>(lpRecord + 4);
                    *reinterpret_cast<float*>(&lEvent) = lfBase + static_cast<float>(lpW[0]);
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 32:  // AutosaveRequest -> byte0 = 0
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
                case 48:  // OnlineCarStatus -> {word0, byte@+4}
                {
                    CGS_ASSERT(lpRecord != 0, "lpPlayerCarSelectStatus");
                    BrnGui::GuiOnlineCarStatusEvent lEvent;
                    *reinterpret_cast<int*>(&lEvent) = lpW[0];
                    RecordBytes(lEvent)[4] = lpRecord[4];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 51:  // ScoreboardResponseTable -> 2924-byte copy
                {
                    BrnGui::GuiEventScoreboardResponseTableEvent lEvent;
                    std::memcpy(RecordBytes(lEvent), lpRecord, 2924);
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 52:  // ScoreboardResponse (category / index / variation heading sub-switch)
                    TranslateScoreboardResponse(lpGuiBuffer, lpRecord);
                    break;
                case 56:  // CapturingImage -> MugshotControlEvent (+ filter-gated debug spew)
                {
                    CGS_ASSERT(lpRecord != 0, "lpCapturingImageEvent");
                    BrnGui::GuiMugshotControlEvent lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    std::memcpy(lpDst, lpRecord, 8);                              // qword @ +0
                    *reinterpret_cast<int*>(lpDst + 8) = 1;
                    *reinterpret_cast<int*>(lpDst + 12) = lpW[2];                 // record[8]
                    *reinterpret_cast<int*>(lpDst + 16) = lpW[3];                 // record[12]
                    lpDst[20] = 1;
                    // FLAG: sub_821F0E50/sub_8229F668/sub_82203EE8 are un-homed StrStreamBase
                    // insertion overloads; modelled by their operand-typed committed overload.
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE NETWORK TO X (1)\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "IMAGE TYPE : " << lpW[2] << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "AGG ARCI   : " << lpW[3] << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << static_cast<s32>(1) << "\n";
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                        *CgsDev::Log::gpDebugPrint << "ROAD?      : " << *reinterpret_cast<const u64*>(lpRecord) << "\n";
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 59:  // AbortCapture -> MugshotControlEvent (+ filter-gated debug spew)
                {
                    CGS_ASSERT(lpRecord != 0, "lpEvent");
                    BrnGui::GuiMugshotControlEvent lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    const s32 liResponse = (lpRecord[0] == 0) ? 5 : 4;
                    *reinterpret_cast<u64*>(lpDst) = 0;                           // qword @ +0
                    *reinterpret_cast<int*>(lpDst + 8) = liResponse;
                    *reinterpret_cast<int*>(lpDst + 12) = 1;
                    *reinterpret_cast<int*>(lpDst + 16) = -1;
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
                case 66:  // ActiveFreeburnChallenge -> ChallengeNotActiveStartEvent (40-byte record)
                {
                    CGS_ASSERT(lpRecord != 0, "lpActiveFreeburnChallengeEvent");
                    BrnGui::GuiChallengeNotActiveStartEvent lEvent;
                    unsigned char* lpDst = RecordBytes(lEvent);
                    std::memcpy(lpDst, lpRecord + 32, 8);                         // qword @ +0 = record[32..40)
                    std::memcpy(lpDst + 8, lpRecord, 28);                        // record[0..28)
                    *reinterpret_cast<int*>(lpDst + 36) = lpW[10];               // record[40]
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 70:  // OnlineAccountSettings -> 3 bytes
                {
                    CGS_ASSERT(lpRecord != 0, "lpAccountSettings");
                    BrnGui::GuiEventOnlineAccountSettings lEvent;
                    RecordBytes(lEvent)[0] = lpRecord[0];
                    RecordBytes(lEvent)[1] = lpRecord[1];
                    RecordBytes(lEvent)[2] = lpRecord[2];
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 71:  // GuiEvent<127>
                {
                    CgsGui::GuiEvent<127> lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 72:  // CamPicCompressed -> 3 words
                {
                    CGS_ASSERT(lpRecord != 0, "lpCamPicCompressedEvent");
                    BrnGui::GuiEventCamPicCompressed lEvent;
                    int* lpDst = reinterpret_cast<int*>(&lEvent);
                    lpDst[0] = lpW[0];
                    lpDst[1] = lpW[1];
                    lpDst[2] = lpW[2];
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

            result = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        return result;
    }

    // =========================================================================
    // BridgeNetworkToGui
    // Top-level per-frame network->GUI bridge: run both translators, bulk-append the network
    // GUI event queue into the GUI input queue, then synthesise the three player-snapshot GUI
    // events (player-list / player-status / lobby-player-list). Called by DoUpdate_GUI /
    // LoadingScriptedState::Update.
    // =========================================================================
    int BrnGameModule::BridgeNetworkToGui(void* lpGuiBuffer, const OutputBuffer* lpNetworkOutput)
    {
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput =
            static_cast<CgsGui::CgsGuiModuleIO::InputBuffer*>(lpGuiBuffer);

        const NetworkToGuiInterface*       lpNetworkToGuiInterface = lpNetworkOutput->GetNetworkToGuiInterface();
        const InGamePlayerStatusInterface* lpStatusInterface       = lpNetworkOutput->GetInGamePlayerStatusInterface();

        CGS_ASSERT(lpNetworkOutput->GetInGamePlayerStatusInterface() != 0,
                   "lpNetworkOutput->GetInGamePlayerStatusInterface()");
        CGS_ASSERT(lpNetworkToGuiInterface != 0, "lpNetworkToGuiInterface");
        CGS_ASSERT(lpNetworkOutput->GetGuiEventQueue() != 0, "lpNetworkOutput->GetGuiEventQueue()");

        TranslateNetworkInterfaceToGuiEvents(lpGuiBuffer, lpNetworkToGuiInterface);
        TranslateNetworkEventsToGuiEvents(lpGuiBuffer, lpNetworkOutput);

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
                // InGamePlayerStatusInterface::miTotalNumberPlayers (+0x9E8) is private and has no
                // accessor yet; the interface is pointer-free (host == console), so it is read at
                // its offset until the header grows the getter.
                *reinterpret_cast<s32*>(lpList + 0xA4) = *reinterpret_cast<const s32*>(
                    reinterpret_cast<const unsigned char*>(lpStatusInterface) + 0x9E8);
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

        // The console function returns nothing (the declaration's int is not the console's
        // void); the caller discards it.
        return 0;
    }
}
