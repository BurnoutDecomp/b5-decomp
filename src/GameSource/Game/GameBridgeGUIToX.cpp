#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf (telemetry "%i")
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (the invite-action assert)
#include "GameSource/GameState/BrnCgsPlayerName.h"                    // CgsNetwork::PlayerName
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"           // TelemetryData, ETelemetryHook
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"             // the typed network IN-event leaves

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
                   "lpReplayModuleInputBuffer && lpGuiOutputBuffer");

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, "lpGuiEventQueue");

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
                    BrnReplays::BaseSerialiser* lpSerialiser =
                        *reinterpret_cast<BrnReplays::BaseSerialiser* const*>(lpEvent);
                    BrnReplays::ReplayIO::RequestInterface* lpRequestInterface =
                        lpReplayModuleInputBuffer->GetRequestInterface();
                    lpRequestInterface->RegisterSerialiser(lpSerialiser);
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
    // FLAG PC-ABI adapter (the BridgeGuiToGameState / BridgeGuiToDirector one): the PC GUI
    // module leaves records on their channel (40) with the event type in the record's second
    // word, the payload size in its first and the payload offset in its third -- the three
    // words GuiEventWrapper::GetRawEvent() unwraps on the console. A console-style queue
    // (type = event id, pointer = payload) passes through unchanged.
    //
    // FLAG return type: the reference declares this void and the only caller never reads the
    // result; the int return (the final GetNextEvent status) follows the BrnGameModule.hpp
    // declaration until that header changes.
    // =========================================================================
    int BrnGameModule::TranslateGuiEventsToNetworkEvents(
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
                    CGS_ASSERT(lp != 0, "lpGuiGamercardEvent");
                    CgsNetwork::PlayerName lShowGamerCard;
                    std::memcpy(&lShowGamerCard, lp, sizeof(lShowGamerCard));
                    lpNetworkInputEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowGamerCard), 47,
                                                       sizeof(lShowGamerCard));
                    break;
                }
                case 121:  // scoreboard score target: payload {score, category, index, variation, name[16], flag}
                {          // -> {name[16], score, category, index, variation, flag}, 36 bytes
                    CGS_ASSERT(lp != 0, "lpGuiReqScoreTargetEvent");
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
                    CGS_ASSERT(lp != 0, "lpReqCompCamPicEvent");
                    IO::NetworkInReqCamPicEvent lReqCamPicEvent;
                    lReqCamPicEvent.miQualitySetting   = lpW[0];
                    lReqCamPicEvent.meCompressedFormat = static_cast<renderengine::PixelFormat>(lpW[1]);
                    // FLAG PC-ABI: the texture pointer is the payload's last member, so it is read
                    // from the payload's end -- +0x08 in the console's 12-byte payload; the host
                    // producer's pointer alignment puts it after padding.
                    std::memcpy(&lReqCamPicEvent.mpTextureToCompressedInto,
                                lp + liPayloadSize - sizeof(lReqCamPicEvent.mpTextureToCompressedInto),
                                sizeof(lReqCamPicEvent.mpTextureToCompressedInto));
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

        return liEventId;
    }
}
