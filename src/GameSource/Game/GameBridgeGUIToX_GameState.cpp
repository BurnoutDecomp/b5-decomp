#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // [DIAG] CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress ("GMInvAccept" / "GMStrOffline")
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (the formatted default-case assert)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"  // BrnGameState::TrainingManager (cases 94 / 473)
#include "SharedClasses/Progression/BrnTrainingTypes.h"               // BrnProgression::ETrainingType

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copies)

// ============================================================================================
// BrnGame::BrnGameModule::BridgeGuiToGameState  @ 0x823DDB78
//
// SIBLING SPLIT (2026-08-25, P1 sim-pause) -- the same pattern as
// GameBridgeGameStateToX_TrainingStringIds.cpp. This function was MOVED here (not copied)
// out of GameBridgeGUIToX.cpp, which CANNOT be mounted: its other two members
// (BridgeGuiToReplay_PostSim / TranslateGuiEventsToNetworkEvents) reference six symbols
// with no home in the linked set -- BrnReplays::ReplayIO::InputBuffer_PostSim::
// GetRequestInterface / GetGuiEventQueue, the three NetworkInSelectScoreboardEvent statics,
// and BrnNetwork::BrnNetworkModuleIO::TelemetryData::AddParameter (that last one has NO
// reconstructed home ANYWHERE in the tree, so the parent TU cannot link at any price today).
// MEASURED, not guessed: mounting the whole TU produced exactly those 6 LNK2019s and nothing
// from this function. Folding it back later is a delete, not a merge.
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
    //
    // [P1 sim-pause] NOW CALLED (the "DELETE-WHEN it has a caller" flags are retired), with
    // two PC adaptations at the seam, both precedented:
    //   * the queue arrives directly (the PC GuiModule owns it; the BridgeGuiToDirector
    //     precedent) and the walk carries that bridge's channel-40 PC-ABI adapter -- the PC
    //     GuiModule keeps records on their channel (40) with the type in the record's own
    //     header word and the payload at its miOutEventOffset, which is exactly what the
    //     console's GuiEventWrapper::GetRawEvent() unwraps.
    //   * the training manager is reached through GetTrainingManager() (the PC module holds
    //     it by pointer), NOT the console's this+0x674B30 raw offset -- a raw console offset
    //     on a host object is the wheel-blanking defect class. The 64-bit game-flags word
    //     (console this+0x691E60; bit 41 gates the case-94 training request) has no PC
    //     member yet -- FLAG'd below, read as 0 (bit clear), which is the console's value
    //     until something sets it.
    // =========================================================================
    int BrnGameModule::BridgeGuiToGameState(
        BrnGameState::GameStateModule* lpGameStateInput,
        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue =
            BrnGameState::GameStateModuleIO::PostWorldInput(lpGameStateInput);

        BrnGameState::TrainingManager* lpTrainingManager =
            lpGameStateInput->GetTrainingManager();
        // FLAG: the 64-bit game-flags word (console this+0x691E60) is un-homed on the PC
        // module; bit 41 reads 0 (see the banner).
        const u64 lu64GameFlags = 0;

        // [DIAG] NOT IN THE X360 BINARY -- prove the bridge is reached at all.
        {
            static bool sbLoggedFirstCall = false;
            if (!sbLoggedFirstCall && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedFirstCall = true;
                *CgsDev::Log::gpDebugPrint
                    << "[sim-pause] BridgeGuiToGameState: FIRST CALL\n";
            }
        }

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        int liResult = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent)
        {
            // ---- resolve (command, payload) -- the channel-40 PC-ABI adapter (see banner) --
            const unsigned char* lp = reinterpret_cast<const unsigned char*>(lpEvent);
            const s32* lpW = reinterpret_cast<const s32*>(lp);
            s32 liCommand = liResult;
            if (liResult == 40 && liEventSize >= 12)
            {
                const u32* lpuRecord = reinterpret_cast<const u32*>(lpEvent);
                liCommand = static_cast<s32>(lpuRecord[1]);
                const u32 luOffset = lpuRecord[2];
                if (luOffset >= 12u && static_cast<s32>(luOffset) < liEventSize)
                {
                    lp  = reinterpret_cast<const unsigned char*>(lpEvent) + luOffset;
                    lpW = reinterpret_cast<const s32*>(lp);
                }
            }

            alignas(16) unsigned char lBuf[512];
            unsigned char* lpOut = lBuf;
            s32 liType = 0;
            s32 liSize = 0;
            bool lbEmit = false;

            switch (liCommand)
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
                    if (lp[0] != 0 && (lu64GameFlags & (1ULL << 41)) == 0)
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
                    // [DIAG] NOT IN THE X360 BINARY -- the pause spine's head rung.
                    if (CgsDev::Log::gpDebugPrint != 0)
                        *CgsDev::Log::gpDebugPrint
                            << "[sim-pause] GUI 191 activate=" << lpW[0]
                            << " -> game event 93 payload " << ((lpW[0] == 0) ? 1 : 0) << "\n";
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

}
