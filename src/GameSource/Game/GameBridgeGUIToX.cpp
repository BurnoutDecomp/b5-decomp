#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf (telemetry "%i")
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (the invite-action assert)
#include "GameSource/GameState/BrnCgsPlayerName.h"                    // CgsNetwork::PlayerName
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"           // TelemetryData, ETelemetryHook
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"             // the typed network IN-event leaves
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGUIToX.cpp"
#include <cstring>   // std::memcpy (the console's memcpy block-copies)

// GUI-output bridge family (reference home GameSource/Game/GameBridgeGUIToX.cpp). Each bridge
// walks the GUI output buffer's out-event queue (VariableEventQueue<18432,16> at +0x814, via
// GetGuiOutEventQueue) and republishes or translates the queued GUI events into a downstream
// module's INPUT buffer.
//
// Siblings split out so they could mount first: BridgeGuiToGameState lives in
// GameBridgeGUIToX_GameState.cpp and BridgeGuiToSound in GameBridgeGUIToX_Sound.cpp.
// This TU keeps BridgeGuiToReplay_PostSim and TranslateGuiEventsToNetworkEvents.

namespace BrnGame
{
    // @ 0x823CCA00 -- forward replay-bound GUI events to the replay module's post-sim GUI
    // event queue (ids 349 / 525..533 / 594), and register the case-595 serialiser with the
    // replay request interface.
    void BrnGameModule::BridgeGuiToReplay_PostSim(
        BrnReplays::ReplayIO::InputBuffer_PostSim* lpReplayModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpReplayModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   CGS_ASSERT_MESSAGE_AT("lpReplayModuleInputBuffer && lpGuiOutputBuffer", CGS_ARTIST_FILE, 1721));

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, CGS_ASSERT_MESSAGE_AT("lpGuiEventQueue", CGS_ARTIST_FILE, 1726));

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            switch (liType)
            {
                case 349: case 525: case 526: case 527: case 528: case 529:
                case 530: case 531: case 532: case 533: case 594:
                {
                    BrnReplays::ReplayIO::InputBuffer_PostSim::GuiEventQueue* lpDestQueue =
                        lpReplayModuleInputBuffer->GetGuiEventQueue();
                    lpDestQueue->AddEvent(lpEvent, liType, liSize);
                    break;
                }
                case 595:
                {
                    const BrnGui::GuiReplayRegisterSerialiser* lpRegistration =
                        reinterpret_cast<const BrnGui::GuiReplayRegisterSerialiser*>(lpEvent);
                    BrnReplays::ReplayIO::RequestInterface* lpRequestInterface =
                        lpReplayModuleInputBuffer->GetRequestInterface();
                    lpRequestInterface->RegisterSerialiser(lpRegistration->mpSerialiser);
                    break;
                }
                default: break;
            }
            liType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // =========================================================================
    // TranslateGuiEventsToNetworkEvents
    //
    // Drain the GUI out-event queue and translate each network-bound GUI event into the
    // matching network IN-event, AddEvent'd into the network module's in-event queue
    // (VariableEventQueue<14000,16>). Called by DoUpdate_NetworkPostSim; `this` is unused.
    //
    // Leaves with a typed home (select-scoreboard, telemetry, camera-picture request) are
    // built by name and queued at sizeof(); the rest are copied as the console's byte spans.
    // The one-byte "signal" events queue an unwritten stack byte, as the console does.
    // FLAG: the 353 -> 24 word is, by the reference's naming, a LiveRevengeProfile pointer
    // (NetworkInLiveRevengeProfileLoaded); it stays a 4-byte copy until its GUI producer and
    // network consumer have host homes.
    //
    void BrnGameModule::TranslateGuiEventsToNetworkEvents(
        CgsModule::VariableEventQueue<14000, 16>* lpNetworkInputEventQueue,
        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        namespace IO = BrnNetwork::BrnNetworkModuleIO;

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventId = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent)
        {
            // ---- resolve (command, payload, payload size) -- the PC-ABI adapter above ----
            const u8* lp            = reinterpret_cast<const u8*>(lpEvent);
            s32       liCommand     = liEventId;
            s32       liPayloadSize = liEventSize;
            if (liEventId == 40 && liEventSize >= 12)
            {
                const u32* lpuRecord = reinterpret_cast<const u32*>(lpEvent);
                liCommand = static_cast<s32>(lpuRecord[1]);
                const u32 luOffset = lpuRecord[2];
                if (luOffset >= 12u && static_cast<s32>(luOffset) < liEventSize)
                {
                    lp            = reinterpret_cast<const u8*>(lpEvent) + luOffset;
                    liPayloadSize = static_cast<s32>(lpuRecord[0]);
                }
            }
            const s32* lpW = reinterpret_cast<const s32*>(lp);

            u8 lSignal;   // the one-byte signal events' payload (never written)

            switch (liCommand)
            {
                case 49:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 27, 1);
                    break;
                case 80:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 28, 1);
                    break;
                case 94:   // easy-drive opened: a parameterless telemetry event, only when the flag byte is set
                {
                    if (lp[0] == 0)
                        break;
                    IO::NetworkInTelemetryEvent lTelemetryEvent;
                    lTelemetryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EASY_DRIVE_OPENED);
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTelemetryEvent),
                                                       IO::NetworkInTelemetryEvent::KI_EVENT_TYPE,
                                                       sizeof(lTelemetryEvent));
                    break;
                }
                case 97:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 1, 1);
                    break;
                case 98:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 2, 1);
                    break;
                case 99:   // show profile: the friend's PlayerName
                {
                    CGS_ASSERT(lp != 0, "lpGuiProfileEvent");
                    CgsNetwork::PlayerName lShowProfile;
                    std::memcpy(&lShowProfile, lp, sizeof(lShowProfile));
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowProfile), 13,
                                                       sizeof(lShowProfile));
                    break;
                }
                case 100:  // invite action (payload word 0), the friend's PlayerName at +4
                    switch (lpW[0])
                    {
                        case 0:   // send
                        {
                            CgsNetwork::PlayerName lSendInvite;
                            std::memcpy(&lSendInvite, lp + 4, sizeof(lSendInvite));
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSendInvite), 8,
                                                               sizeof(lSendInvite));
                            break;
                        }
                        case 1:   // revoke: the name + the has-the-online-game-started byte at +20
                        {
                            u8 lacRevokeInvite[17];
                            std::memcpy(lacRevokeInvite, lp + 4, 16);
                            lacRevokeInvite[16] = lp[20];
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacRevokeInvite), 9,
                                                               sizeof(lacRevokeInvite));
                            break;
                        }
                        case 2:   // accept
                        {
                            CgsNetwork::PlayerName lAcceptInvite;
                            std::memcpy(&lAcceptInvite, lp + 4, sizeof(lAcceptInvite));
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAcceptInvite), 10,
                                                               sizeof(lAcceptInvite));
                            break;
                        }
                        case 3:   // decline
                        {
                            CgsNetwork::PlayerName lDeclineInvite;
                            std::memcpy(&lDeclineInvite, lp + 4, sizeof(lDeclineInvite));
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDeclineInvite), 11,
                                                               sizeof(lDeclineInvite));
                            break;
                        }
                        case 4:   // join
                        {
                            CgsNetwork::PlayerName lJoinBuddy;
                            std::memcpy(&lJoinBuddy, lp + 4, sizeof(lJoinBuddy));
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lJoinBuddy), 12,
                                                               sizeof(lJoinBuddy));
                            break;
                        }
                        case 5:   // send, to an empty name
                        {
                            CgsNetwork::PlayerName lSendInvite;
                            lSendInvite.Construct("");
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSendInvite), 8,
                                                               sizeof(lSendInvite));
                            break;
                        }
                        case 6:
                            lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 14, 1);
                            break;
                        default:
                        {
                            CgsDev::Assert::BeginAssert();
                            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStream << "Invalid invite action : " << lpW[0] << "\n";
                            CgsDev::Assert::FireAssert(lStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                            break;
                        }
                    }
                    break;
                case 110:  // scoreboard: list the categories
                {
                    IO::NetworkInSelectScoreboardEvent lScoreboardEvent;
                    lScoreboardEvent.Prepare();
                    lScoreboardEvent.GetCategories();
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                                                       IO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE,
                                                       sizeof(lScoreboardEvent));
                    break;
                }
                case 111:  // scoreboard: list the indexes of a category
                {
                    IO::NetworkInSelectScoreboardEvent lScoreboardEvent;
                    lScoreboardEvent.Prepare();
                    lScoreboardEvent.GetIndexes(lpW[0]);
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                                                       IO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE,
                                                       sizeof(lScoreboardEvent));
                    break;
                }
                case 112:  // scoreboard: list the variations of an index
                {
                    IO::NetworkInSelectScoreboardEvent lScoreboardEvent;
                    lScoreboardEvent.Prepare();
                    lScoreboardEvent.GetVariations(lpW[0]);
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                                                       IO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE,
                                                       sizeof(lScoreboardEvent));
                    break;
                }
                case 113:  // scoreboard: show a variation's table
                {
                    IO::NetworkInSelectScoreboardEvent lScoreboardEvent;
                    lScoreboardEvent.Prepare();
                    lScoreboardEvent.GetScoreboard(lpW[0]);
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                                                       IO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE,
                                                       sizeof(lScoreboardEvent));
                    break;
                }
                case 114:  // scoreboard: page up
                {
                    IO::NetworkInSelectScoreboardEvent lScoreboardEvent;
                    lScoreboardEvent.Prepare();
                    lScoreboardEvent.PageUp();
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                                                       IO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE,
                                                       sizeof(lScoreboardEvent));
                    break;
                }
                case 115:  // scoreboard: page down
                {
                    IO::NetworkInSelectScoreboardEvent lScoreboardEvent;
                    lScoreboardEvent.Prepare();
                    lScoreboardEvent.PageDown();
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lScoreboardEvent),
                                                       IO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE,
                                                       sizeof(lScoreboardEvent));
                    break;
                }
                case 120:  // scoreboard gamercard: the highlighted PlayerName
                {
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpGuiGamercardEvent", CGS_ARTIST_FILE, 1154));
                    CgsNetwork::PlayerName lShowGamerCard;
                    std::memcpy(&lShowGamerCard, lp, sizeof(lShowGamerCard));
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowGamerCard), 47,
                                                       sizeof(lShowGamerCard));
                    break;
                }
                case 121:  // scoreboard score target: payload {score, category, index, variation, name[16], flag}
                {          // -> {name[16], score, category, index, variation, flag}, 36 bytes
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpGuiReqScoreTargetEvent", CGS_ARTIST_FILE, 1169));
                    s32 laiScoreTarget[9];
                    u8* lpScoreTarget = reinterpret_cast<u8*>(laiScoreTarget);
                    std::memcpy(lpScoreTarget, lp + 16, 16);
                    laiScoreTarget[4] = lpW[0];
                    laiScoreTarget[5] = lpW[1];
                    laiScoreTarget[6] = lpW[2];
                    laiScoreTarget[7] = lpW[3];
                    lpScoreTarget[32] = lp[32];
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(laiScoreTarget), 50, 36);
                    break;
                }
                case 124:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 51, 1);
                    break;
                case 126:  // three flag bytes
                {
                    u8 lacAccountUpdate[3];
                    lacAccountUpdate[0] = lp[0];
                    lacAccountUpdate[1] = lp[1];
                    lacAccountUpdate[2] = lp[2];
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacAccountUpdate), 52,
                                                       sizeof(lacAccountUpdate));
                    break;
                }
                case 270:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 21, 1);
                    break;
                case 278:
                {
                    s32 liValue = lpW[0];
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liValue), 23, 4);
                    break;
                }
                case 281:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 25, 1);
                    break;
                case 286:  // custom route created: telemetry with the payload's second then first word
                {
                    IO::NetworkInTelemetryEvent lTelemetryEvent;
                    lTelemetryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_CUSTOM_ROUTE_CREATED);

                    char lacParameter0[16];
                    CgsCore::SPrintf(lacParameter0, 16, "%i", lpW[1]);
                    lTelemetryEvent.mEventData.AddParameter(lacParameter0);

                    char lacParameter1[16];
                    CgsCore::SPrintf(lacParameter1, 16, "%i", lpW[0]);
                    lTelemetryEvent.mEventData.AddParameter(lacParameter1);

                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTelemetryEvent),
                                                       IO::NetworkInTelemetryEvent::KI_EVENT_TYPE,
                                                       sizeof(lTelemetryEvent));
                    break;
                }
                case 287:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 6, 1);
                    break;
                case 293:
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignal), 26, 1);
                    break;
                case 323:  // a ready-made telemetry record
                {
                    IO::NetworkInTelemetryEvent lTelemetryEvent;
                    std::memcpy(&lTelemetryEvent.mEventData, lp, sizeof(lTelemetryEvent.mEventData));
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTelemetryEvent),
                                                       IO::NetworkInTelemetryEvent::KI_EVENT_TYPE,
                                                       sizeof(lTelemetryEvent));
                    break;
                }
                case 353:
                {
                    s32 liValue = lpW[0];
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liValue), 24, 4);
                    break;
                }
                case 474:
                {
                    s32 liValue = lpW[0];
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liValue), 17, 4);
                    break;
                }
                case 568:  // compressed camera-picture request: {quality, format, target texture}
                {
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpReqCompCamPicEvent", CGS_ARTIST_FILE, 1218));
                    IO::NetworkInReqCamPicEvent lReqCamPicEvent;
                    lReqCamPicEvent.miQualitySetting   = lpW[0];
                    lReqCamPicEvent.meCompressedFormat = static_cast<renderengine::PixelFormat>(lpW[1]);
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReqCamPicEvent),
                                                       IO::NetworkInReqCamPicEvent::KI_EVENT_TYPE,
                                                       sizeof(lReqCamPicEvent));
                    break;
                }
                default:
                    break;
            }

            liEventId = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
    }
}

// FOLDED FROM GameBridgeGUIToX_GameState.cpp
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h" // typed PostWorldInputBuffer owner
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress ("GMInvAccept" / "GMStrOffline")
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (the formatted default-case assert)
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"  // BrnGameState::TrainingManager (cases 94 / 473)
#include "SharedClasses/Progression/BrnTrainingTypes.h"               // BrnProgression::ETrainingType

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copies)

// ============================================================================================
// BrnGame::BrnGameModule::BridgeGuiToGameState  @ 0x823DDB78
//
// SIBLING SPLIT (2026-08-25, P1 sim-pause) -- the same pattern as
// GameBridgeGameStateToX_TrainingStringIds.cpp. This function was MOVED here (not copied)
// out of GameBridgeGUIToX.cpp so it could mount before that TU's other two members had
// their callees homed. Folding it back later is a delete, not a merge.
//
// BridgeGuiToGameState is the ONE producer of game event 93 (the crash-nav activate/
// deactivate pause event) and therefore the head of the sim-pause spine:
//   GUI 191 -> game event 93 -> RequestPause/RequestUnpause(reason 4) -> game actions 86/87
//   -> BrnGameModule::CheckGameActions -> mbSimPaused + the sim timer.
// ============================================================================================

namespace BrnGame
{
    // =========================================================================
    // BridgeGuiToGameState  (X360 0x823DDB78)
    //
    // Drain the GUI out-event queue and translate each recognised GUI event
    // into the matching game-state event, AddEvent'd into the game-state module's post-world
    // input GameEventQueue (VariableEventQueue<1536,16>, via GameStateModuleIO::PostWorldInput).
    // Two events (94 / 473) additionally drive the training manager; two (191 / 494)
    // build/emit their game-state event inline. Returns the final queue-walk status.
    // Called by DoUpdate_GameStatePostWorld / LoadingScriptedState::Update.
    // =========================================================================
    int BrnGameModule::BridgeGuiToGameState(
        BrnGameState::GameStateModuleIO::PostWorldInputBuffer* lpGameStateInput,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        const CgsModule::VariableEventQueue<18432, 16>* const lpGuiEventQueue =
            lpGuiOutput->GetOutEventQueue();
        CgsModule::VariableEventQueue<1536, 16>* const lpGameEventQueue =
            lpGameStateInput->GetGameEventQueue();
        BrnGameState::TrainingManager* const lpTrainingManager = mGameStateModule.GetTrainingManager();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        int liResult = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent)
        {
            const unsigned char* lp = reinterpret_cast<const unsigned char*>(lpEvent);
            const s32* lpW = reinterpret_cast<const s32*>(lp);

            CGS_ALIGN(16) unsigned char lBuf[512];
            unsigned char* lpOut = lBuf;
            s32 liType = 0;
            s32 liSize = 0;
            bool lbEmit = false;

            switch (liResult)
            {
                // ---- switch A (65..191) ----
                case 65:
                    lpOut[0] = lp[0];
                    liType = 106; liSize = 1; lbEmit = true;
                    break;
                case 77:
                    liType = 83; liSize = 1; lbEmit = true;   // signal (uninitialised payload byte)
                    break;
                case 78:
                    CGS_ASSERT(lp != 0, "lpGuiRequestCarUnlockEvent != NULL");
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 105; liSize = 8; lbEmit = true;
                    break;
                case 86:
                case 87:
                    liType = 110; liSize = 1; lbEmit = true;   // signal
                    break;
                case 92:
                    liType = 14; liSize = 1; lbEmit = true;    // signal
                    break;
                case 94:
                    // X360 reads a SINGLE BYTE lp[0] (lbz r11,0(r31); cmplwi r11,0) for both the
                    // emitted payload and the training gate -- not the 32-bit word (Hex-Rays _R31 is u8*).
                    lpOut[0] = (lp[0] == 0) ? 1 : 0;
                    // ARTIST 823DDFC8 reads the saved profile's first training word.
                    if (lp[0] != 0)
                    {
                        const BrnProgression::Profile* const lpProfile =
                            mGameStateModule.GetProgressionManager()->GetProfile();
                        if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_INTRO_TO_ONLINE_1))
                            lpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_ACTIVATED_EASY_DRIVE);
                    }
                    liType = 99; liSize = 1; lbEmit = true;
                    break;
                case 135:
                    reinterpret_cast<s32*>(lpOut)[0] = 3;
                    liType = 9; liSize = 16; lbEmit = true;
                    break;
                case 145:
                    liType = 78; liSize = 1; lbEmit = true;    // signal
                    break;
                case 146:
                    liType = 27; liSize = 1; lbEmit = true;    // signal
                    break;
                case 161:
                    liType = 24; liSize = 1; lbEmit = true;    // signal
                    break;
                case 163:
                    liType = 25; liSize = 1; lbEmit = true;    // signal
                    break;
                case 167:
                    // 80-byte repack: [0..39]=src[0..39], [40..43]=0, [44..75]=src[40..71], [76]=src[72].
                    std::memcpy(lpOut, lp, 40);
                    reinterpret_cast<s32*>(lpOut + 40)[0] = 0;
                    std::memcpy(lpOut + 44, lp + 40, 32);
                    lpOut[76] = lp[72];
                    liType = 20; liSize = 80; lbEmit = true;
                    break;
                case 172:
                    std::memcpy(lpOut, lp, 120);
                    liType = 90; liSize = 120; lbEmit = true;
                    break;
                case 174:
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp + 32);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                    liType = 92; liSize = 8; lbEmit = true;
                    break;
                case 189:
                    CGS_ASSERT(lp != 0, "lpCompleteEvent");
                    if (*reinterpret_cast<const u64*>(lp) == CgsIDCompress("GMInvAccept") && lpW[2] == 1)
                    {
                        liType = 58; liSize = 1; lbEmit = true;   // signal
                    }
                    else if (*reinterpret_cast<const u64*>(lp) == CgsIDCompress("GMStrOffline"))
                    {
                        lpOut[0] = (lpW[2] == 1) ? 1 : 0;
                        liType = 56; liSize = 1; lbEmit = true;
                    }
                    break;
                case 191:
                    if (lpW[0] == 0 && lpW[1] == 1)
                    {
                        unsigned char lSignal;   // 1 uninitialised payload byte (matches &v48)
                        lpGameEventQueue->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lSignal), 15, 1);
                    }
                    lpOut[0] = (lpW[0] == 0) ? 1 : 0;
                    liType = 93; liSize = 1; lbEmit = true;
                    break;

                // ---- result == 192 ----
                case 192:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lpOut)[1] = lpW[1];
                    liType = 94; liSize = 8; lbEmit = true;
                    break;

                // ---- switch B (193..327) ----
                case 193:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 28; liSize = 4; lbEmit = true;
                    break;
                case 195:
                    std::memcpy(lpOut, lp, 16);
                    liType = 95; liSize = 16; lbEmit = true;
                    break;
                case 197:
                    *reinterpret_cast<u16*>(lpOut) = *reinterpret_cast<const u16*>(lp);
                    liType = 89; liSize = 2; lbEmit = true;
                    break;
                case 231:
                    lpOut[0] = 0; lpOut[1] = 0; lpOut[2] = 1;
                    liType = 32; liSize = 3; lbEmit = true;
                    break;
                case 232:
                    CGS_ASSERT(*reinterpret_cast<const u16*>(lp + 32) != 0x7FFF,   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                               "lEvent.muTargetSectionId != BrnWorld::KI_INVALID_SECTION_INDEX");
                    std::memcpy(lpOut, lp + 16, 16);
                    *reinterpret_cast<u16*>(lpOut + 16) = *reinterpret_cast<const u16*>(lp + 32);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                    liType = 116; liSize = 32; lbEmit = true;
                    break;
                case 268:
                    liType = 29; liSize = 1; lbEmit = true;    // signal
                    break;
                case 269:
                    if (lpW[0] != 2)
                        break;
                    liType = 23; liSize = 1; lbEmit = true;    // signal
                    break;
                case 282:
                    liType = 21; liSize = 1; lbEmit = true;    // signal
                    break;
                case 286:
                    liType = 148; liSize = 1; lbEmit = true;   // signal
                    break;
                case 292:
                    liType = 26; liSize = 1; lbEmit = true;    // signal
                    break;
                case 293:
                    liType = 145; liSize = 1; lbEmit = true;   // signal
                    break;
                case 295:
                    liType = 149; liSize = 1; lbEmit = true;   // signal
                    break;
                case 308:
                    liType = 114; liSize = 1; lbEmit = true;   // signal
                    break;
                case 324:
                    liType = 137; liSize = 1; lbEmit = true;   // signal
                    break;
                case 326:
                    if (lp[4] != 0)
                        reinterpret_cast<s32*>(lpOut)[0] = (lpW[0] != 0) ? 2 : 1;
                    else
                        reinterpret_cast<s32*>(lpOut)[0] = 0;
                    liType = 103; liSize = 4; lbEmit = true;
                    break;
                case 327:
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 96; liSize = 8; lbEmit = true;
                    break;

                // ---- result == 328 ----
                case 328:
                    liType = 98; liSize = 1; lbEmit = true;    // signal
                    break;

                // ---- switch C (329..580) ----
                case 329:
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpScoreRequestEvent", CGS_ARTIST_FILE, 655));
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 97; liSize = 4; lbEmit = true;
                    break;
                case 330:
                    lpOut[0] = 0;
                    liType = 100; liSize = 1; lbEmit = true;
                    break;
                case 331:
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpRequest", CGS_ARTIST_FILE, 562));
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 104; liSize = 8; lbEmit = true;
                    break;
                case 352:
                    // The loaded profile's pointer (console size 4); the host copies all of it.
                    std::memcpy(lpOut, lp, sizeof(const void*));
                    liType = 109; liSize = static_cast<s32>(sizeof(const void*)); lbEmit = true;
                    break;
                case 354:
                    liType = 110; liSize = 1; lbEmit = true;   // signal
                    break;
                case 357:
                {
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpAutosaveCompletedEvent", CGS_ARTIST_FILE, 670));
                    const s32 liCount = lpW[0];
                    if (liCount <= 0)
                        break;
                    reinterpret_cast<s32*>(lpOut)[0] = liCount;
                    std::memcpy(lpOut + 4, lp + 4, 48 * static_cast<size_t>(liCount));
                    liType = 157; liSize = 52; lbEmit = true;
                    break;
                }
                case 360:
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpImagesLoadedEvent", CGS_ARTIST_FILE, 695));
                    std::memcpy(lpOut, lp, 48);
                    liType = 161; liSize = 48; lbEmit = true;
                    break;
                case 370:
                    liType = 34; liSize = 1; lbEmit = true;    // signal
                    break;
                case 405:
                    liType = 81; liSize = 1; lbEmit = true;    // signal
                    break;
                case 407:
                    liType = 86; liSize = 1; lbEmit = true;    // signal
                    break;
                case 408:
                    liType = 87; liSize = 1; lbEmit = true;    // signal
                    break;
                case 409:
                    lpOut[0] = lp[0]; lpOut[1] = 0;
                    liType = 88; liSize = 2; lbEmit = true;
                    break;
                case 410:
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 82; liSize = 8; lbEmit = true;
                    break;
                case 411:
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 6; liSize = 8; lbEmit = true;
                    break;
                case 415:
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 4; liSize = 16; lbEmit = true;
                    break;
                case 416:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lpOut)[1] = lpW[1];
                    liType = 5; liSize = 8; lbEmit = true;
                    break;
                case 435:
                    liType = 79; liSize = 1; lbEmit = true;    // signal
                    break;
                case 437:
                    liType = 80; liSize = 1; lbEmit = true;    // signal
                    break;
                case 441:
                    liType = 101; liSize = 1; lbEmit = true;   // signal
                    break;
                case 443:
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 102; liSize = 8; lbEmit = true;
                    break;
                case 469:
                    liType = (lp[0] == 1) ? 107 : 108; liSize = 1; lbEmit = true;   // signal
                    break;
                case 470:
                    liType = (lp[0] == 1) ? 107 : 108; liSize = 1; lbEmit = true;   // signal
                    break;
                case 472:
                    lpOut[0] = lp[0]; lpOut[1] = lp[1]; lpOut[2] = lp[2];
                    liType = 11; liSize = 3; lbEmit = true;
                    break;
                case 473:
                    lpTrainingManager->OnEnableTrainingTips(lp[0] != 0);
                    break;   // no emit
                case 494:
                {
                    // 80-byte structured repack of the two per-lane sub-records (event type 84).
                    for (s32 k = 0; k < 2; ++k)
                    {
                        std::memcpy(lpOut + k * 16, lp + k * 16, 16);
                        const s32 liAction = *reinterpret_cast<const s32*>(lp + 48 + k * 4);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                        if (liAction == 0)
                            reinterpret_cast<s32*>(lpOut + 32)[k] = 0;
                        else if (liAction == 1 || liAction == 2)
                            reinterpret_cast<s32*>(lpOut + 32)[k] = liAction;
                        reinterpret_cast<s32*>(lpOut + 40)[k] =
                            *reinterpret_cast<const s32*>(lp + 56 + k * 4);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                        reinterpret_cast<u64*>(lpOut + 48)[k] =
                            *reinterpret_cast<const u64*>(lp + 32 + k * 8);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                        reinterpret_cast<u16*>(lpOut + 64)[k] =
                            *reinterpret_cast<const u16*>(lp + 64 + k * 2);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                    }
                    *reinterpret_cast<u16*>(lpOut + 68) = *reinterpret_cast<const u16*>(lp + 68);   // FLAG opaque serialized event payload: asm-attested byte offset, no reconstructed type
                    lpGameEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lpOut), 84, 80);
                    break;   // no standard emit
                }
                case 517:
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpGuiImageGalleryRequest", CGS_ARTIST_FILE, 607));
                    std::memcpy(lpOut, lp, 16);
                    liType = 158; liSize = 16; lbEmit = true;
                    break;
                case 519:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 159; liSize = 4; lbEmit = true;
                    break;
                case 521:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 160; liSize = 4; lbEmit = true;
                    break;
                case 555:
                    liType = 77; liSize = 1; lbEmit = true;    // signal
                    break;
                case 572:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 113; liSize = 4; lbEmit = true;
                    break;
                case 573:
                {
                    CGS_ASSERT(lp != 0, CGS_ASSERT_MESSAGE_AT("lpChallengeSelectedEvent", CGS_ARTIST_FILE, 727));
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    reinterpret_cast<s32*>(lpOut)[3] = lpW[3];
                    const s32 liSelector = lpW[2];
                    switch (liSelector)
                    {
                        case 0: reinterpret_cast<s32*>(lpOut)[2] = 0; break;
                        case 1: reinterpret_cast<s32*>(lpOut)[2] = 1; break;
                        case 2: reinterpret_cast<s32*>(lpOut)[2] = 2; break;
                        case 3: reinterpret_cast<s32*>(lpOut)[2] = 3; break;
                        default:
                        {
                            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStream << "Unknown freeburn challenge selector action " << lpW[2];
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert(lStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                            break;
                        }
                    }
                    liType = 162; liSize = 16; lbEmit = true;
                    break;
                }
                case 580:
                    liType = 170; liSize = 1; lbEmit = true;   // signal
                    break;

                default:
                    break;
            }

            if (lbEmit)
                lpGameEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lpOut), liType, liSize);

            liResult = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        return liResult;
    }

}

// FOLDED FROM GameBridgeGUIToX_Sound.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGUIToX_Sound.cpp
//
// SIBLING SPLIT of GameBridgeGUIToX.cpp (the established precedent: that parent
// TU cannot be mounted -- its two event-translating members reference six symbols
// with no home in the linked set, one of which, TelemetryData::AddParameter, has
// no reconstructed home anywhere; see the parent banner and the build bat's
// GameBridgeGUIToX_GameState.cpp note). BridgeGuiToSound gained its caller with
// the faithful-audio-engine phase C4 spine wiring (LoadingScriptedState::Update
// drives it each frame @0x823F2A2C), so it is MOVED -- not copied -- here so it
// can be mounted on its own. Folding it back later is a delete.
//
// Bodied here (1 ledger function):
//   BrnGame::BrnGameModule::BridgeGuiToSound @ 0x823C0A58  (DWARF home
//   GameSource/Game/GameBridgeGUIToX.cpp)
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"                 // typed GetGuiOutEventQueue bridge
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"     // RootInputBuffer + GuiEventQueue

namespace BrnGame
{
    // @ 0x823C0A58 -- hand the GUI output buffer's out-event queue to the sound root input
    // buffer (SetGuiEventQueue) so the sound module can drain GUI events.
    void BrnGameModule::BridgeGuiToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpSoundModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   CGS_ASSERT_MESSAGE_AT("lpSoundModuleInputBuffer && lpGuiOutputBuffer", CGS_ARTIST_FILE, 1694));

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, CGS_ASSERT_MESSAGE_AT("lpGuiEventQueue", CGS_ARTIST_FILE, 1699));
        lpSoundModuleInputBuffer->SetGuiEventQueue(
            reinterpret_cast<const BrnSound::Module::Io::RootInputBuffer::GuiEventQueue*>(
                lpGuiEventQueue));
    }
}

// FOLDED FROM GameBridgeGUIToX_wBT_01.cpp
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                    // CgsGuiModuleIO::OutputBuffer (out events + game actions)
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"            // CgsGui::ModelIO::OutputBuffer::GetGuiResourceRequestQueue
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"      // EventReceiverQueue<256,16> -> BaseEventReceiverQueue
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::InputBuffer
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"         // RequestInterface<256>
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueueImpl.h"     // RequestInterface<N>::GetVehicleList / GetWheelList bodies
#include "GameSource/World/BrnWorldModuleIO.h"                            // BrnWorldIO::UpdateInputBuffer
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventNetworkVehicleListRequest / GuiCarSelectWheelRequest
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventRequestCollisionWorldEvent

// GameBridgeGUIToX partfile (blocked-TU wave): the two GUI-output bridges that feed the
// GameData module and the world, beside their home GameBridgeGUIToX.cpp.

namespace BrnGame
{
    // GameBridgeGUIToX.cpp. Forward the GUI model's resource requests into the GameData
    // input, then translate the GUI's vehicle/wheel list requests into GameData list requests
    // built in a local RequestInterface<256> and appended in one block at the end.
    //   263 GuiEventNetworkVehicleListRequest -> GetVehicleList(id 0) + GetWheelList(id 1)
    //   417 GuiCarSelectWheelRequest          -> GetWheelList(id 1)
    // Both reply into the requesting screen's receiver queue.
    void BrnGameModule::BridgeGuiToResource(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                                            const CgsGui::ModelIO::OutputBuffer* lpModelOutput,
                                            const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        static const s32 KI_VEHICLE_LIST_EVENT_ID = 0;
        static const s32 KI_WHEEL_LIST_EVENT_ID   = 1;

        lpGDMInput->GetRequestInterface()->mRequestQueue.Append(*lpModelOutput->GetGuiResourceRequestQueue());

        const CgsGui::CgsGuiModuleIO::OutputBuffer::GuiEventQueue* lpGuiEventQueue = lpGuiOutput->GetOutEventQueue();
        BrnResource::GameDataIO::RequestInterface<256> lRequestInterface;
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
        lRequestInterface.Construct();

        while (lpEvent != 0)
        {
            if (liEventType == 263)
            {
                const BrnGui::GuiEventNetworkVehicleListRequest* lpRequest =
                    reinterpret_cast<const BrnGui::GuiEventNetworkVehicleListRequest*>(lpEvent);
                lRequestInterface.GetVehicleList(lpRequest->mpReceiverQueue, KI_VEHICLE_LIST_EVENT_ID);
                lRequestInterface.GetWheelList(lpRequest->mpReceiverQueue, KI_WHEEL_LIST_EVENT_ID);
            }
            else if (liEventType == 417)
            {
                const BrnGui::GuiCarSelectWheelRequest* lpRequest =
                    reinterpret_cast<const BrnGui::GuiCarSelectWheelRequest*>(lpEvent);
                lRequestInterface.GetWheelList(lpRequest->mpReceiverQueue, KI_WHEEL_LIST_EVENT_ID);
            }
            liEventType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        lpGDMInput->AppendRequestInterface(&lRequestInterface);
    }

    // GameBridgeGUIToX.cpp. Turn the GUI's collision-world requests (493) into the
    // world-entity request flags, then hand the GUI's game actions to the world input.
    // The request kind is compared unsigned: 0 invalidates, 1 validates, anything else
    // trips the (non-gating) assert.
    void BrnGameModule::BridgeGuiToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                         const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        typedef BrnGui::GuiEventRequestCollisionWorldEvent CollisionWorldEvent;

        const CgsGui::CgsGuiModuleIO::OutputBuffer::GuiEventQueue* lpGuiEventQueue = lpGuiOutputBuffer->GetOutEventQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != 0)
        {
            if (liEventType == 493)
            {
                const CollisionWorldEvent* lpCollisionWorldEvent = reinterpret_cast<const CollisionWorldEvent*>(lpEvent);
                const u32 luRequest = static_cast<u32>(lpCollisionWorldEvent->meEventType);
                if (luRequest == CollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE)
                    lpWorldInput->GetWorldEntityRequestInterface()->InvalidateCollisionWorld();
                else if (luRequest == CollisionWorldEvent::E_COLLISON_WORLD_VALIDATE)
                    lpWorldInput->GetWorldEntityRequestInterface()->ValidateCollisionWorld();
                else
                    CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("Invalid collsion world request event type", CGS_ARTIST_FILE, 1409));
            }
            liEventType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        lpWorldInput->GetGameActionQueue()->Append(*lpGuiOutputBuffer->GetGameActionQueue());
    }
}

#undef CGS_ARTIST_FILE
