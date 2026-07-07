#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress ("GMInvAccept" / "GMStrOffline")
#include "GameShared/GameClasses/Core/CgsStringUtils.h"               // CgsCore::SPrintf (telemetry "%i")
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (formatted default-case asserts)
#include "GameSource/GameState/BrnCgsPlayerName.h"                    // CgsNetwork::PlayerName
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"  // BrnGameState::TrainingManager
#include "SharedClasses/Progression/BrnTrainingTypes.h"              // BrnProgression::ETrainingType
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"            // BrnNetwork::...::NetworkInSelectScoreboardEvent

#include <cstring>   // std::memcpy / std::memset (models the Xbox XMemCpy / memcpy block-copies)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGame::BrnGameModule::BridgeGuiToReplay_PostSim      @ 0x823CCA00
//   BrnGame::BrnGameModule::BridgeGuiToSound               @ 0x823C0A58
//   BrnGame::BrnGameModule::BridgeGuiToGameState           @ 0x823DDB78
//   BrnGame::BrnGameModule::TranslateGuiEventsToNetworkEvents @ 0x823DEEB8
//
// GUI-output bridge family: each per-frame bridge walks the GUI output buffer's out-event
// queue (VariableEventQueue<18432,16> @ +0x814, via GetGuiOutEventQueue) and re-publishes /
// translates the queued GUI events into a downstream subsystem's INPUT buffer.
//
// The two event-translating members (BridgeGuiToGameState / TranslateGuiEventsToNetworkEvents)
// drain the GUI queue and, per recognised GUI event type, build a downstream event into a
// correctly-sized RAW local buffer (the exact stack span the X360 writes) and AddEvent it with
// the attested integer event-type TAG + byte size. This mirrors the committed
// GameBridgeNetworkToX.cpp translators. Event-type tags + sizes are asm-authoritative; the
// downstream event payload layouts are opaque (no field names invented). One-byte "signal"
// events carry an uninitialised payload byte, exactly as the X360 passes a pointer to
// uninitialised stack.

// ---------------------------------------------------------------------------
// FLAG (by-name, un-homed collaborators). Declared here so this TU compiles/links against the
// real X360 symbols; their homes are the not-yet-reconstructed GameState / Network module-IO TUs.
// ---------------------------------------------------------------------------
namespace BrnGameState
{
    namespace GameStateModuleIO
    {
        // X360 free function: returns the module's post-world input GameEventQueue
        // (VariableEventQueue<1536,16>) that BridgeGuiToGameState AddEvent's translated events into.
        CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule);
    }
}

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // X360 BrnNetwork::BrnNetworkModuleIO::TelemetryData::AddParameter -- appends one
        // (SPrintf'd) parameter string to the telemetry record the case-286 GUI event builds.
        // The record layout + the AddParameter body are owned by the network telemetry TU;
        // modelled here as an opaque record with the attested member (no fields invented).
        struct TelemetryData
        {
            void AddParameter(const char* lpcParameter);
        };
    }
}

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

    // @ 0x823C0A58 -- hand the GUI output buffer's out-event queue to the sound root input
    // buffer (SetGuiEventQueue) so the sound module can drain GUI events.
    void BrnGameModule::BridgeGuiToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpSoundModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   "lpSoundModuleInputBuffer && lpGuiOutputBuffer");

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, "lpGuiEventQueue");

        lpSoundModuleInputBuffer->SetGuiEventQueue(
            reinterpret_cast<const BrnSound::Module::Io::RootInputBuffer::GuiEventQueue*>(
                lpGuiEventQueue));
    }

    // =========================================================================
    // BridgeGuiToGameState  (X360 0x823DDB78)
    //
    // Drain the GUI output buffer's out-event queue and translate each recognised GUI event
    // into the matching game-state event, AddEvent'd into the game-state module's post-world
    // input GameEventQueue (VariableEventQueue<1536,16>, via GameStateModuleIO::PostWorldInput).
    // Two events (94 / 473) additionally drive the training manager embedded at this+0x674B30;
    // two (191 / 494) build/emit their game-state event inline. Returns the final queue-walk
    // status. Called by DoUpdate_GameStatePostWorld / LoadingScriptedState::Update.
    // =========================================================================
    int BrnGameModule::BridgeGuiToGameState(
        BrnGameState::GameStateModule* lpGameStateInput,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue =
            BrnGameState::GameStateModuleIO::PostWorldInput(lpGameStateInput);

        // FLAG: the embedded training manager (this + 0x674B30) + the 64-bit game-flags word
        // (this + 0x691E60, bit 41 tested by case 94). Reached by their committed byte offsets.
        BrnGameState::TrainingManager* lpTrainingManager =
            reinterpret_cast<BrnGameState::TrainingManager*>(
                reinterpret_cast<unsigned char*>(this) + 6769456);
        const u64* lpGameFlags =
            reinterpret_cast<const u64*>(reinterpret_cast<const unsigned char*>(this) + 6889060);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        int liResult = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent)
        {
            const unsigned char* lp = reinterpret_cast<const unsigned char*>(lpEvent);
            const s32* lpW = reinterpret_cast<const s32*>(lp);

            alignas(16) unsigned char lBuf[512];
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
                    if (lp[0] != 0 && (*lpGameFlags & (1ULL << 41)) == 0)
                        lpTrainingManager->RequestTraining(static_cast<BrnProgression::ETrainingType>(33));
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
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp + 32);
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
                    CGS_ASSERT(*reinterpret_cast<const u16*>(lp + 32) != 0x7FFF,
                               "lEvent.muTargetSectionId != BrnWorld::KI_INVALID_SECTION_INDEX");
                    std::memcpy(lpOut, lp + 16, 16);
                    *reinterpret_cast<u16*>(lpOut + 16) = *reinterpret_cast<const u16*>(lp + 32);
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
                    CGS_ASSERT(lp != 0, "lpScoreRequestEvent");
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 97; liSize = 4; lbEmit = true;
                    break;
                case 330:
                    lpOut[0] = 0;
                    liType = 100; liSize = 1; lbEmit = true;
                    break;
                case 331:
                    CGS_ASSERT(lp != 0, "lpRequest");
                    *reinterpret_cast<u64*>(lpOut) = *reinterpret_cast<const u64*>(lp);
                    liType = 104; liSize = 8; lbEmit = true;
                    break;
                case 352:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 109; liSize = 4; lbEmit = true;
                    break;
                case 354:
                    liType = 110; liSize = 1; lbEmit = true;   // signal
                    break;
                case 357:
                {
                    CGS_ASSERT(lp != 0, "lpAutosaveCompletedEvent");
                    const s32 liCount = lpW[0];
                    if (liCount <= 0)
                        break;
                    reinterpret_cast<s32*>(lpOut)[0] = liCount;
                    std::memcpy(lpOut + 4, lp + 4, 48 * static_cast<size_t>(liCount));
                    liType = 157; liSize = 52; lbEmit = true;
                    break;
                }
                case 360:
                    CGS_ASSERT(lp != 0, "lpImagesLoadedEvent");
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
                        const s32 liAction = *reinterpret_cast<const s32*>(lp + 48 + k * 4);
                        if (liAction == 0)
                            reinterpret_cast<s32*>(lpOut + 32)[k] = 0;
                        else if (liAction == 1 || liAction == 2)
                            reinterpret_cast<s32*>(lpOut + 32)[k] = liAction;
                        reinterpret_cast<s32*>(lpOut + 40)[k] =
                            *reinterpret_cast<const s32*>(lp + 56 + k * 4);
                        reinterpret_cast<u64*>(lpOut + 48)[k] =
                            *reinterpret_cast<const u64*>(lp + 32 + k * 8);
                        reinterpret_cast<u16*>(lpOut + 64)[k] =
                            *reinterpret_cast<const u16*>(lp + 64 + k * 2);
                    }
                    *reinterpret_cast<u16*>(lpOut + 68) = *reinterpret_cast<const u16*>(lp + 68);
                    lpGameEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lpOut), 84, 80);
                    break;   // no standard emit
                }
                case 517:
                    CGS_ASSERT(lp != 0, "lpGuiImageGalleryRequest");
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
                    CGS_ASSERT(lp != 0, "lpChallengeSelectedEvent");
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

    // =========================================================================
    // TranslateGuiEventsToNetworkEvents  (X360 0x823DEEB8)
    //
    // Drain a GUI event queue (VariableEventQueue<18432,16>) and translate each network-bound
    // GUI event into the matching network IN-event, AddEvent'd into the network module's in-event
    // queue (VariableEventQueue<14000,16>). Returns the final queue-walk status. Called by
    // DoUpdate_NetworkPostSim. (The X360 method receives + uses only the two queue pointers; the
    // implicit `this` is unreferenced.)
    // =========================================================================
    int BrnGameModule::TranslateGuiEventsToNetworkEvents(
        CgsModule::VariableEventQueue<14000, 16>* lpNetworkInputQueue,
        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        int liResult = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent)
        {
            const unsigned char* lp = reinterpret_cast<const unsigned char*>(lpEvent);
            const s32* lpW = reinterpret_cast<const s32*>(lp);

            alignas(16) unsigned char lBuf[64];
            unsigned char* lpOut = lBuf;
            s32 liType = 0;
            s32 liSize = 0;
            bool lbEmit = false;

            switch (liResult)
            {
                case 120:   // gamercard
                    CGS_ASSERT(lp != 0, "lpGuiGamercardEvent");
                    std::memcpy(lpOut, lp, 16);
                    liType = 47; liSize = 16; lbEmit = true;
                    break;

                case 49:
                    liType = 27; liSize = 1; lbEmit = true;    // signal
                    break;
                case 80:
                    liType = 28; liSize = 1; lbEmit = true;    // signal
                    break;
                case 94:
                    if (lp[0] == 0)
                        break;
                    reinterpret_cast<s32*>(lpOut)[0] = 45;
                    lpOut[4] = 0;
                    liType = 16; liSize = 20; lbEmit = true;
                    break;
                case 97:
                    liType = 1; liSize = 1; lbEmit = true;     // signal
                    break;
                case 98:
                    liType = 2; liSize = 1; lbEmit = true;     // signal
                    break;
                case 99:
                    CGS_ASSERT(lp != 0, "lpGuiProfileEvent");
                    std::memcpy(lpOut, lp, 16);
                    liType = 13; liSize = 16; lbEmit = true;
                    break;
                case 100:   // invite action sub-switch on src word 0
                    switch (lpW[0])
                    {
                        case 0:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 8; liSize = 16; lbEmit = true;
                            break;
                        case 1:
                            std::memcpy(lpOut, lp + 4, 16);
                            lpOut[16] = lp[20];
                            liType = 9; liSize = 17; lbEmit = true;
                            break;
                        case 2:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 10; liSize = 16; lbEmit = true;
                            break;
                        case 3:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 11; liSize = 16; lbEmit = true;
                            break;
                        case 4:
                            std::memcpy(lpOut, lp + 4, 16);
                            liType = 12; liSize = 16; lbEmit = true;
                            break;
                        case 5:
                            reinterpret_cast<CgsNetwork::PlayerName*>(lpOut)->Construct("");
                            liType = 8; liSize = 16; lbEmit = true;
                            break;
                        case 6:
                            liType = 14; liSize = 1; lbEmit = true;   // signal
                            break;
                        default:
                        {
                            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStream << "Invalid invite action : " << lpW[0] << "\n";
                            CgsDev::Assert::BeginAssert();
                            CgsDev::Assert::FireAssert(lStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                            break;
                        }
                    }
                    break;
                case 110:   // select-scoreboard: fixed selector 1
                    std::memset(lpOut, 0xFF, 12);
                    reinterpret_cast<s32*>(lpOut)[3] = 1;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                case 111:   // select indexes (category = src word 0)
                {
                    std::memset(lpOut, 0xFF, 16);
                    BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lSel =
                        BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetIndexes(lpW[0]);
                    reinterpret_cast<s32*>(lpOut)[0] = lSel.miCategory;
                    reinterpret_cast<s32*>(lpOut)[3] = lSel.meSelectType;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                }
                case 112:   // select variations (index = src word 0)
                {
                    std::memset(lpOut, 0xFF, 16);
                    BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lSel =
                        BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetVariations(lpW[0]);
                    reinterpret_cast<s32*>(lpOut)[1] = lSel.miIndex;
                    reinterpret_cast<s32*>(lpOut)[3] = lSel.meSelectType;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                }
                case 113:   // show scoreboard (variation = src word 0)
                {
                    std::memset(lpOut, 0xFF, 16);
                    BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent lSel =
                        BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetScoreboard(lpW[0]);
                    reinterpret_cast<s32*>(lpOut)[2] = lSel.miVariation;
                    reinterpret_cast<s32*>(lpOut)[3] = lSel.meSelectType;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                }
                case 114:   // select-scoreboard: fixed selector 5
                    std::memset(lpOut, 0xFF, 12);
                    reinterpret_cast<s32*>(lpOut)[3] = 5;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;
                case 115:   // select-scoreboard: fixed selector 6
                    std::memset(lpOut, 0xFF, 12);
                    reinterpret_cast<s32*>(lpOut)[3] = 6;
                    liType = 49; liSize = 16; lbEmit = true;
                    break;

                case 121:   // req score target (36-byte record)
                    CGS_ASSERT(lp != 0, "lpGuiReqScoreTargetEvent");
                    std::memcpy(lpOut, lp + 16, 16);
                    reinterpret_cast<s32*>(lpOut)[4] = lpW[0];
                    reinterpret_cast<s32*>(lpOut)[5] = lpW[1];
                    reinterpret_cast<s32*>(lpOut)[6] = lpW[2];
                    reinterpret_cast<s32*>(lpOut)[7] = lpW[3];
                    lpOut[32] = lp[32];
                    liType = 50; liSize = 36; lbEmit = true;
                    break;
                case 124:
                    liType = 51; liSize = 1; lbEmit = true;    // signal
                    break;
                case 126:
                    lpOut[0] = lp[0]; lpOut[1] = lp[1]; lpOut[2] = lp[2];
                    liType = 52; liSize = 3; lbEmit = true;
                    break;
                case 270:
                    liType = 21; liSize = 1; lbEmit = true;    // signal
                    break;
                case 278:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 23; liSize = 4; lbEmit = true;
                    break;
                case 281:
                    liType = 25; liSize = 1; lbEmit = true;    // signal
                    break;

                case 286:   // telemetry (two "%i" parameters)
                {
                    reinterpret_cast<s32*>(lpOut)[0] = 44;
                    lpOut[4] = 0;
                    BrnNetwork::BrnNetworkModuleIO::TelemetryData* lpTelemetry =
                        reinterpret_cast<BrnNetwork::BrnNetworkModuleIO::TelemetryData*>(lpOut);

                    char lacParam0[16];
                    CgsCore::SPrintf(lacParam0, 16, "%i", lpW[1]);
                    lpTelemetry->AddParameter(lacParam0);

                    char lacParam1[16];
                    CgsCore::SPrintf(lacParam1, 16, "%i", lpW[0]);
                    lpTelemetry->AddParameter(lacParam1);

                    liType = 16; liSize = 20; lbEmit = true;
                    break;
                }

                case 287:
                    liType = 6; liSize = 1; lbEmit = true;     // signal
                    break;
                case 293:
                    liType = 26; liSize = 1; lbEmit = true;    // signal
                    break;
                case 323:
                    std::memcpy(lpOut, lp, 20);
                    liType = 16; liSize = 20; lbEmit = true;
                    break;
                case 353:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 24; liSize = 4; lbEmit = true;
                    break;

                case 474:
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    liType = 17; liSize = 4; lbEmit = true;
                    break;
                case 568:
                    CGS_ASSERT(lp != 0, "lpReqCompCamPicEvent");
                    reinterpret_cast<s32*>(lpOut)[0] = lpW[0];
                    reinterpret_cast<s32*>(lpOut)[1] = lpW[1];
                    reinterpret_cast<s32*>(lpOut)[2] = lpW[2];
                    liType = 53; liSize = 12; lbEmit = true;
                    break;

                default:
                    break;
            }

            if (lbEmit)
                lpNetworkInputQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lpOut), liType, liSize);

            liResult = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        return liResult;
    }
}
