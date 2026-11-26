// GameState-to-X bridges. ARTIST 0x823EE880 is recovered below with its direct
// output builders in EventStatusGuiEvents/EventStartsGuiEvents. The network
// wrapper remains deferred; the action translator has its separately owned TU.
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/Game/GameBridgeControllerToX.h"                // CgsGui::GuiModule + AddGuiEvent<T> (established placeholder home)

// [gateui] the real GUI-event payload homes (the two placeholders this TU used to fork live here).
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::GuiTakedownEvent / GuiSoftTakedownEvent
// GameSource/Gui/BrnGuiDemangledEventTypes.h is DELIBERATELY NOT INCLUDED HERE, and that is a
// MEASURED blocker, not a preference. It re-defines two payload types that are ALSO forked in
// headers this TU already reaches through BrnGameModule.hpp:
//     BrnGuiDemangledEventTypes.h GuiEventNetworkPlayerImage
//         vs GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h
//     BrnGuiDemangledEventTypes.h GuiEventToggleChangeCarMessage
//         vs GameSource/Game/GameBridgeControllerToX.h
// Both forks are pre-existing and neither is repaired here: deleting the
// GameBridgeControllerToX.h fork (this lane's half) would force ITS TU to include the demangled
// header, where the renderer fork -- a Gui-lane file -- would break it in turn. Filed as a
// shared_header_request; the consequence for this TU is the parked autosave-request post in the
// action-58 arm of the stunt sibling.
#include "GameSource/GameState/BrnGameActions.h"                   // BrnGameState::GameStateModuleIO action payloads
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer / GameActionQueue
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"  // BrnGameState::TakedownEvent
#include "GameShared/GameClasses/Module/CgsEventQueue.h"           // CgsModule::BaseEventQueue / EventQueue<TakedownEvent,8>
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // CgsGuiModuleIO::InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // [td-gui] PC witness
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include <cstring>
#include <cstdlib>                                                  // std::getenv ([td-gui] switch)

namespace BrnGame
{
    // ARTIST 0x823EE880: r3=this, r4=GUI input, r5=GameState output.
    // The AddGuiEvent instantiations do not read their CgsGui::GuiModule this;
    // PushGuiEvent follows their existing whole-record queue boundary.
    void BrnGameModule::BridgeGameStateToGui(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        CGS_ASSERT_AT(lpGuiInput != 0, "lpGuiInput", CGS_ARTIST_FILE, 423);
        CGS_ASSERT_AT(lpGameStateOutput != 0, "lpGameStateOutput", CGS_ARTIST_FILE, 424);
        const BrnGameState::GameStateModuleIO::ScoringOutputInterface* lpScoringOutputInterface =
            lpGameStateOutput->GetScoringOutputInterface();
        const BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface* lpOnlScoringOutputInterface =
            lpGameStateOutput->GetOnlineScoringOutputInterface();
        CGS_ASSERT_AT(lpScoringOutputInterface != 0, "lpScoringOutputInterface", CGS_ARTIST_FILE, 431);
        CGS_ASSERT_AT(lpOnlScoringOutputInterface != 0, "lpOnlScoringOutputInterface", CGS_ARTIST_FILE, 432);
        CGS_ASSERT_AT(lpGuiInput->GetGuiEvents() != 0, "lpGuiInput->GetGuiEvents()", CGS_ARTIST_FILE, 434);
        CGS_ASSERT_AT(lpGameStateOutput->GetGuiEventQueue() != 0, "lpGameStateOutput->GetGuiEventQueue()", CGS_ARTIST_FILE, 435);

        // 823EE9C4: the Append result is ignored. It neither clears nor retires its source.
        lpGuiInput->GetGuiEvents()->Append(*lpGameStateOutput->GetGuiEventQueue());
        BridgeGameStateToGui_EventStatus(&mTimerStatusInterface, lpGameStateOutput, lpGuiInput);
        BridgeGameStateToGui_EventStarts(lpGameStateOutput, lpGuiInput);
        if (lpGameStateOutput->GetSpecificGameModeEventInterfaceIsValid())
        {
            BrnGui::GuiEventSpecificPresetRaces lEvents;
            const BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface& lrEvents =
                lpGameStateOutput->GetSpecificGameModeEventInterface();
            std::memcpy(&lEvents.mEvents, &lrEvents, sizeof(lEvents.mEvents));
            PushGuiEvent(lEvents, lpGuiInput); // 194 / 7704, 823D13B0; no GuiEvent header
        }
        TranslateGameActionsToGuiEvents(lpGuiInput, lpGameStateOutput);
        CGS_ASSERT(lpGameStateOutput->GetTakedownEventOutputQueue() != 0,
                   "lpGameStateOutput->GetTakedownEventOutputQueue()");
        TranslateTakedownsToGuiEvents(lpGuiInput, lpGameStateOutput->GetTakedownEventOutputQueue(),
                                     static_cast<s32>(lpScoringOutputInterface->mePlayerRaceCarIndex));
        CGS_ASSERT(lpGameStateOutput->GetGameStateToGuiInterface() != 0,
                   "lpGameStateOutput->GetGameStateToGuiInterface()");
        TranslateGuiInterfaceToGuiEvents(lpGuiInput, lpGameStateOutput->GetGameStateToGuiInterface());

        // 823EF2C0..823EF300: status snapshot, not the live game timer.
        const CgsSystem::TimerStatus* lpGameTimerStatus = mTimerStatusInterface.GetGameTimerStatus();
        const CgsSystem::Time lGameTime = lpGameTimerStatus->GetTime();
        CgsGui::GuiEventTimeInfo lTimeInfo;
        lTimeInfo.Set(lpGameTimerStatus->GetCurrentTimeStep(),
                      static_cast<f32>(lGameTime.GetSeconds()) + lGameTime.GetFraction());
        PushGuiEvent(lTimeInfo, lpGuiInput); // 26 / 8
    }

    void BrnGameModule::TranslateGuiInterfaceToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::GameStateToGuiInterface* lpGameToGuiInterface)
    {
        typedef BrnGameState::GameStateModuleIO::GameStateToGuiInterface GameStateToGuiInterface;

        s32 liIndex;

        const GameStateToGuiInterface::NewDirtyTrickQueue* lpDirtyTrickNewQueue =
            lpGameToGuiInterface->GetNewDirtyTrickQueue();                      // + 4
        const GameStateToGuiInterface::DirtyTrickTriggeredQueue* lpDirtyTrickTriggeredQueue =
            lpGameToGuiInterface->GetDirtyTrickTriggeredQueue();                // + 0x40
        const GameStateToGuiInterface::DirtyTrickEndingQueue* lpDirtyTrickEndingQueue =
            lpGameToGuiInterface->GetDirtyTrickEndingQueue();                   // + 0x7C
        const GameStateToGuiInterface::OvertakeEventQueue* lpOvertakeQueue =
            lpGameToGuiInterface->GetOvertakeEventQueue();                      // + 0xC8
        const GameStateToGuiInterface::FinishedRaceEventQueue* lpFinishRaceQueue =
            lpGameToGuiInterface->GetFinishedRaceEventQueue();                  // + 0xF4
        const GameStateToGuiInterface::TookLeadEventQueue* lpTookLeadEventQueue =
            lpGameToGuiInterface->GetTookLeadEventQueue();                      // + 0x120
        const GameStateToGuiInterface::TookLastEventQueue* lpTookLastEventQueue =
            lpGameToGuiInterface->GetTookLastEventQueue();                      // + 0x140
        const GameStateToGuiInterface::OnTailEventQueue* lpOnTailEventQueue =
            lpGameToGuiInterface->GetOnTailEventQueue();                        // + 0x160

        const s32 liDTNewLength       = lpDirtyTrickNewQueue->GetLength();
        const s32 liDTTriggeredLength = lpDirtyTrickTriggeredQueue->GetLength();
        const s32 liDTEndedLength     = lpDirtyTrickEndingQueue->GetLength();
        const s32 liOvertakeLength    = lpOvertakeQueue->GetLength();
        const s32 liFinishRaceLength  = lpFinishRaceQueue->GetLength();
        const s32 liTookLeadLength    = lpTookLeadEventQueue->GetLength();
        const s32 liTookLastLength    = lpTookLastEventQueue->GetLength();
        const s32 liOnTailQueueLength = lpOnTailEventQueue->GetLength();

        for (liIndex = 0; liIndex < liDTNewLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiNewDirtyTrick lDirtyTrickNew =
                lpDirtyTrickNewQueue->GetEvent(liIndex);
            BrnGui::GuiDirtyTrickNewEvent lGuiDirtyTrickNewEvent;
            lGuiDirtyTrickNewEvent.meAggressorActiveRaceCarIndex = lDirtyTrickNew.meAggressorActiveRaceCarIndex;
            lGuiDirtyTrickNewEvent.meVictimActiveRaceCarIndex    = lDirtyTrickNew.meVictimActiveRaceCarIndex;
            lGuiDirtyTrickNewEvent.meTrickType                   = lDirtyTrickNew.meTrickType;
            PushGuiEvent(lGuiDirtyTrickNewEvent, lpGuiInput);                   // 177 / 12
        }

        for (liIndex = 0; liIndex < liDTTriggeredLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiTriggeredDirtyTrick lDirtyTrickTriggered =
                lpDirtyTrickTriggeredQueue->GetEvent(liIndex);
            BrnGui::GuiDirtyTrickTriggerEvent lGuiDirtyTrickTriggerEvent;
            lGuiDirtyTrickTriggerEvent.meAggressorActiveRaceCarIndex = lDirtyTrickTriggered.meAggressorActiveRaceCarIndex;
            lGuiDirtyTrickTriggerEvent.meVictimActiveRaceCarIndex    = lDirtyTrickTriggered.meVictimActiveRaceCarIndex;
            lGuiDirtyTrickTriggerEvent.meTrickType                   = lDirtyTrickTriggered.meTrickType;
            PushGuiEvent(lGuiDirtyTrickTriggerEvent, lpGuiInput);               // 179 / 12
        }

        for (liIndex = 0; liIndex < liDTEndedLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiEndingDirtyTrick lDirtyTrickEnding =
                lpDirtyTrickEndingQueue->GetEvent(liIndex);
            BrnGui::GuiDirtyTrickEndedEvent lGuiDirtyTrickEndedEvent;
            lGuiDirtyTrickEndedEvent.meAggressorActiveRaceCarIndex = lDirtyTrickEnding.meAggressorActiveRaceCarIndex;
            lGuiDirtyTrickEndedEvent.meVictimActiveRaceCarIndex    = lDirtyTrickEnding.meVictimActiveRaceCarIndex;
            lGuiDirtyTrickEndedEvent.meTrickType                   = lDirtyTrickEnding.meTrickType;
            lGuiDirtyTrickEndedEvent.mbSurvived                    = lDirtyTrickEnding.mbSurvived;  // `lbz 0x7c` -> `stb 0x6c`
            PushGuiEvent(lGuiDirtyTrickEndedEvent, lpGuiInput);                 // 181 / 16
        }

        for (liIndex = 0; liIndex < liOvertakeLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiOvertakeEvent lOvertake =
                lpOvertakeQueue->GetEvent(liIndex);
            BrnGui::GuiOvertakeEvent lGuiOvertakeEvent;
            lGuiOvertakeEvent.muNewPosition        = lOvertake.mu8NewPosition;          // rec +0 -> +4 (`stb`)
            lGuiOvertakeEvent.meActiveRaceCarIndex = lOvertake.meActiveRaceCarIndex;    // rec +4 -> +0
            PushGuiEvent(lGuiOvertakeEvent, lpGuiInput);                        // 371 / 8
        }

        for (liIndex = 0; liIndex < liFinishRaceLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiFinishedRaceEvent lFinishRace =
                lpFinishRaceQueue->GetEvent(liIndex);
            BrnGui::GuiFinishRaceEvent lGuiFinishRaceEvent;
            lGuiFinishRaceEvent.meFinishType         = lFinishRace.meFinishType;         // rec +0 -> +4
            lGuiFinishRaceEvent.meActiveRaceCarIndex = lFinishRace.meActiveRaceCarIndex; // rec +4 -> +0
            PushGuiEvent(lGuiFinishRaceEvent, lpGuiInput);                      // 372 / 8
        }

        for (liIndex = 0; liIndex < liTookLeadLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiTookLeadEvent lTookLeadEvent =
                lpTookLeadEventQueue->GetEvent(liIndex);
            BrnGui::GuiTookLeadEvent lGuiTookLeadEvent;
            lGuiTookLeadEvent.meLeadActiveRaceCarIndex = lTookLeadEvent.meActiveRaceCarIndex;  // rec +8 -> +8
            lGuiTookLeadEvent.mOfflineRivalCarID       = lTookLeadEvent.mOfflineRivalCarID;    // rec +0 -> +0
            PushGuiEvent(lGuiTookLeadEvent, lpGuiInput);                        // 484 / 16
        }

        for (liIndex = 0; liIndex < liTookLastLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiTookLastEvent lTookLastEvent =
                lpTookLastEventQueue->GetEvent(liIndex);
            BrnGui::GuiTookLastEvent lGuiTookLastEvent;
            lGuiTookLastEvent.meLastActiveRaceCarIndex = lTookLastEvent.meActiveRaceCarIndex;  // rec +8 -> +8
            lGuiTookLastEvent.mOfflineRivalCarID       = lTookLastEvent.mOfflineRivalCarID;    // rec +0 -> +0
            PushGuiEvent(lGuiTookLastEvent, lpGuiInput);                        // 485 / 16
        }

        for (liIndex = 0; liIndex < liOnTailQueueLength; ++liIndex)
        {
            const BrnGameState::GameStateToGuiOnTailEvent lOnTailEvent =
                lpOnTailEventQueue->GetEvent(liIndex);
            BrnGui::GuiNetworkPlayerOnTailEvent lGuiOnTailEvent;
            lGuiOnTailEvent.meOnTailActiveRaceCarIndex = lOnTailEvent.meOnTailActiveRaceCarIndex;  // rec +8 -> +8
            lGuiOnTailEvent.mOfflineRivalCarID         = lOnTailEvent.mOfflineRivalCarID;          // rec +0 -> +0
            PushGuiEvent(lGuiOnTailEvent, lpGuiInput);                          // 486 / 16
        }

    }

    // =========================================================================
    // TranslateTakedownsToGuiEvents
    //
    // For each record in the game-state output's TakedownEvent output queue, synthesise a
    // takedown GUI event and push it into the GUI module's input buffer. A record is a HARD
    // takedown (BrnGui::GuiTakedownEvent, id 363 / 40 bytes) when the record's VICTIM index
    // equals the player's active race-car index, or when the soft-takedown-display flag bit is
    // clear; otherwise it is a SOFT takedown (BrnGui::GuiSoftTakedownEvent, id 364 / 32 bytes),
    // which carries the same leading five fields plus the two status bytes pulled forward to
    // +0x1C/+0x1D and no chain/multiple counts.
    //
    // Both arms are transcribed store-for-store off the console body, whose source image is the
    // queue element copied out whole (5 qwords == sizeof(BrnGameState::TakedownEvent)). The hard
    // arm's two extra stores CROSS: event miTakedownChainCount takes record +0x20 and event
    // miMultipleTakedownCount takes record +0x1C -- the record's multiple count precedes its
    // chain count, the event's does not.
    //
    // Branch polarity read off the assembly and NOT the pseudocode: compare the record's victim
    // index against the player index, equal -> HARD; otherwise load the 64-bit display-flag word,
    // mask bit 33, and take HARD when the masked value is ZERO. SOFT happens only when the record
    // is not the player's AND the display bit is SET.
    //
    // The queue arrives typed: it is the same payload BridgeGameStateToWorld already names, the
    // committed CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>, reached through its
    // BaseEventQueue<TakedownEvent> base. The one cross-home cast the caller still needs (the
    // output buffer's accessor returns the forward-declared TakedownEventOutputQueueType) lives at
    // the call site, with the same documented note the world bridge carries.
    //
    // NO ENTRY GUARD, DELIBERATELY. The console body has neither an assert nor a null test: its
    // prologue loads the queue length and falls straight into the ble-exit. The
    // "lpGameStateOutput->GetTakedownEventOutputQueue()" assert belongs to the CALLER, which fires
    // it on the queue accessor's return immediately before this call -- so it must land at the
    // call site, not here.
    // =========================================================================
    void BrnGameModule::TranslateTakedownsToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* lpTakedownQueue,
        s32 liPlayerActiveRaceCarIndex)
    {
        const s32 liCount = lpTakedownQueue->GetLength();

        for (s32 i = 0; i < liCount; ++i)
        {
            const BrnGameState::TakedownEvent& lrRecord = lpTakedownQueue->GetEvent(i);

            // HARD when the record's victim IS the player, or the soft-display bit is clear.
            bool lbHard;
            if (static_cast<s32>(lrRecord.meVictimIndex) == liPlayerActiveRaceCarIndex)
            {
                lbHard = true;
            }
            else
            {
                lbHard = !mGameStateModule.GetModeManager()->GetCurrentGameModeParams()->GetFlag(
                    BrnGameState::GameModeParams::KU_FLAG_ENFORCE_SOFT_TAKEDOWNS);
            }

            if (lbHard)
            {
                BrnGui::GuiTakedownEvent lEvent;
                lEvent.mAggressorCarID         = lrRecord.mAggressorCarID;         // +0x00 <- rec +0x08
                lEvent.mVictimCarID            = lrRecord.mVictimCarID;            // +0x08 <- rec +0x10
                lEvent.meAggressorIndex        = lrRecord.meAggressorIndex;        // +0x10 <- rec +0x00
                lEvent.meVictimIndex           = lrRecord.meVictimIndex;           // +0x14 <- rec +0x04
                lEvent.meTakedownType          = lrRecord.meType;                  // +0x18 <- rec +0x18
                lEvent.miTakedownChainCount    = lrRecord.miTakedownChainCount;    // +0x1C <- rec +0x20
                lEvent.miMultipleTakedownCount = lrRecord.miMultipleTakedownCount; // +0x20 <- rec +0x1C
                lEvent.mbMarkedManTakeDown     = lrRecord.mbMarkedManTakeDown;     // +0x24 <- rec +0x24
                lEvent.mbSettledScore          = lrRecord.mbSettledScore;          // +0x25 <- rec +0x26
                PushGuiEvent(lEvent, lpGuiInput);
            }
            else
            {
                BrnGui::GuiSoftTakedownEvent lEvent;
                lEvent.mAggressorCarID     = lrRecord.mAggressorCarID;         // +0x00 <- rec +0x08
                lEvent.mVictimCarID        = lrRecord.mVictimCarID;            // +0x08 <- rec +0x10
                lEvent.meAggressorIndex    = lrRecord.meAggressorIndex;        // +0x10 <- rec +0x00
                lEvent.meVictimIndex       = lrRecord.meVictimIndex;           // +0x14 <- rec +0x04
                lEvent.meTakedownType      = lrRecord.meType;                  // +0x18 <- rec +0x18
                lEvent.mbMarkedManTakeDown = lrRecord.mbMarkedManTakeDown;     // +0x1C <- rec +0x24
                lEvent.mbSettledScore      = lrRecord.mbSettledScore;          // +0x1D <- rec +0x26
                PushGuiEvent(lEvent, lpGuiInput);
            }
        }
    }

} // namespace BrnGame

// FOLDED FROM GameBridgeGameStateToX_Controller.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_Controller.cpp
//
// SIBLING SPLIT of GameSource/Game/GameBridgeGameStateToX.cpp (the same MOVED-not-copied
// split as _TrainingStringIds.cpp / _Sound.cpp): BrnGameModule::BridgeGameStateToController
// lives here so its owning TU can be mounted without it.
//
// The typed GameStateToController read accessor and PostWorldInput request posts are now homed.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"  // CgsInput::InputIO::PostWorldInputBuffer / BaseInputEvent

namespace BrnGame
{
    // =========================================================================
    // BridgeGameStateToController
    // Merge the game-state output's input-bind / input-unbind REQUEST queues into the input
    // module's post-world input buffer: PostBindRequest per queued bind, PostUnbindRequest per
    // queued unbind. When either queue produced any work, stamp miInputModuleState with the
    // bind (3) / unbind (6) sentinel. Called by DoUpdate_InputPostWorld.
    //
    // ARTIST 0x823B9CD8 reads the GameStateToControllerInterface under the output read lock.
    // =========================================================================
    void BrnGameModule::BridgeGameStateToController(
        CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue* lpBindQueue =
            lpGameStateOutput->GetGameStateToControllerInterface()->GetBindRequestQueue();
        const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue* lpUnbindQueue =
            lpGameStateOutput->GetGameStateToControllerInterface()->GetUnBindRequestQueue();

        CGS_ASSERT_AT(lpBindQueue   != 0, "lpGameStateInputBindRequestQueue", CGS_ARTIST_FILE, 5055);
        CGS_ASSERT_AT(lpUnbindQueue != 0, "lpGameStateInputUnbindRequestQueue", CGS_ARTIST_FILE, 5056);

        // Bind requests: element i carries {word0, word1} (asm ld 8 bytes then split r4/r5).
        const s32 liNumBind = lpBindQueue->GetLength();
        for (s32 i = 0; i < liNumBind; ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpBindQueue->GetEvent(i);
            lpPostWorldInput->PostBindRequest(lrEvent.miPlayer, lrEvent.miPort);
        }
        if (liNumBind > 0)
            miInputModuleState = E_INPUTBINDSTATE_BIND_REQUESTED;

        // Unbind requests: element j carries a single request word (asm *v12 -> PostUnbindRequest).
        const s32 liNumUnbind = lpUnbindQueue->GetLength();
        for (s32 j = 0; j < liNumUnbind; ++j)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpUnbindQueue->GetEvent(j);
            lpPostWorldInput->PostUnbindRequest(lrEvent.miPlayer);
        }
        if (liNumUnbind > 0)
            miInputModuleState = E_INPUTBINDSTATE_UNBIND_REQUESTED;
    }

} // namespace BrnGame

// FOLDED FROM GameBridgeGameStateToX_EventFlowGuiEvents.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp
//
// [WAVE E1 -- event GUI prerequisites, 2026-08-26]
// THE EVENT-FLOW SLICE of BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0.
//
// The console body is one ~105 KB switch (jpt_823EA1F0) over ~300 game actions. Two slices of
// it live in b5-decomp:
//
//   GameBridgeGameStateToX_StuntGuiEvents.cpp   -- MapStuntEnumsFromGameplayToGui @0x823AA4A8
//                                                  and the member TranslateGameActionsToGuiEvents
//                                                  itself, carrying arms 58 / 59 / 60 / 112 / 148.
//   THIS FILE                                   -- the EVENT-FLOW arms: 23, 30, 37, 38, 39, 44,
//                                                  47, 75, 200, 201 -- and, since 2026-09-02, the
//                                                  three ROAD RAGE arms 103 / 255 / 205 (-> GUI
//                                                  168 / 427 / 348), on the same terms as arm 75.
//                                                  Since 2026-09-27 also the race / burning-route
//                                                  / post-event arms 40, 113, 204, 206, 208, 210,
//                                                  251, 262 and 272 (the wrong-car refusal).
//   GameBridgeGameStateToX_EventFlowGuiEvents_wZ_00.cpp -- the free-roam arms 57, 99, 104, 105,
//                                                  109, 190, 263, 264 and 265, reached from the
//                                                  `default:` of this file's switch.
//
// ⭐⭐⭐ ARM 75 IS NOT AN EVENT-FLOW ARM AND IS HERE ON PURPOSE (returning-player wave,
// 2026-08-28). It belongs to the CAR-SELECT band, but the same three reasons that put every arm
// above in a sibling TU apply unchanged -- the member function has exactly one definition
// (in the stunt TU), this file is
// already the wired dispatch seam. Opening a third partfile for ONE three-instruction arm would
// buy nothing and cost another mount. It is listed under its own heading below.
//
// WHY A SIBLING TU AND NOT MORE ARMS IN THE STUNT TU: the same reason that TU exists at all
// (its banner, and the ConvertTrainingTypeToStringId split of 2026-08-16) -- one 105 KB
// function reconstructed by five different waves in one file is a merge hazard. The member function has exactly ONE definition,
// in the stunt TU, so this file cannot re-define it; it exposes the arms as a dispatch-seam
// helper instead, shaped exactly like the switch body it came from.
//
// ✅ WIRED (2026-08-27 audit; the wire-request banner that stood here was STALE). All three
// steps landed: the declaration is in GameSource/Game/GameBridgeGameStateToX.h:87, the
// `default:` arm of TranslateGameActionsToGuiEvents calls it
// (GameBridgeGameStateToX_StuntGuiEvents.cpp:399-406), and this file is mounted (bat 4388).
// Runtime-proven: BrnGame.log shows action 201 -> gui 311 flowing and action 23 driving the
// FSM hop to PRE_FLY_BY.
//
// WHAT THIS BUYS, IN ORDER OF VISIBILITY
//   201 -> GuiEventJunctionInfo(311)   THE ORACLE. The consumer half is COMPLETE AND MOUNTED
//                                      (BrnJunctionInfoComponent + BrnFBurnMainHudState case
//                                      311 + BrnOdometerComponent), so the moment a producer
//                                      posts action 201 the "hold accelerator + brake" junction
//                                      panel appears. Nothing else in this wave is observable
//                                      without further reconstruction.
//    23 -> GuiEventPrepareForModeStart(93) + GuiEventRunFsm(144, BRNEVENTFSM / E_GUI_HUD_EVENT)
//                                      -- the FSM hop that puts the GUI into the event HUD.
//    30 -> CgsGui::GuiEvent<164>        [ADDED 2026-08-27, stunt-race frontier round 2 / D1]
//                                      THE FLY-BY EXIT. 23 gets the GUI INTO PRE_FLY_BY; this arm
//                                      is the only thing in the image that gets it back out. Its
//                                      consumer (PreRaceFlyByState's KI_EVENT_FLYBY_END arm) is
//                                      mounted, so landing this arm closes the whole
//                                      163 -> 25 -> IntroState::OnLeave -> action 30 -> 164 ->
//                                      "BF_PROCEED" loop. See the arm's own banner.
//                                      (Online lobby/showtime take the other branch: 269 / 279.)
//    47 -> GuiEventUpdateEventCountdown(234)
//    44 -> GuiEventEnterEventStartLocation(166)
//    38 -> GuiEventFinishedModeResults(321)
//    37 -> GuiEventOfflinePostEvent(289) / GuiEventTriggerOnlinePostEvent(320) / GuiEvent<291>
//    39 -> GuiEventStopMode(322)
//   200 -> GuiEventMedalUpdate(307)
//
// ID DISCIPLINE
// ACTION ids come from BrnGameActions.h's new event-flow block, where each value is cited to a
// producer AND to this consumer's jump-table case. GUI event ids come from each type's
// CgsGui::GuiModule::AddGuiEvent<T> instantiation, whose AddEvent(&event, <id>, <size>) literals
// are quoted per record below. Game ACTION ids are X360-shifted; GUI event ids are their own
// space and are NOT shifted.
//
// THE PAYLOAD RECORDS BELOW ARE TU-LOCAL ON PURPOSE, AND THAT IS THE HOUSE PRECEDENT.
// GameSource/Gui/BrnGuiDemangledEventTypes.h CANNOT BE INCLUDED FROM THIS TU -- measured, not
// assumed: it re-defines BrnGui::GuiEventNetworkPlayerImage, which
// GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h:75 (reached through
// BrnGameModule.hpp) already defines, so the include is a hard C2011. That is the identical
// fork the stunt TU's case-58 arm documents when it parks its GuiAutosaveRequestEvent post.
// Independently of the fork, the shells in that header would be the WRONG SHAPE for this
// posting path anyway: AddGuiEvent<T> posts `&event` whole, with the payload's own fields at
// record offset +0 (see every store map quoted below), while the shells for the 4-aligned
// >= 12-byte events are `CgsGui::GuiEvent<N> + u8 maPayload[size-12]`, which puts every field
// 12 bytes late. GuiEventStopMode is the sharpest case: it is spelled
// `: public CgsGui::GuiEvent<322> {}`, i.e. its 12 bytes are ALL header and there is nowhere to
// put the three words the console actually sends -- exactly the defect that header's own
// GuiEventBoostBarStuntInfo note records ("the 12 bytes ARE the payload").
// So each record is rebuilt here from its producer store map, at its attested size, carrying its
// own GetEventType() (mandatory -- PushGuiEvent bakes id and size off the type), exactly as
// GameBridgeGameStateToX_StuntGuiEvents.cpp's TickerCustomMessageWire537 and
// BrnCarSelectLivery_Components.cpp's GuiTickerCustomMessagePayload already do.
//   SHARED_HEADER_REQUEST (owner: the Gui lane), in this order:
//     (a) remove the GuiEventNetworkPlayerImage fork from BrnNetworkPlayerImageRenderer.h:75 so
//         BrnGuiDemangledEventTypes.h becomes includable from the bridge family;
//     (b) THEN re-shape 93 / 289 / 322 in BrnGuiDemangledEventTypes.h onto the store maps below
//         -- but ONLY once a CONSUMER exists to arbitrate the field names. Today none of these
//         eight event ids has a single consumer anywhere in b5-decomp/src (grepped: only the
//         AddGuiEvent instantiation list and the header itself), so this wave deliberately
//         re-shapes NOTHING there: a producer store map proves offsets and widths, not names.
//     (c) then these TU-local records collapse into `using BrnGui::...;`.
//
// GuiEventRunFsm CARRIES THE WRONG ID IN ITS OWN HEADER. BrnGuiEventTypeDefs.h:140 has
// `GetEventType() const { return 142; }`, but AddGuiEvent<GuiEventRunFsm> @0x823D2438 bakes
// `AddEvent(q, ev, 144, 24)` and the MOUNTED consumer is BrnGuiModule.cpp:1758 `case 144`.
// 142 is dead on both ends. This TU therefore posts the record the way the tree's own live
// producer already does (BrnGameModule::BridgeGameToGui @0x823DCA10, BrnGameModule.cpp:625-637):
// build the real BrnGui::GuiEventRunFsm and AddEvent it with a literal 144.
// SHARED_HEADER_REQUEST (owner: the Gui lane): correct BrnGuiEventTypeDefs.h:140 to 144 and
// delete both hand-rolled posts.
// ============================================================================

#include "GameSource/Gui/Events/BrnGuiEventRoadRuleData.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"                // PushGuiEvent<T>

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                     // CgsID / CgsIDCompress
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // GuiEventJunctionInfo, GuiEventRunFsm
#include "GameSource/GameState/BrnGameActions.h"                   // the action payload homes
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer
#include "GameSource/GameState/BrnGameStateSharedIO.h"             // EGameModeType
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // GameModeParams
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // KI_MAX_CHALLENGES
#include <cstring>                                                 // memset / memcpy

namespace BrnGame
{
namespace
{
    // The two mode-set tests the event-flow arms share, spelled once. Both are literal
    // transcriptions: `cmpwi 2 / beq ... cmpwi 0x10 / bne` is the SHOWTIME pair (offline 2,
    // online 16), and `cmpwi 0xF / beq ... cmpwi 0x10 / bne` is the online free-burn-lobby +
    // online-showtime pair.
    inline bool IsShowtimeMode(s32 liMode)
    {
        return liMode == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
               liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
    }
    inline bool IsOnlineLobbyOrShowtimeMode(s32 liMode)
    {
        return liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
               liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
    }

    // =====================================================================================
    // GUI-EVENT WIRE RECORDS. One per posted id; each carries its attested (id, size) pair in
    // GetEventType() + sizeof, both read off that type's AddGuiEvent<T> instantiation.
    // =====================================================================================

    // id 93 size 152 -- AddGuiEvent<GuiEventPrepareForModeStart> @0x823D27D0.
    // Store map: the case-23 arm's var_30D0-based frame, @0x823EADCC..0x823EAFD0.
    struct PrepareForModeStartWire93
    {
        CgsID mPursuedCarId;                   // +0x00  GameModeParams::mPursuedCarID (ld 0x58(r25))
        s32   miCurrentRound;                  // +0x08  action->GetCurrentRound()     (lwz 0x8A0(r31))
        s32   meGameModeType;                  // +0x0C  GameModeParams::GetGameModeType()
        u32   muEventJunctionID;               // +0x10  GameModeParams::muEventJunctionID (+0x44)
        u32   muJunctionID;                    // +0x14  GameModeParams::muJunctionID      (+0x48)
        u16   mau16CheckpointLandmark[16];     // +0x18  CheckpointData[i]+0x00, i < GetCheckpointCount()
        s32   maiCheckpointDistrict[16];       // +0x38  CheckpointData[i]+0x04
        // The medal-run pin (closure verify 2026-08-26): GameModeParams +0x60/0x64/0x68/0x6C are
        // bronze/silver/gold/mfModeTimeLimit (SetMedalModeTimer's own assert names +0x68's value
        // lfGoldTimeLimitSeconds). The console arm copies them straight through
        // (@0x823EAE00..0x823EAE2C), so the wire slots are:
        f32   mfNeedForBronze;                 // +0x78  GameModeParams+0x60
        f32   mfNeedForSilver;                 // +0x7C  GameModeParams+0x64
        f32   mfNeedForGold;                   // +0x80  GameModeParams+0x68
        f32   mfModeTimeLimit;                 // +0x84  GameModeParams+0x6C
        s32   miPursuitRivalTotalDamage;       // +0x88  GameModeParams+0x50
        u8    mu8CheckpointCount;              // +0x8C  (u8)GameModeParams::GetCheckpointCount()
        u8    mu8DifficultyLevel;              // +0x8D  GameModeParams::muDifficultyLevel (+0x70)
        u8    mu8CarCount;                     // +0x8E  mbIsOnline ? miNumNetworkPlayers : miNumRivals
        u8    mu8RoadRageThreshold;            // +0x8F  (u8)GameModeParams::miRoadRageThreshold (+0x4C)
        u8    mbIsOnline;                      // +0x90  GameModeParams::mbIsOnline (+0x94)
        u8    mbOnlineLobbyTransition;         // +0x91  see the arm (name FLAGGED there)
        u8    maPad92[6];                      // +0x92..+0x97 tail padding to the attested 152

        s32 GetEventType() const { return 93; }
    };
    CGS_STATIC_ASSERT((sizeof(PrepareForModeStartWire93) == 152),
                  "X360 AddGuiEvent<GuiEventPrepareForModeStart> posts 152 bytes (id 93)");

    // id 164 size 1 -- AddGuiEvent<CgsGui::GuiEvent<164>> @0x823D2B68 (`li r6,1` / `li r5,0xA4`
    // into VariableEventQueue<32768,16>::AddEvent, asm 0x823D2C04..0x823D2C10).
    // ⭐ THE FLY-BY'S EXIT SIGNAL. The mangled name carries no BrnGui type name (the console posts
    // the bare CgsGui::GuiEvent<164> tag) and the case-30 arm hands it a 1-byte stack slot it never
    // writes (`addi r4, r1, var_3543` @0x823EB330, with no preceding store), so there is no payload
    // to model. Reproduced as a zeroed byte -- the host must not post uninitialised memory; the
    // consumer never reads it. Same accommodation as FinishedModeResultsWire321 below.
    // The CONSUMER IS MOUNTED and reads only the id: BrnGui::PreRaceFlyByState::HandleIncomingEvents'
    // KI_EVENT_FLYBY_END arm (BrnPreRaceFlyBy_wJ_03.cpp:129), which plays the trans-out and ends in
    // TriggerExitState -> SendStateEvent("BF_PROCEED") -- the ONLY producer of the lua BRNEVENTFSM's
    // PRE_FLY_BY exit edge. 164 is also one of the eight ids the state registers for
    // (PreRaceFlyByState::maiEventToObserve @0x82065CAC == {6,21,64,159,160,162,164,213}).
    // id 237 size 16 -- AddGuiEvent<BrnGui::GuiGameModeStarted> @0x823D4A50 (`li r5,0xED` /
    // `li r6,0x10` into VariableEventQueue<32768,16>::AddEvent). Posted by the case-34 arm
    // (E_ACTION_START_PLAYING_MODE) of TranslateGameActionsToGuiEvents @0x823E9CE0, which
    // copies the action's landmark id to +0 and its mode type to +8 of the 16-byte record. The
    // ONE consumer is GuiCache::RecEvent case 237 @0x8250DDF0 (`*(a1 + 40980) = 0`): it clears
    // mbEventPreparedForModeStart and reads nothing else. The record is the payload alone
    // (GuiEvent<237> is empty).
    // ⭐ WHY IT MATTERS (2026-09-10): RaceMainHudState::UpdateWFInit defers its reveal while
    // IsEventPreparedForModeStart() && mbPreRaceCountdown ("waiting for the countdown GO").
    // Without this arm the flag stayed up for the whole event, so pausing a RACE / BURNING
    // ROUTE / ELIMINATOR and unpausing re-entered RACE_MAIN into that deferral and the HUD
    // never came back (stunt runs / road rage arm no countdown, which is why they were fine).
    struct GameModeStartedWire237 : public CgsGui::GuiEvent<237>
    {
        CgsID mDestinationLandmarkID;          // +0x00  `ld 8(action)` / `std 0x4D0` (0x823EBD5C)
        s32   meGameMode;                      // +0x08  `lwz 0(action)` / `stw 0x4D8` (0x823EBD48)
    };                                         // DWARF GuiGameModeStarted (h:4815/4816) names,
                                               // ARTIST's X360 member order
    CGS_STATIC_ASSERT((sizeof(GameModeStartedWire237) == 16), "id 237 size 16");

    struct PreRaceFlyByEndWire164    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 164; }
    };
    CGS_STATIC_ASSERT((sizeof(PreRaceFlyByEndWire164) == 1), "id 164 size 1");

    // id 269 size 4 -- AddGuiEvent<BrnGui::GuiEventNetworkSplashEvent> @0x823D2C20
    // (`AddEvent(q, ev, 269, 4)`). Store map: one word, `stw r19` (0) @0x823EB344 on the first post
    // and `stw r14` (1) @0x823EB37C on the second.
    // FLAG: the field NAME is role-derived from those two values only -- nothing in b5-decomp
    // consumes id 269, so there is no consumer to arbitrate it. The COPY is exact.
    struct NetworkSplashWire269
    {
        s32 miSplashState;                     // +0x00
        s32 GetEventType() const { return 269; }
    };
    CGS_STATIC_ASSERT((sizeof(NetworkSplashWire269) == 4), "id 269 size 4");

    // id 279 size 2 -- AddGuiEvent<BrnGui::GuiEventNetworkShowFreeBurnIntro> @0x823D2AB0
    // (`AddEvent(q, ev, 279, 2)`). Store map: `addi r4, r1, var_3568` is the record base and the
    // arm writes `stb r19, var_3567` + `stb r19, var_3568` @0x823EB35C/0x823EB364 -- both bytes 0.
    // FLAG: no consumer in b5-decomp, so the two bytes stay unnamed.
    struct NetworkShowFreeBurnIntroWire279
    {
        u8 maZero[2];                          // +0x00..+0x01
        s32 GetEventType() const { return 279; }
    };
    CGS_STATIC_ASSERT((sizeof(NetworkShowFreeBurnIntroWire279) == 2), "id 279 size 2");

    // id 234 size 4 -- AddGuiEvent<GuiEventUpdateEventCountdown> @0x823D25A8.
    // Store map: `lwz r11,0(r31); stw r11, var_35D8` @0x823EAD50.
    struct UpdateEventCountdownWire234
    {
        s32 miCountdownDisplay;                // +0x00
        s32 GetEventType() const { return 234; }
    };
    CGS_STATIC_ASSERT((sizeof(UpdateEventCountdownWire234) == 4),
                  "X360 AddGuiEvent<GuiEventUpdateEventCountdown> posts 4 bytes (id 234)");

    // id 81 size 4 -- AddGuiEvent<GuiCarSelectStartEvent> @0x823D1690, whose tail is literally
    // `li r6,4 / li r5,0x51 / bl VariableEventQueue<32768,16>::AddEvent`.
    // Store map: `lwz r11,0(r31); stw r11, var_35D8` @0x823EA700 (the case-75 arm).
    // ⚠️ NOT the shell in BrnGuiDemangledEventTypes.h: that header cannot be included from this
    // TU (the C2011 documented in the banner), and its `GuiCarSelectStartEvent { u8 maData[4]; }`
    // is the right size but this record needs its own GetEventType() for PushGuiEvent anyway.
    struct CarSelectStartWire81
    {
        s32 miCarSelectFlow;                   // +0x00
        s32 GetEventType() const { return 81; }
    };
    CGS_STATIC_ASSERT((sizeof(CarSelectStartWire81) == 4),
                  "X360 AddGuiEvent<GuiCarSelectStartEvent> posts 4 bytes (id 81)");

    // id 166 size 8 -- AddGuiEvent<GuiEventEnterEventStartLocation> @0x823D1E78.
    // Store map: the case-44 arm's var_3508-based frame @0x823EA948..0x823EA970.
    struct EnterEventStartLocationWire166
    {
        u32 muLeavingStartLocation;            // +0x00  0 when the action reports "in region", else 1
        u16 mu16StartLocationId;               // +0x04  the action's id, or 0xFFFF when leaving
        u8  maPad06[2];                        // +0x06

        s32 GetEventType() const { return 166; }
    };
    CGS_STATIC_ASSERT((sizeof(EnterEventStartLocationWire166) == 8),
                  "X360 AddGuiEvent<GuiEventEnterEventStartLocation> posts 8 bytes (id 166)");

    // id 321 size 1 -- AddGuiEvent<GuiEventFinishedModeResults> @0x823D24F0.
    // The case-38 arm posts `addi r4, r1, var_3547` with NO preceding store: the console sends
    // one uninitialised stack byte. Reproduced as a zeroed byte (the host must not post
    // uninitialised memory; the consumer never reads it).
    struct FinishedModeResultsWire321
    {
        u8 mu8Unused;                          // +0x00
        s32 GetEventType() const { return 321; }
    };
    CGS_STATIC_ASSERT((sizeof(FinishedModeResultsWire321) == 1),
                  "X360 AddGuiEvent<GuiEventFinishedModeResults> posts 1 byte (id 321)");

    // id 322 size 12 -- AddGuiEvent<GuiEventStopMode> @0x823D2380.
    // Store map: the case-39 arm's var_3388-based frame @0x823EABCC..0x823EABFC.
    struct StopModeWire322
    {
        s32 meGameModeType;                    // +0x00  action+0x00
        s32 miField04;                         // +0x04  action+0x0C   FLAG: unnamed on both ends
        u8  mu8Field08;                        // +0x08  action+0x11   FLAG
        u8  mu8Field09;                        // +0x09  action+0x12   FLAG
        u8  mu8Field0A;                        // +0x0A  action+0x13   FLAG
        u8  maPad0B[1];                        // +0x0B

        s32 GetEventType() const { return 322; }
    };
    CGS_STATIC_ASSERT((sizeof(StopModeWire322) == 12),
                  "X360 AddGuiEvent<GuiEventStopMode> posts 12 bytes (id 322)");

    // id 307 size 8 -- AddGuiEvent<GuiEventMedalUpdate> @0x823D1A28.
    // Store map: four halfword copies @0x823EA7DC..0x823EA804.
    struct MedalUpdateWire307
    {
        s16 mi16TotalWins;                     // +0x00  action+0x00
        s16 mi16Field02;                       // +0x02  action+0x02   FLAG
        s16 mi16Field04;                       // +0x04  action+0x04   FLAG
        s16 mi16WinsToNextRank;                // +0x06  action+0x06

        s32 GetEventType() const { return 307; }
    };
    CGS_STATIC_ASSERT((sizeof(MedalUpdateWire307) == 8),
                  "X360 AddGuiEvent<GuiEventMedalUpdate> posts 8 bytes (id 307)");

    // id 320 size 1 -- AddGuiEvent<GuiEventTriggerOnlinePostEvent> @0x823D1F30.
    struct TriggerOnlinePostEventWire320
    {
        u8 mu8Flag;                            // +0x00  action+0xE1
        s32 GetEventType() const { return 320; }
    };
    CGS_STATIC_ASSERT((sizeof(TriggerOnlinePostEventWire320) == 1), "id 320 size 1");

    // id 291 size 1 -- AddGuiEvent<CgsGui::GuiEvent<291>> @0x823D1FE8 (`AddEvent(q, ev, 291, 1)`).
    // The OFFLINE post-event request. The mangled name carries no BrnGui type name (the console
    // posts the bare CgsGui::GuiEvent<291> tag), and the case-37 arm posts it from a 1-byte
    // scratch slot it never writes -- so there is no payload to model.
    struct PostEventRequestWire291
    {
        u8 mu8Unused;                          // +0x00
        s32 GetEventType() const { return 291; }
    };
    CGS_STATIC_ASSERT((sizeof(PostEventRequestWire291) == 1), "id 291 size 1");

    // id 356 size 1 -- AddGuiEvent<GuiAutosaveRequestEvent> @0x823D03E0. Posted with r19 == 0.
    struct AutosaveRequestWire356
    {
        u8 mu8Zero;                            // +0x00
        s32 GetEventType() const { return 356; }
    };
    CGS_STATIC_ASSERT((sizeof(AutosaveRequestWire356) == 1), "id 356 size 1");

    // id 96 size 1 -- AddGuiEvent<GuiSetEasyDriveNotAllowedEvent> @0x823D2158. Posted with r19 == 0.
    struct SetEasyDriveNotAllowedWire96
    {
        u8 mu8Zero;                            // +0x00
        s32 GetEventType() const { return 96; }
    };
    CGS_STATIC_ASSERT((sizeof(SetEasyDriveNotAllowedWire96) == 1), "id 96 size 1");

    // id 536 size 2 -- AddGuiEvent<GuiEventTickerClearMessages> @0x823D2718.
    // The case-23 arm writes both bytes 0 (`stb r19, var_3564` / `stb r19, var_3563`).
    struct TickerClearMessagesWire536
    {
        u8 maZero[2];                          // +0x00..+0x01
        s32 GetEventType() const { return 536; }
    };
    CGS_STATIC_ASSERT((sizeof(TickerClearMessagesWire536) == 2), "id 536 size 2");

    // id 289 size 192 -- AddGuiEvent<GuiEventOfflinePostEvent> @0x823D20A0.
    // Store map: the case-37 arm's var_2D10-based frame @0x823EAA74..0x823EAB3C. Every member
    // below is a store the arm emits; the gaps are never written by the arm and are explicit
    // padding, so the record is byte-exact at 192 without inventing fields.
    struct OfflinePostEventWire289
    {
        u64 mu64Field00;                       // +0x00  action+0xC8      FLAG
        u64 mu64Field08;                       // +0x08  action+0x40, only when action->mbHasField40
        u8  maPad10[8];                        // +0x10..+0x17 never written by the arm
        s32 meGameModeType;                    // +0x18  action+0x00
        s32 miField1C;                         // +0x1C  action+0x08      FLAG
        f32 mfField20;                         // +0x20  action+0x0C      FLAG
        s32 miField24;                         // +0x24  action+0x10      FLAG
        s32 miField28;                         // +0x28  action+0x14      FLAG
        f32 mfField2C;                         // +0x2C  action+0x18      FLAG
        u8  maBlock30[0x70];                   // +0x30  memcpy 0x70 from action+0x58,
                                               //        only when action->mbHasBlock58
        s32 miFieldA0;                         // +0xA0  action+0x48      FLAG
        s32 miFieldA4;                         // +0xA4  action+0x4C      FLAG
        u8  maPadA8[8];                        // +0xA8..+0xAF never written by the arm
        u8  mu8FieldB0;                        // +0xB0  (u8)action+0x04  FLAG
        u8  mu8FieldB1;                        // +0xB1  action+0xE0      FLAG
        u8  mu8FieldB2;                        // +0xB2  action+0xE2      FLAG
        u8  mu8FieldB3;                        // +0xB3  action+0xE1      FLAG
        u8  mu8FieldB4;                        // +0xB4  action+0xE3      FLAG
        u8  mu8FieldB5;                        // +0xB5  action+0xDA      FLAG
        u8  mu8FieldB6;                        // +0xB6  action+0xDB      FLAG
        u8  mbHasField08;                      // +0xB7  action+0xDE (the +0x08 copy's own gate)
        u8  mu8FieldB8;                        // +0xB8  action+0xDC      FLAG
        u8  maPadB9[7];                        // +0xB9..+0xBF tail padding to the attested 192

        s32 GetEventType() const { return 289; }
    };
    CGS_STATIC_ASSERT((sizeof(OfflinePostEventWire289) == 192),
                  "X360 AddGuiEvent<GuiEventOfflinePostEvent> posts 192 bytes (id 289)");

    // =====================================================================================
    // ⭐⭐ [road-rage wave 2026-09-02] THE THREE ROAD RAGE TRANSLATE ARMS -- wire records.
    // Same TU-local precedent as every record above (BrnGuiDemangledEventTypes.h is a C2011
    // from this TU, and its 168/348/427 shells are opaque `u8 maData[N]` anyway). Every id
    // below is the AddGuiEvent<T> instantiation's own `li r5`, every size its `li r6`; the
    // field NAMES are the DWARF's (BrnGuiEventTypeDefs.h:4355 / :2546-2548 / :4874), and the
    // field OFFSETS are the arms' own store maps. GAME ACTION ids are the X360 jump-table
    // cases (103 / 255 / 205 of jpt_823EA1F0) -- their own id space, NOT the GUI ids.
    // =====================================================================================

    // id 168 size 1 -- AddGuiEvent<GuiEventPlayerReachedRoadRageTarget> @0x823D1DC0
    // (`li r6, 1` @0x823D1E5C / `li r5, 0xA8` @0x823D1E60). The case-103 arm posts
    // `addi r4, r1, var_354B` @0x823EA938 with NO preceding store -- one uninitialised stack
    // byte, exactly like FinishedModeResultsWire321. Reproduced as a zeroed byte; the consumer
    // (HudMessageAnalyzer case 168, BrnGuiHudMessageAnalyzer_wB_12.cpp:114) reads only the id.
    // DWARF: `struct GuiEventPlayerReachedRoadRageTarget : public GuiEvent<166> {}` -- empty.
    struct PlayerReachedRoadRageTargetWire168
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 168; }
    };
    CGS_STATIC_ASSERT((sizeof(PlayerReachedRoadRageTargetWire168) == 1),
                  "X360 AddGuiEvent<GuiEventPlayerReachedRoadRageTarget> posts 1 byte (id 168)");

    // id 427 size 4 -- AddGuiEvent<GuiEventRoadRageTimeExtended> @0x823D64E8
    // (`li r6, 4` @0x823D6584 / `li r5, 0x1AB` @0x823D6588).
    // Store map: the case-255 arm, `lwz r11, 0(r31) ; stw r11, var_35D8` @0x823EC494/0x823EC4A4.
    // DWARF: `uint32_t muExtensiontime` (BrnGuiEventTypeDefs.h:4874).
    struct RoadRageTimeExtendedWire427
    {
        u32 muExtensiontime;                   // +0x00  action+0x00
        s32 GetEventType() const { return 427; }
    };
    CGS_STATIC_ASSERT((sizeof(RoadRageTimeExtendedWire427) == 4),
                  "X360 AddGuiEvent<GuiEventRoadRageTimeExtended> posts 4 bytes (id 427)");

    // id 348 size 8 -- AddGuiEvent<GuiEventRoadRagePlayerDamage> @0x823D2210
    // (`li r6, 8` @0x823D22AC / `li r5, 0x15C` @0x823D22B0).
    // Store map: the case-205 arm's var_34E0-based frame @0x823EAB7C..0x823EAB9C:
    //   lfs f0,  0(r31) -> stfs var_34E0     (+0x00)
    //   lbz r11, 4(r31) -> stb  var_34DC     (+0x04)
    //   lbz r11, 5(r31) -> stb  var_34DB     (+0x05)
    // DWARF: mfHowCloseToTotalled / mbOneMoreCrashToTotalled / mbPlayerTotalled
    // (BrnGuiEventTypeDefs.h:2546-2548). +0x06..+0x07 are never written by the arm; zeroed
    // here so the host never posts uninitialised bytes. The mounted consumer
    // (HudMessageAnalyzer case 348, BrnGuiHudMessageAnalyzer_wB_12.cpp:262) reads the byte at
    // record+4 -- mbOneMoreCrashToTotalled -- which is exactly where this record puts it.
    struct RoadRagePlayerDamageWire348
    {
        f32 mfHowCloseToTotalled;              // +0x00  action+0x00
        u8  mbOneMoreCrashToTotalled;          // +0x04  action+0x04
        u8  mbPlayerTotalled;                  // +0x05  action+0x05
        u8  maPad06[2];                        // +0x06..+0x07 never written by the arm

        s32 GetEventType() const { return 348; }
    };
    CGS_STATIC_ASSERT((sizeof(RoadRagePlayerDamageWire348) == 8),
                  "X360 AddGuiEvent<GuiEventRoadRagePlayerDamage> posts 8 bytes (id 348)");

    // [stale as of 2026-09-02, verify V2: BrnGameActions.h NOW defines E_ACTION_ROAD_RAGE_PLAYER_DAMAGE (205),
    //  E_ACTION_HUD_MESSAGE_ROAD_RAGE_TIME_EXTENSION (255) and their records; the *Mirror structs below
    //  duplicate those shapes and should collapse onto the header types in a follow-up.]
    // The two road-rage ACTION payloads the arms above read. BrnGameActions.h carries neither
    // enumerator nor struct for X360 actions 205 / 255 yet (the scorer lane owns that header
    // this wave -- see the header request in the wave report), so the DWARF shapes are
    // mirrored here, names and all, and read by NAME:
    //   RoadRagePlayerDamageAction          DWARF BrnGameActions.h:2453-2457 (8 bytes)
    //   HUDMessageRoadRageTimeExtensionAction DWARF BrnGameActions.h:4616      (4 bytes)
    // Producers (X360): ScoringSystem::OnRoadRagePlayerCrashed @0x823444B0 posts 205
    // (`li r5, 0xCD` @0x823445BC); RoadRageModeScoring::IncrementPlayerNumTakedowns @0x823445D0
    // posts 103 (`li r5, 0x67` @0x82344644, size 1) and 255 twice (`li r5, 0xFF` @0x823446BC /
    // @0x823446D4, size 4, the word being `lhz r11, 0xA(r31)` -- the extension seconds).
    struct RoadRagePlayerDamageActionMirror
    {
        f32 mfHowCloseToTotalled;              // +0x00
        u8  mbOneMoreCrashToTotalled;          // +0x04
        u8  mbPlayerTotalled;                  // +0x05
        u8  maPad06[2];                        // +0x06
    };
    CGS_STATIC_ASSERT((sizeof(RoadRagePlayerDamageActionMirror) == 8), "X360 action 205 size 8");

    struct HUDMessageRoadRageTimeExtensionActionMirror
    {
        u32 muExtensionTime;                   // +0x00
    };
    CGS_STATIC_ASSERT((sizeof(HUDMessageRoadRageTimeExtensionActionMirror) == 4), "X360 action 255 size 4");

    // =====================================================================================
    // [race wave, lane BR] RACE / BURNING ROUTE / POST-EVENT ARMS -- wire records.
    // Same TU-local precedent as every record above: the shells in BrnGuiDemangledEventTypes.h
    // cannot be included from this TU, so each record is rebuilt from the arm's store map at
    // the size its AddGuiEvent<T> instantiation passes. Types that have a flat home in
    // BrnGuiEventTypeDefs.h (316, 375, 425) are used from there instead.
    // =====================================================================================

    // id 153 size 1 -- AddGuiEvent<CgsGui::GuiEvent<153>> (id and size immediates 0x99 / 1). The
    // WRONG WAY message. The case-251 arm posts a stack slot it never writes; the consumer
    // (HudMessageAnalyzer case 153 -> "WrongWay") reads only the id.
    struct WrongWayWire153
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 153; }
    };
    CGS_STATIC_ASSERT((sizeof(WrongWayWire153) == 1), "id 153 size 1");

    // id 550 size 1 -- AddGuiEvent<GuiEventTimeUp> (immediates 0x226 / 1). The case-262
    // arm copies the action's one byte (a byte load at +0x00, a byte store). The consumer
    // (HudMessageAnalyzer case 550) picks "TimeUpPos" when it is set and "TimeUpNeg" when not.
    // FLAG: the field name is role-derived from the producer (ModeManager::UpdateCurrentMode's
    // succeeded byte) and that consumer; no declaration names it.
    struct TimeUpWire550
    {
        u8 mbSucceeded;                        // +0x00  action+0x00
        s32 GetEventType() const { return 550; }
    };
    CGS_STATIC_ASSERT((sizeof(TimeUpWire550) == 1), "id 550 size 1");

    // id 305 size 4 -- AddGuiEvent<GuiEventAllOfTypeComplete> (immediates 0x131 / 4).
    // The case-206 arm copies one word (a word load at +0x00, a word store); the consumer
    // (HudMessageAnalyzer case 305) reads it as the event type whose events are all won.
    struct AllOfTypeCompleteWire305
    {
        s32 meGameEventType;                   // +0x00  AllEventTypeWonAction::leGameEventType
        s32 GetEventType() const { return 305; }
    };
    CGS_STATIC_ASSERT((sizeof(AllOfTypeCompleteWire305) == 4), "id 305 size 4");

    // id 306 size 1 -- AddGuiEvent<GuiEventAllOfRivalsShutdown> (immediates 0x132 / 1).
    // The case-210 arm posts a stack byte it never stores to; the consumer
    // (HudMessageAnalyzer case 306 -> "EvryRivShut") reads only the id.
    struct AllOfRivalsShutdownWire306
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 306; }
    };
    CGS_STATIC_ASSERT((sizeof(AllOfRivalsShutdownWire306) == 1), "id 306 size 1");

    // ids 309 / 310 size 2 -- AddGuiEvent<GuiEventGameCompleted> (immediates 0x135 / 2)
    // and AddGuiEvent<GuiEventGameCompletedOnline> (immediates 0x136 / 2). The case-208
    // arm copies the action's two bytes at +0x04 / +0x05 into both records the same way.
    // FLAG: the field names are the producer record's (GameCompletionResultsAction); the GUI
    // side declares no members.
    struct GameCompletedWire309
    {
        u8 mbGameComplete;                     // +0x00  action+0x04
        u8 mbCompletionAlreadyRecorded;        // +0x01  action+0x05
        s32 GetEventType() const { return 309; }
    };
    CGS_STATIC_ASSERT((sizeof(GameCompletedWire309) == 2), "id 309 size 2");

    struct GameCompletedOnlineWire310
    {
        u8 mbGameComplete;                     // +0x00  action+0x04
        u8 mbCompletionAlreadyRecorded;        // +0x01  action+0x05
        s32 GetEventType() const { return 310; }
    };
    CGS_STATIC_ASSERT((sizeof(GameCompletedOnlineWire310) == 2), "id 310 size 2");

    // ---------------------------------------------------------------------------------------
    // GuiEventRunFsm posts through the raw queue (24-byte controller record, not a PushGuiEvent
    // payload); GetEventType() is 144, corrected from the dead PS3 id 142 on 2026-08-27.
    // ---------------------------------------------------------------------------------------
    void PostRunFsm(CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
                    const char* lpacFsmName, const char* lpacInitialState,
                    BrnGui::EHUDFSMs leFsmToRun, BrnGui::GuiFlow leFlowToUse)
    {
        if (lpGuiInput == 0)
        {
            return;
        }
        BrnGui::GuiEventRunFsm lEvent;
        lEvent.mFsmId          = CgsIDCompress(lpacFsmName);
        lEvent.mInitialStateId = (lpacInitialState != 0) ? CgsIDCompress(lpacInitialState)
                                                         : static_cast<CgsID>(0);
        lEvent.meFsmToRun      = leFsmToRun;
        lEvent.meFlowToUse     = leFlowToUse;
        lpGuiInput->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEvent), lEvent.GetEventType(),
            static_cast<s32>(sizeof(lEvent)));
    }
}   // anonymous namespace

    // =========================================================================
    // [evt-flow E1] The event-flow arms of BrnGameModule::TranslateGameActionsToGuiEvents
    // @0x823E9CE0. `liActionType` / `lpAction` are the queue walk's current record (the caller's
    // GetFirstEvent / GetNextEvent pair, @0x823E9D88); `lpGuiInput` is the console's r20 and
    // `lpGameStateOutput` its r31-at-entry.
    //
    // Returns true when an arm consumed the action, so the caller's `default:` can keep falling
    // through for everything else. On the console this IS the default arm's continuation: every
    // case below ends with `b def_823EA1F0`, the shared "advance the queue" tail.
    //
    // The two entry asserts (GameBridgeGameStateToX.cpp:754/755) stay with the caller -- they
    // guard the whole walk, not an arm.
    // =========================================================================
    bool TranslateEventFlowGameActionToGuiEvent(
        s32 liActionType,
        const CgsModule::Event* lpAction,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        if (lpAction == 0 || lpGuiInput == 0)
        {
            return false;
        }
        (void)lpGameStateOutput;   // read only by the parked case-37 asserts -- see that arm

        switch (liActionType)
        {
        // ARTIST junkyard unlock transport. These events connect the camera's
        // streaming wait, unlock screen, car-description ticker and return to selection.
        case 2: // CarUnlockAction -> GuiCarUnlockEvent, id 76, CgsID payload.
        {
            struct UnlockWire { CgsID mCar; s32 GetEventType() const { return 76; } };
            const CgsID lCar = *reinterpret_cast<const CgsID*>(lpAction);
            CGS_ASSERT(lCar != 0, "Invalid car unlock id");
            UnlockWire lEvent;
            lEvent.mCar = lCar;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }
        case 62: // NewCarUnlockedAction -> GuiCarUnlockNewCarEvent, id 73.
        {
            struct NewCarWire { CgsID mCar; s32 GetEventType() const { return 73; } };
            NewCarWire lEvent;
            lEvent.mCar = *reinterpret_cast<const CgsID*>(lpAction);
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }
        case 63: // CarUnlockEndAction -> unlock screen ADVANCE.
        {
            struct UnlockEndWire { u8 mSignal; s32 GetEventType() const { return 74; } };
            UnlockEndWire lEvent;
            lEvent.mSignal = 0;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }
        case 73: // Transition-in begins with unlocks -> InGame TO_CUNLOCK.
        {
            const u8* lFlags = reinterpret_cast<const u8*>(lpAction);
            if (lFlags[0] && lFlags[1])
            {
                struct UnlockStartWire { u8 mSignal; s32 GetEventType() const { return 75; } };
                UnlockStartWire lEvent;
                lEvent.mSignal = 0;
                PushGuiEvent(lEvent, lpGuiInput);
            }
            return true;
        }

        // ---- 201  E_ACTION_EVENT_AT_JUNCTION_AVAILABLE (40 bytes) --------------------------
        // *** THE VISIBLE ORACLE. @0x823EA810..0x823EA874, instruction for instruction: eleven
        // loads off the action record, eleven stores into a 32-byte frame at var_35C0, then
        // AddGuiEvent<GuiEventJunctionInfo> (id 311, size 32 -- @0x823D1AE0).
        //   var_35C0+0x00 <- ld  0x10(r31)   -> mSpecialEventCarId
        //   var_35C0+0x08 <- lwz 0x18(r31)   -> meGameModeType
        //   var_35C0+0x0C <- lbz 0x1C(r31)   -> mi8Difficulty
        //   var_35C0+0x0D <- lbz 0x1D(r31)   -> mi8MedalAchieved
        //   var_35C0+0x10 <- lwz 0x04(r31)   -> miEventID
        //   var_35C0+0x14 <- lbz 0x1F(r31)   -> mbCanEnterEvent
        //   var_35C0+0x15 <- lbz 0x20(r31)   -> mbEventUnlocked
        //   var_35C0+0x16 <- lbz 0x1E(r31)   -> mbOnEntry
        //   var_35C0+0x17 <- lbz 0x21(r31)   -> mbSpecificCarEventValid
        //   var_35C0+0x18 <- lbz 0x22(r31)   -> mbIsNewlyDiscovered
        //   var_35C0+0x19 <- lbz 0x23(r31)   -> mbIsAutoUnlockedChallenge
        // The HUD, odometer and SatNav read the same 32-byte GuiEventJunctionInfo payload.
        case BrnGameState::GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE:
        {
            const BrnGameState::GameStateModuleIO::JunctionInfoAction* lpJunction =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::JunctionInfoAction*>(lpAction);

            BrnGui::GuiEventJunctionInfo lEvent;
            lEvent.mSpecialEventCarId        = lpJunction->mSpecialEventCarId;
            lEvent.meGameModeType            = lpJunction->meGameModeType;
            lEvent.mi8Difficulty             = lpJunction->mi8Difficulty;
            lEvent.mi8MedalAchieved          = lpJunction->mi8MedalAchieved;
            lEvent.miEventID                 = static_cast<s32>(lpJunction->muEventJunctionID);
            lEvent.mbCanEnterEvent           = lpJunction->mbCanEnterEvent;
            lEvent.mbEventUnlocked           = lpJunction->mbEventUnlocked;
            lEvent.mbOnEntry                 = lpJunction->mbOnEntry;
            lEvent.mbSpecificCarEventValid   = lpJunction->mbSpecificCarEventValid;
            lEvent.mbIsNewlyDiscovered       = lpJunction->mbIsNewlyDiscovered;
            lEvent.mbIsAutoUnlockedChallenge = lpJunction->mbIsAutoUnlockedChallenge;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 23  E_ACTION_PREPARE_FOR_MODE (2272 bytes) -----------------------------------
        // @0x823EAD80..0x823EB070. The arm that both DESCRIBES the event to the GUI and, for an
        // offline mode, tells the flow controller to run the EVENT FSM.
        //
        // Body order, transcribed:
        //   1. `lwz r11,0(r31); cmpwi 0; beq; cmpwi 1; bne default` -- drop unless this is the
        //      first prepare-for-mode of the (possibly split) pair.
        //   2. `addi r25, r31, 0x30` + a null assert on it ("lpParams",
        //      GameBridgeGameStateToX.cpp:1258) -- r25 is the embedded GameModeParams.
        //   3. fill the 152-byte GuiEventPrepareForModeStart frame (store map on
        //      PrepareForModeStartWire93).
        //   4. mbIsOnline fork: online takes miNumNetworkPlayers, offline takes miNumRivals; the
        //      online branch additionally posts a GuiOverlayWaitFinishRequest("GMStrOffline")
        //      when the mode is the lobby/showtime pair.
        //   5. showtime pair -> post GuiEventTickerClearMessages (536).
        //   6. `GetCheckpointCount() <= 16` assert ("Exceded landmark count", :1313) + the two
        //      per-checkpoint arrays.
        //   7. AddGuiEvent<GuiEventPrepareForModeStart>.
        //   8. RUN THE EVENT FSM: post GuiEventRunFsm{ CgsIDCompress("BRNEVENTFSM"), 0,
        //      E_GUI_HUD_EVENT(2), E_GUIFLOW_HUD(1) } when the mode is offline (< 10), or when it
        //      is the online lobby/showtime pair AND neither mbStartingFreeburnDueToPlayerJoin
        //      (action+0x8D2) nor mbFinishedOnlineEvent (action+0x8D1) is set. "BRNEVENTFSM" is
        //      the image literal at var_3524 (loaded @0x823E9DC8).
        //   9. always post GuiSetEasyDriveNotAllowedEvent (96) with a zero byte.
        //
        // FLAG -- payload +0x91 (mbOnlineLobbyTransition) is transcribed exactly but its NAME is
        // a guess from its inputs: the console computes it as
        // `action->IsMovingBetweenOnlineLobbyModes() && IsOnlineLobbyOrShowtimeMode(mode)`
        // (@0x823EAED4..0x823EAF1C). The predicate is proven; the name is not attested.
        case BrnGameState::GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE:
        {
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPrepare =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(lpAction);

            if (!lpPrepare->IsFirstPrepareForMode())
            {
                return true;      // consumed: the console's `beq cr6, def_823EA1F0`
            }

            const BrnGameState::GameModeParams* lpParams = lpPrepare->GetGameModeParams();
            CGS_ASSERT(lpParams != 0, "lpParams");                         // :1258
            if (lpParams == 0)
            {
                return true;
            }

            const s32 liGameModeType = static_cast<s32>(lpParams->GetGameModeType());

            PrepareForModeStartWire93 lEvent;
            std::memset(&lEvent, 0, sizeof(lEvent));
            lEvent.mPursuedCarId               = lpParams->mPursuedCarID;
            lEvent.miCurrentRound              = lpPrepare->GetCurrentRound();
            lEvent.meGameModeType              = liGameModeType;
            lEvent.muEventJunctionID           = lpParams->muEventJunctionID;
            lEvent.muJunctionID                = lpParams->muJunctionID;
            lEvent.mfNeedForBronze             = lpParams->mfNeedForBronze;      // params+0x60
            lEvent.mfNeedForSilver             = lpParams->mfNeedForSilver;      // params+0x64
            lEvent.mfNeedForGold               = lpParams->mfNeedForGold;        // params+0x68
            lEvent.mfModeTimeLimit             = lpParams->mfModeTimeLimit;      // params+0x6C
            lEvent.miPursuitRivalTotalDamage   = lpParams->miPursuitRivalTotalDamage;
            lEvent.mu8DifficultyLevel          = lpParams->muDifficultyLevel;
            lEvent.mu8RoadRageThreshold        = static_cast<u8>(lpParams->miRoadRageThreshold);
            lEvent.mbIsOnline                  = lpParams->mbIsOnline ? 1u : 0u;
            const s32 checkpointCount = lpParams->GetCheckpointCount();
            CGS_ASSERT(checkpointCount <= 16, "Exceded landmark count");
            lEvent.mu8CheckpointCount = static_cast<u8>(checkpointCount);
            for (s32 i = 0; i < checkpointCount; ++i)
            {
                lEvent.mau16CheckpointLandmark[i] = static_cast<u16>(lpParams->GetCheckpointData(i)->GetLandmarkIndex());
                lEvent.maiCheckpointDistrict[i] = lpParams->GetCheckpointData(i)->GetDistrict();
            }

            if (lpParams->mbIsOnline)
            {
                lEvent.mu8CarCount = static_cast<u8>(lpParams->miNumNetworkPlayers);

                // @0x823EAE40..0x823EAE88 -- the online branch's overlay hand-off. The overlay
                // name is the image literal at r23 ("GMStrOffline", loaded @0x823EA114).
                if (IsOnlineLobbyOrShowtimeMode(liGameModeType))
                {
                    BrnGui::GuiOverlayWaitFinishRequest lWaitRequest;
                    lWaitRequest.Construct("GMStrOffline");
                    PushGuiEvent(lWaitRequest, lpGuiInput);
                }
            }
            else
            {
                lEvent.mu8CarCount = static_cast<u8>(lpParams->miNumRivals);
            }

            // @0x823EAE98..0x823EAED0 -- both showtime modes clear the ticker first.
            if (IsShowtimeMode(liGameModeType))
            {
                TickerClearMessagesWire536 lClear;
                lClear.maZero[0] = 0;
                lClear.maZero[1] = 0;
                PushGuiEvent(lClear, lpGuiInput);
            }

            lEvent.mbOnlineLobbyTransition =
                (lpPrepare->IsMovingBetweenOnlineLobbyModes() &&
                 IsOnlineLobbyOrShowtimeMode(liGameModeType)) ? 1u : 0u;

            PushGuiEvent(lEvent, lpGuiInput);

            // @0x823EAFE4..0x823EB058 -- THE FSM HOP. Offline modes always run it; the online
            // lobby/showtime pair runs it only when the mode was not entered by a joining player
            // and did not just finish an online event.
            bool lbRunEventFsm =
                (liGameModeType < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT);
            if (!lbRunEventFsm && IsOnlineLobbyOrShowtimeMode(liGameModeType))
            {
                lbRunEventFsm = !lpPrepare->GetStartingFreeburnLobbyDueToPlayerJoin() &&
                                !lpPrepare->GetFinishedOnlineEvent();
            }
            if (lbRunEventFsm)
            {
                PostRunFsm(lpGuiInput, "BRNEVENTFSM", 0,
                           BrnGui::E_GUI_HUD_EVENT, BrnGui::E_GUIFLOW_HUD);
            }

            // @0x823EB05C..0x823EB06C -- unconditional tail.
            SetEasyDriveNotAllowedWire96 lEasyDrive;
            lEasyDrive.mu8Zero = 0;
            PushGuiEvent(lEasyDrive, lpGuiInput);

            return true;
        }

        // ARTIST823EB14C..B300: all fly-by messages, then the original online tail.
        case BrnGameState::GameStateModuleIO::E_ACTION_SET_LANDMARK_RACES:
        {
            const BrnGameState::GameStateModuleIO::SetLandmarkRacesAction* lpRaces =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::SetLandmarkRacesAction*>(lpAction);
            BrnGui::GuiEventSetAvailablePresetRaces lEvent;
            // ARTIST823EB56C..B590 copies count*120, then sets the signed GUI count.
            std::memcpy(lEvent.maPresetRaces, lpRaces->maRaces,
                        static_cast<size_t>(lpRaces->muNumRaces) * sizeof(BrnProgression::Race));
            lEvent.miNumPresetRaces = static_cast<s32>(lpRaces->muNumRaces);
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        case BrnGameState::GameStateModuleIO::E_ACTION_START_MODE_INTRO:
        {
            namespace GsmIO = BrnGameState::GameStateModuleIO;
            const GsmIO::StartModeIntroAction* lpIntro =
                reinterpret_cast<const GsmIO::StartModeIntroAction*>(lpAction);
            BrnGui::GuiEventPreRaceMessages lMessages;
            lMessages.Construct();
            for (s32 liCar = 0; liCar < lpIntro->mFlybyData.miNumberOfCars; ++liCar)
            {
                // The original accessor is non-const; this bridge only reads its result.
                const GsmIO::FlybyRivalData* lpRival = const_cast<GsmIO::FlybyData&>(lpIntro->mFlybyData)
                    .GetFlybyRivalData(liCar);
                switch (static_cast<s32>(lpRival->meMessageStyle))
                {
                case 0:
                    lMessages.AddMessage(BrnGui::GuiEventPreRaceMessages::E_RELATIONSHIP_GOOD,
                                         lpRival->mPlayerName.GetPlayerName());
                    break;
                case 1:
                    lMessages.AddMessage(BrnGui::GuiEventPreRaceMessages::E_RELATIONSHIP_NEUTRAL,
                                         lpRival->mPlayerName.GetPlayerName());
                    break;
                case 2:
                    lMessages.AddMessage(BrnGui::GuiEventPreRaceMessages::E_RELATIONSHIP_BAD,
                                         lpRival->mPlayerName.GetPlayerName());
                    break;
                default:
                    CGS_FAIL("Invalid message type in BrnGameModule::TranslateGameActionsToGuiEvents");
                    break;
                }
                for (s32 liString = 0; liString < lpRival->miNumberOfMessages; ++liString)
                    lMessages.AddStringToMessage(liCar,liString,lpRival->maacMessageIDs[liString],
                        lpRival->maiNumberOfParameters[liString],lpRival->maacMessageParameter[liString]);
            }
            PushGuiEvent(lMessages,lpGuiInput);   // GUI159, all1744 payload bytes, even count0.
            if (IsOnlineLobbyOrShowtimeMode(static_cast<s32>(lpIntro->meGameMode)))
            {
                BrnGui::GuiOverlayWaitFinishRequest lWait;
                lWait.Construct("CNOnlEntGame");
                PushGuiEvent(lWait,lpGuiInput);
                if (!lpIntro->mbFinishedOnlineLobbyMode)
                {
                    NetworkShowFreeBurnIntroWire279 lIntro;
                    lIntro.maZero[0] = lpIntro->mbFinishedOnlineEvent ? 1 : 0;
                    lIntro.maZero[1] = 1;
                    PushGuiEvent(lIntro,lpGuiInput);
                }
            }
            return true;
        }

        // ---- 30  E_ACTION_STOP_MODE_INTRO (8 bytes) ----------------------------------------
        // ⭐⭐ THE OTHER HALF OF CASE 23: 23 puts the GUI INTO the event FSM's PRE_FLY_BY state,
        // and THIS arm is the only thing in the whole image that gets it back out again.
        //
        // @0x823EB304..0x823EB38C, instruction for instruction (r19 == 0, r14 == 1, r31 == the
        // action record):
        //   lwz  r11, 0(r31)            -- action->meGameMode
        //   cmpwi 0xF / beq ; cmpwi 0x10 / bne   -- r11 = (mode == 15 || mode == 16)
        //   cmplwi 0 / bne loc_823EB340          -- ONLINE lobby/showtime takes the far branch
        //   [offline]  addi r4, r1, var_3543 (a 1-byte slot, NEVER WRITTEN)
        //              bl AddGuiEvent<CgsGui::GuiEvent<164>>          ; id 164 size 1
        //              b  default
        //   [online]   stw r19, var_35D8 ; bl AddGuiEvent<GuiEventNetworkSplashEvent>      (0)
        //              stb r19, var_3567 ; stb r19, var_3568
        //              bl AddGuiEvent<GuiEventNetworkShowFreeBurnIntro>                    (0,0)
        //              lbz r11, 4(r31)   ; beq default                 -- mbMovingBetweenLobbyModes
        //              stw r14, var_35D8 ; bl AddGuiEvent<GuiEventNetworkSplashEvent>      (1)
        //
        // THE WHOLE CONSOLE CHAIN THIS CLOSES, producer to consumer, every rung already mounted
        // except this one:
        //   PreRaceFlyByState::Update tail (BrnPreRaceFlyBy_wJ_04.cpp:360) posts GUI-out 163 once
        //     mfTimeRemaining (KAF_MODE_TYPE_PRE_EVENT_DURATION[mode]) expires
        //   -> BridgeGuiToGameState case 163 -> game event 25 (GameBridgeGUIToX_GameState.cpp:153)
        //   -> GameStateModule::ProcessGameEvents case 25 -> ModeManager::FinishOfflineModeIntro
        //      -> GameMode::SendEvent(E_GME_NEXT)
        //   -> IntroState::OnLeave sets GameMode::mbIntroJustFinished (+176)
        //   -> ModeManager::UpdateCurrentMode's latch arm (BrnModeManager_UpdateMode.cpp:345)
        //      -> ModeManager::StopModeIntro posts ACTION 30
        //   -> **THIS ARM** -> GUI event 164
        //   -> PreRaceFlyByState::HandleIncomingEvents KI_EVENT_FLYBY_END -> trans-out (or an
        //      immediate TriggerExitState when nothing is on screen yet)
        //   -> TriggerExitState -> SendStateEvent("BF_PROCEED") -> the lua BRNEVENTFSM leaves
        //      PRE_FLY_BY.
        // Every rung above 30 was proven live in scratch/flow_run/20260827_134528/BrnGame.log
        // (the SECOND "[start] event 25 -> FinishOfflineModeIntro" at t+6.15 s is the fly-by's own
        // 163 arriving 0.13 s after the one-shot harness self-trigger); the flow stopped dead here
        // because action 30 had no arm and fell through the caller's `default:`.
        // 34 -> GuiGameModeStarted (id 237). X360 case 34 @0x823E9CE0: `v504 = *v7 ;
        // v503 = *(v7 + 1) ; AddGuiEvent<GuiGameModeStarted>(&v503)`.
        case BrnGameState::GameStateModuleIO::E_ACTION_START_PLAYING_MODE:
        {
            const BrnGameState::GameStateModuleIO::StartPlayingModeAction* lpStart =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::StartPlayingModeAction*>(lpAction);
            GameModeStartedWire237 lStarted; lStarted.meGameMode = static_cast<s32>(lpStart->meGameMode); lStarted.mDestinationLandmarkID = lpStart->mDestinationLandmarkID;
            PushGuiEvent(lStarted, lpGuiInput);
            return true;
        }

        case BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE_INTRO:        {
            const BrnGameState::GameStateModuleIO::StopModeIntroAction* lpStopIntro =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::StopModeIntroAction*>(lpAction);

            const s32 liGameModeType = static_cast<s32>(lpStopIntro->meGameMode);

            if (!IsOnlineLobbyOrShowtimeMode(liGameModeType))
            {
                PreRaceFlyByEndWire164 lFlyByEnd;
                lFlyByEnd.mu8Unused = 0;                 // console: the slot is left unwritten
                PushGuiEvent(lFlyByEnd, lpGuiInput);
            }
            else
            {
                NetworkSplashWire269 lSplashOff;
                lSplashOff.miSplashState = 0;            // `stw r19` @0x823EB344
                PushGuiEvent(lSplashOff, lpGuiInput);

                NetworkShowFreeBurnIntroWire279 lFreeBurnIntro;
                lFreeBurnIntro.maZero[0] = 0;            // `stb r19` @0x823EB364
                lFreeBurnIntro.maZero[1] = 0;            // `stb r19` @0x823EB35C
                PushGuiEvent(lFreeBurnIntro, lpGuiInput);

                if (lpStopIntro->mbMovingBetweenLobbyModes)   // `lbz r11, 4(r31)` @0x823EB36C
                {
                    NetworkSplashWire269 lSplashOn;
                    lSplashOn.miSplashState = 1;         // `stw r14` @0x823EB37C
                    PushGuiEvent(lSplashOn, lpGuiInput);
                }
            }

            return true;
        }

        // ---- 47  E_ACTION_SET_COUNTDOWN (4 bytes) ------------------------------------------
        // @0x823EAD50..0x823EAD64: `lwz r11,0(r31); stw r11, var_35D8;
        // AddGuiEvent<GuiEventUpdateEventCountdown>` (id 234, size 4). One word, straight through.
        case BrnGameState::GameStateModuleIO::E_ACTION_SET_COUNTDOWN:
        {
            const BrnGameState::GameStateModuleIO::SetCountdownAction* lpCountdown =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::SetCountdownAction*>(lpAction);

            UpdateEventCountdownWire234 lEvent;
            lEvent.miCountdownDisplay = lpCountdown->miCountdownDisplay;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 75  E_ACTION_CAR_SELECT_READY (4 bytes) ---------------------------------------
        // ⭐⭐⭐ [returning-player wave 2026-08-28] THE ARM THAT PUTS THE GUI IN THE JUNKYARD
        // CAR-SELECT SCREEN. @0x823EA700..0x823EA714, three instructions:
        //     lwz r11, 0(r31)                 -- the action's only word
        //     stw r11, var_35D8(r1)           -- payload +0x00
        //     bl  AddGuiEvent<GuiCarSelectStartEvent>      (id 81, size 4)
        // One word, straight through -- the same shape as the case-47 countdown arm below.
        //
        // ⛔ WHY IT IS LOAD-BEARING, MEASURED (scratch/flow_run/rp_A_return, a returning-profile
        // boot with the junkyard-entry fix landed): CarSelectManager reached E_STATE_CAR_SELECT
        // and posted this action ("=== CarSelectManager: Car Select", log :1827), and NOTHING
        // CONSUMED IT -- TranslateGameActionsToGuiEvents is a documented deferral and this arm did
        // not exist -- so the SCREEN flow stayed in BrnGui::InGame, the car-select screen never
        // came up, no ACCEPT could ever arrive, CarSelectManager::UpdateExitState never ran, and
        // the ResetPlayerCarAction that CLEARS RaceCarEntityModule::mbInCarSelectScreen was never
        // posted. The car therefore could not start its engine for the whole run (the ignition
        // chain is spelled out in BrnRaceCarEntityModule.cpp's AttachActiveRaceCar banner).
        //
        // ⭐ WHY THE FRESH PATH NEVER NEEDED IT, and why that hid this for so long: on a NEW
        // profile the screen flow reaches the car-select states through the new-profile intro
        // (the licence / DMV sequence -- "BNLicense", "DmvStamp" in the log), which drives the
        // scripted screen FSM on its own. A returning boot has no intro, so the console's own
        // action-75 -> GUI-81 -> InGame::Update case 81 -> SendStateEvent("TO_CSELECT") chain is
        // the ONLY route -- and every other rung of it was already reconstructed and mounted.
        case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECT_READY:
        {
            const BrnGameState::GameStateModuleIO::CarSelectReadyAction* lpReady =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::CarSelectReadyAction*>(lpAction);

            CarSelectStartWire81 lEvent;
            lEvent.miCarSelectFlow = lpReady->miCarSelectFlow;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 44  E_ACTION_SET_IN_MODE_START_REGION (4 bytes) -------------------------------
        // @0x823EA948..0x823EA97C:
        //   lbz  r11, 2(r31)                       -- the action's in-region byte
        //   bne  -> { r11 = lhz 0(r31) ;   payload+0x00 = r19 (0) }
        //   beq  -> { payload+0x00 = r14 (1) ; r11 = lhz word_82F241B8 }
        //   sth  r11, payload+0x04
        //   AddGuiEvent<GuiEventEnterEventStartLocation>  (id 166, size 8)
        // The rodata halfword at 0x82F241B8 is 0xFFFF -- read out of the image at file offset
        // 0xF241B8 (bytes FF FF, big-endian), i.e. the invalid-start-location sentinel, NOT an
        // invented constant.
        case BrnGameState::GameStateModuleIO::E_ACTION_SET_IN_MODE_START_REGION:
        {
            const BrnGameState::GameStateModuleIO::SetInModeStartRegionAction* lpRegion =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::SetInModeStartRegionAction*>(lpAction);

            EnterEventStartLocationWire166 lEvent;
            lEvent.maPad06[0] = 0;
            lEvent.maPad06[1] = 0;
            if (lpRegion->mbInModeStartRegion)
            {
                lEvent.muLeavingStartLocation = 0u;
                lEvent.mu16StartLocationId    =
                    static_cast<u16>(static_cast<s32>(lpRegion->mCurrentLandmarkIndex));
            }
            else
            {
                lEvent.muLeavingStartLocation = 1u;
                lEvent.mu16StartLocationId    = 0xFFFFu;   // image 0x82F241B8 (BE) == FF FF
            }
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 38  E_ACTION_FINISHED_MODE_RESULTS (1 byte) -----------------------------------
        // @0x823EAD20..0x823EAD48. The whole arm: log the banner behind the
        // CgsDev::Message::gxMessageFilterFlags bit-0 gate, then
        // AddGuiEvent<GuiEventFinishedModeResults> (id 321, size 1) from an unwritten scratch
        // byte. The banner is the image literal loaded into var_33A4 @0x823EA130 -- and it is
        // what NAMES action 38 (see the citation in BrnGameActions.h).
        case BrnGameState::GameStateModuleIO::E_ACTION_FINISHED_MODE_RESULTS:
        {
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "***** GameBridgeGameStateToX found E_ACTION_FINISHED_MODE_RESULTS *****\n";
            }

            FinishedModeResultsWire321 lEvent;
            lEvent.mu8Unused = 0;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // ---- 39  E_ACTION_STOP_MODE (24 bytes) ---------------------------------------------
        // @0x823EABCC..0x823EAC00, five copies into a 12-byte frame at var_3388:
        //   +0x00 <- lwz 0x00(r31)   +0x04 <- lwz 0x0C(r31)
        //   +0x08 <- lbz 0x11(r31)   +0x09 <- lbz 0x12(r31)   +0x0A <- lbz 0x13(r31)
        // then AddGuiEvent<GuiEventStopMode> (id 322, size 12).
        case BrnGameState::GameStateModuleIO::E_ACTION_STOP_MODE:
        {
            const BrnGameState::GameStateModuleIO::StopModeAction* lpStop =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::StopModeAction*>(lpAction);

            StopModeWire322 lEvent;
            lEvent.meGameModeType = static_cast<s32>(lpStop->meGameModeType);
            lEvent.miField04      = lpStop->miField0C;
            lEvent.mu8Field08     = lpStop->mu8Field11;
            lEvent.mu8Field09     = lpStop->mu8Field12;
            lEvent.mu8Field0A     = lpStop->mu8Field13;
            lEvent.maPad0B[0]     = 0;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 200  E_ACTION_UPDATE_PLAYER_MEDALS (8 bytes) ----------------------------------
        // @0x823EA784..0x823EA808. Behind the gxMessageFilterFlags bit-0 gate the console streams
        // `"Medals update: " << h0 << ", " << h2 << ", " << h4 << "\n"` (the image literals at
        // var_33BC / var_33C4 / r17, loaded @0x823EA1A0 / @0x823EA198 / @0x823EA190), then copies
        // four halfwords straight across into GuiEventMedalUpdate (id 307, size 8, @0x823D1A28).
        case BrnGameState::GameStateModuleIO::E_ACTION_UPDATE_PLAYER_MEDALS:
        {
            const BrnGameState::GameStateModuleIO::UpdatePlayerMedalsAction* lpMedals =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::UpdatePlayerMedalsAction*>(lpAction);

            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Medals update: " << static_cast<s32>(lpMedals->mi16TotalWins)
                    << ", " << static_cast<s32>(lpMedals->mi16Field02)
                    << ", " << static_cast<s32>(lpMedals->mi16Field04) << "\n";
            }

            MedalUpdateWire307 lEvent;
            lEvent.mi16TotalWins      = lpMedals->mi16TotalWins;
            lEvent.mi16Field02        = lpMedals->mi16Field02;
            lEvent.mi16Field04        = lpMedals->mi16Field04;
            lEvent.mi16WinsToNextRank = lpMedals->mi16WinsToNextRank;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // ---- 37  E_ACTION_SHOW_MODE_RESULTS (232 bytes) ------------------------------------
        // @0x823EA984..0x823EAB78. Four posts, in this order:
        //   a. the post-event REQUEST fork on action+0xE4:
        //        set   -> GuiEventTriggerOnlinePostEvent (320, 1) carrying action+0xE1;
        //        clear -> the bare CgsGui::GuiEvent<291> (291, 1) tag, and the offline results
        //                 record below is then built unconditionally.
        //      (When +0xE4 IS set, the results record is built only for the showtime pair -- the
        //       console's r27 starts as `action+0x00 in {2,16}` and the clear branch forces it
        //       true, `mr r27, r14` @0x823EA9D8.)
        //   b. the 192-byte GuiEventOfflinePostEvent (289) -- store map on OfflinePostEventWire289.
        //   c. GuiAutosaveRequestEvent (356) with a zero byte, only when +0xE4 is clear.
        //   d. GuiSetEasyDriveNotAllowedEvent (96) with a zero byte, unconditionally.
        //
        // FLAG -- THE THREE CONSOLE ASSERTS IN THIS ARM ARE NOT REPRODUCED
        // (@0x823EA9E8..0x823EAA70, GameBridgeGameStateToX.cpp:1016/1017/1018:
        //   "lScoringOutput.mePlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0"
        //   "lScoringOutput.mePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT"
        //   "lScoringOutput.mabValid[lScoringOutput.mePlayerRaceCarIndex]").
        // They read a scoring-output block at lpGameStateOutput + 173240 (the prologue's
        // `addis r11, r31, 3 / addi r11, r11, -0x5B48` @0x823E9D68, cached in var_3534), whose
        // layout has no home in b5-decomp -- the index is at +0xA34 and the validity array at
        // +0xA08. Hand-modelling that offset is exactly the X360-value-on-the-x64-host trap this
        // campaign keeps recording, so the asserts are NAMED AND SKIPPED rather than faked. They
        // are pure diagnostics: no store in the arm depends on them.
        //
        // FLAG -- most of this record's field NAMES are unproven (see the FLAGs on
        // ShowModeResultsAction and OfflinePostEventWire289). The COPY is exact; the meanings are
        // not.
        // ⚠️ THE OLD TAIL OF THIS NOTE -- "Nothing consumes id 289 in b5-decomp yet, so nothing
        // depends on the names" -- IS NOW FALSE (2026-08-29). Two consumers read the record:
        // BrnGui::InstantResultsState and BrnGui::ShowtimeInstantResultsState, both via
        // GuiCache::GetOfflinePostEventData().
        // ⭐ AND THREE OF THE FLAGGED SLOTS BELOW ARE NOW ATTESTED FROM THE CONSUMER SIDE, so
        // whoever next touches this arm should rename them here rather than re-deriving:
        //     miField24 (action+0x10) == OfflinePostEventData::miBaseScore
        //     miField28 (action+0x14) == OfflinePostEventData::miScoreMultiplier
        //     mfField2C (action+0x18) == OfflinePostEventData::mfDistanceTravelled  (metres)
        // Four independent agreements, not one: (a) the DecFIGS DWARF declares exactly that
        // trio consecutively (BrnGuiEventTypeDefs.h:2393/:2394/:2395); (b) the X360 pins the
        // middle one BY NAME in its own stringized assert -- ShowtimeInstantResultsState::
        // UpdateScoreTotalling @0x824C62C4 fires "mResults.miScoreMultiplier > 0" on the word at
        // record+0x28; (c) SetupTotalling @0x824BB548 multiplies record+0x2C by 1.0936133 (the
        // metres->yards constant) and UpdateScoreTotalling hands it to SetLocalisedText under
        // "KAC_SHOWTIME_DISTANCE_IN_METRES"; (d) mfField18's own FLAG in BrnGameActions.h had
        // ALREADY reached "the SHOWTIME DISTANCE ... the raw metres value", from the producer
        // side alone. The rename is deliberately NOT made in this pass: ShowModeResultsAction's
        // write sites live in BrnModeManager_Finish.cpp, which another wave is editing.
        case BrnGameState::GameStateModuleIO::E_ACTION_SHOW_MODE_RESULTS:
        {
            const BrnGameState::GameStateModuleIO::ShowModeResultsAction* lpResults =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::ShowModeResultsAction*>(lpAction);

            bool lbBuildResultsRecord = IsShowtimeMode(static_cast<s32>(lpResults->meGameModeType));

            if (lpResults->mbIsOnlinePostEvent != 0)
            {
                TriggerOnlinePostEventWire320 lOnline;
                lOnline.mu8Flag = lpResults->mu8FieldE1;
                PushGuiEvent(lOnline, lpGuiInput);
            }
            else
            {
                PostEventRequestWire291 lOffline;
                lOffline.mu8Unused = 0;
                PushGuiEvent(lOffline, lpGuiInput);
                lbBuildResultsRecord = true;                    // `mr r27, r14` @0x823EA9D8
            }

            if (lbBuildResultsRecord)
            {
                OfflinePostEventWire289 lEvent;
                std::memset(&lEvent, 0, sizeof(lEvent));
                lEvent.mu64Field00    = lpResults->mu64FieldC8;
                lEvent.meGameModeType = static_cast<s32>(lpResults->meGameModeType);
                lEvent.miField1C      = lpResults->miField08;
                lEvent.mfField20      = lpResults->mfField0C;
                lEvent.miField24      = lpResults->miField10;
                lEvent.miField28      = lpResults->miField14;
                lEvent.mfField2C      = lpResults->mfField18;
                lEvent.miFieldA0      = lpResults->miField48;
                lEvent.miFieldA4      = lpResults->miField4C;
                lEvent.mu8FieldB0     = static_cast<u8>(lpResults->miFinishPosition);
                lEvent.mu8FieldB1     = lpResults->mu8FieldE0;
                lEvent.mu8FieldB2     = lpResults->mu8FieldE2;
                lEvent.mu8FieldB3     = lpResults->mu8FieldE1;
                lEvent.mu8FieldB4     = lpResults->mu8FieldE3;
                lEvent.mu8FieldB5     = lpResults->mu8FieldDA;
                lEvent.mu8FieldB6     = lpResults->mu8FieldDB;
                lEvent.mbHasField08   = lpResults->mbHasField40;
                lEvent.mu8FieldB8     = lpResults->mu8FieldDC;
                // Unconditional console store BEFORE the gated block copy: `li r10,-1;
                // stw r10, base+0x98` @0x823EA9EC..0x823EA9F4. +0x98 falls inside maBlock30
                // (index 0x68), so when mbHasBlock58 is set the memcpy overwrites it -- but on
                // the no-block path the console posts -1 there, not the memset's 0.
                {
                    const s32 liMinusOne = -1;
                    std::memcpy(&lEvent.maBlock30[0x68], &liMinusOne, sizeof(liMinusOne));
                }
                // The two gated copies in the console's own order (@0x823EAAE0 then @0x823EAB00):
                // the 0x70 block first -- on the console it OVERWRITES two frame words the arm
                // had already written (payload+0x70 and payload+0x98 fall inside the memcpy
                // range), which is why the order matters and is reproduced.
                if (lpResults->mbHasBlock58 != 0)
                {
                    std::memcpy(lEvent.maBlock30, lpResults->maBlock58, 0x70);
                }
                if (lpResults->mbHasField40 != 0)
                {
                    lEvent.mu64Field08 = lpResults->mu64Field40;
                }
                PushGuiEvent(lEvent, lpGuiInput);
            }

            if (lpResults->mbIsOnlinePostEvent == 0)
            {
                AutosaveRequestWire356 lAutosave;
                lAutosave.mu8Zero = 0;
                PushGuiEvent(lAutosave, lpGuiInput);
            }

            SetEasyDriveNotAllowedWire96 lEasyDrive;
            lEasyDrive.mu8Zero = 0;
            PushGuiEvent(lEasyDrive, lpGuiInput);

            return true;
        }

        // =====================================================================================
        // ⭐⭐ [road-rage wave 2026-09-02] THE THREE ROAD RAGE ARMS. Not event-flow arms, and
        // here for the same reasons as arm 75: the member function has one definition (in the
        // stunt TU), and this is the wired dispatch seam.
        // Action ids are LITERAL X360 jump-table cases: BrnGameActions.h carries no enumerator
        // for 103 / 255 / 205 yet (DWARF E_ACTION_PLAYER_REACHED_ROAD_RAGE_TARGET 98 (+5),
        // E_ACTION_HUD_MESSAGE_ROAD_RAGE_TIME_EXTENSION 248 (+7), E_ACTION_ROAD_RAGE_PLAYER_DAMAGE
        // 197 (+8)) -- the value is the console's, the name is the header request's.
        // =====================================================================================

        // ---- 103  the TARGET REACHED (1 byte, no payload) -----------------------------------
        // @0x823EA934..0x823EA944: `mr r5, r20 ; addi r4, r1, var_354B ; add r3, r29, r30 ;
        // AddGuiEvent<GuiEventPlayerReachedRoadRageTarget>` (id 168, size 1). No load off the
        // action, no store into the frame: the post IS the message.
        case 103:
        {
            PlayerReachedRoadRageTargetWire168 lEvent;
            lEvent.mu8Unused = 0;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 255  the TIME EXTENDED (4 bytes) -----------------------------------------------
        // @0x823EC494..0x823EC4A8: `lwz r11, 0(r31) ; stw r11, var_35D8 ;
        // AddGuiEvent<GuiEventRoadRageTimeExtended>` (id 427, size 4). One word, straight through.
        case 255:
        {
            const HUDMessageRoadRageTimeExtensionActionMirror* lpExtension =
                reinterpret_cast<const HUDMessageRoadRageTimeExtensionActionMirror*>(lpAction);

            RoadRageTimeExtendedWire427 lEvent;
            lEvent.muExtensiontime = lpExtension->muExtensionTime;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 205  the PLAYER DAMAGE (8 bytes) -----------------------------------------------
        // @0x823EAB7C..0x823EABA0, in the console's own order: `lbz r11, 4(r31)` first, then
        // `lfs f0, 0(r31) ; stfs var_34E0 ; stb r11, var_34DC ; lbz r11, 5(r31) ;
        // stb r11, var_34DB ; AddGuiEvent<GuiEventRoadRagePlayerDamage>` (id 348, size 8).
        case 205:
        {
            const RoadRagePlayerDamageActionMirror* lpDamage =
                reinterpret_cast<const RoadRagePlayerDamageActionMirror*>(lpAction);

            RoadRagePlayerDamageWire348 lEvent;
            lEvent.mfHowCloseToTotalled     = lpDamage->mfHowCloseToTotalled;       // stfs +0x00
            lEvent.mbOneMoreCrashToTotalled = lpDamage->mbOneMoreCrashToTotalled;   // stb  +0x04
            lEvent.mbPlayerTotalled         = lpDamage->mbPlayerTotalled;           // stb  +0x05
            lEvent.maPad06[0] = 0;
            lEvent.maPad06[1] = 0;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ARTIST TranslateGameActionsToGuiEvents 0x823E9CE0, road display arms.
        case 273:
        {
            BrnGui::GuiEventRoadRuleEnter event = {};
            event.Construct(reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction*>(lpAction));
            PushGuiEvent(event, lpGuiInput);
            BrnGui::GuiEventRoadRuleData data;
            data.Construct(reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction*>(lpAction));
            PushGuiEvent(data, lpGuiInput);
            return true;
        }
        case 274:
        {
            struct RoadLeave { CgsID id; u8 residue[8]; s32 GetEventType() const { return 340; } } event = {};
            CGS_STATIC_ASSERT((sizeof(RoadLeave) == 16), "ARTIST GUI road-leave payload");
            event.id = *reinterpret_cast<const CgsID*>(lpAction);
            PushGuiEvent(event, lpGuiInput);
            return true;
        }
        case 276:
        {
            BrnGui::GuiEventRoadRuleUpcomingRoads event = {};
            event.Construct(reinterpret_cast<const BrnGameState::GameStateModuleIO::UpcomingRoadChangeAction*>(lpAction));
            PushGuiEvent(event, lpGuiInput);
            return true;
        }

        // ---- [RR-GUI] the rest of the road-rule band ---------------------------------------
        // The switch is indexed by the action id itself (the jump table is read at id*4 with
        // no rebase; the 273/274/276 arms above sit at their own ids). Console arms, in id order:
        // 275 -> 344, 277 -> 335, 278 -> 336, 279 -> 338 (in the stunt sibling: it reads the
        // crash scorer off the game module), 280 -> 339, 281 -> 342, 282 -> 343, 284 -> 345,
        // 285 -> 346, 286 -> 330. Action 283 (the time-rule warning) sits on the table's DEFAULT
        // list: the console posts no GUI event for it. Every GUI record here is flat (its home
        // is BrnGuiEventTypeDefs.h) except 336 / 339, which the HUD reads through their types.

        // 275 -> GuiEventRoadRuleBatchDataResponse (344): the per-road "beaten" table.
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_BATCH_QUERY:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesBatchQueryAction* lpRoadRulesBatchQuery =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesBatchQueryAction*>(lpAction);
            CGS_ASSERT(lpRoadRulesBatchQuery, "lpRoadRulesBatchQuery");
            CGS_ASSERT(lpRoadRulesBatchQuery->miNumRoads <= BrnGameState::KI_MAX_CHALLENGES,
                       "lpRoadRulesBatchQuery->miNumRoads <= BrnGameState::KI_MAX_CHALLENGES");

            // The console record is a frame local whose unwritten slots (roads past the count)
            // carry stack residue; zero here.
            BrnGui::GuiEventRoadRuleBatchDataResponse lEvent;
            memset(&lEvent, 0, sizeof(lEvent));
            lEvent.miRoadCount = lpRoadRulesBatchQuery->miNumRoads;
            for (s32 liRoad = 0; liRoad < lEvent.miRoadCount; ++liRoad)
            {
                lEvent.maRoadIds[liRoad]                   = lpRoadRulesBatchQuery->maRoadIds[liRoad];
                lEvent.mabPlayerBeatenOfflineTime[liRoad]  = lpRoadRulesBatchQuery->mabPlayerBeatenParTime[liRoad];
                lEvent.mabPlayerBeatenOfflineCrash[liRoad] = lpRoadRulesBatchQuery->mabPlayerBeatenParCrash[liRoad];
                lEvent.mabPlayerBeatenOnlineTime[liRoad]   = lpRoadRulesBatchQuery->mabPlayerBestOnlineTime[liRoad];
                lEvent.mabPlayerBeatenOnlineCrash[liRoad]  = lpRoadRulesBatchQuery->mabPlayerBestOnlineCrash[liRoad];
            }
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 277 -> GuiEventRoadRuleBegin (335): only the rule's score type crosses (`lwz 8`).
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_START_RULE:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesStartRuleAction* lpStartRule =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesStartRuleAction*>(lpAction);
            BrnGui::GuiEventRoadRuleBegin lEvent;
            lEvent.meRuleType = lpStartRule->meScoreType;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 278 -> GuiEventRoadRuleEnd (336): road id, rule type, score, valid-attempt byte.
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_END_RULE:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesEndRuleAction* lpEndRule =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesEndRuleAction*>(lpAction);
            BrnGui::GuiEventRoadRuleEnd lEvent;
            lEvent.mRoadId        = lpEndRule->mRoadId;
            lEvent.meRuleType     = lpEndRule->meScoreType;
            lEvent.mfScore        = lpEndRule->mfScore;
            lEvent.mbScoreAttempt = lpEndRule->mbValidAttempt;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 280 -> GuiEventRoadRuleUpdateTargetScores (339): the leader / best pair per score type,
        // rebuilt from the action's friend and user records (GuiEventRoadRuleUpdateTargetScores::
        // Construct).
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE_TARGET_ROAD_SCORE:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesUpdateTargetScoreAction* lpRRAction =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesUpdateTargetScoreAction*>(lpAction);
            CGS_ASSERT(lpRRAction, "lpRRAction");

            BrnGui::GuiEventRoadRuleUpdateTargetScores lEvent;
            lEvent.Construct(lpRRAction);
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 281 -> GuiEventRoadRuleNewHighScore (342): a field-for-field copy.
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_HIGH_SCORE:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesNewHighScoreAction* lpRoadRulesHighScoreAction =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesNewHighScoreAction*>(lpAction);
            CGS_ASSERT(lpRoadRulesHighScoreAction, "lpRoadRulesHighScoreAction");

            BrnGui::GuiEventRoadRuleNewHighScore lEvent;
            lEvent.mPlayerName                = lpRoadRulesHighScoreAction->mPlayerName;
            lEvent.mRoadId                    = lpRoadRulesHighScoreAction->mRoadId;
            lEvent.mbIsLocalPlayer            = lpRoadRulesHighScoreAction->mbIsLocalPlayer;
            lEvent.meScoreType                = lpRoadRulesHighScoreAction->meScoreType;
            lEvent.miNumScoresLost            = lpRoadRulesHighScoreAction->miNumScoresLost;
            lEvent.miNumRoadsNowRuled         = lpRoadRulesHighScoreAction->miNumRoadsNowRuled;
            lEvent.mbIsWholeRoadOwned         = lpRoadRulesHighScoreAction->mbIsWholeRoadOwned;
            lEvent.mbWasRulePlayersBefore     = lpRoadRulesHighScoreAction->mbWasRulePlayersBefore;
            lEvent.mbMultipleScores           = lpRoadRulesHighScoreAction->mbMultipleScores;
            lEvent.mbOnlineLossButOfflineWin  = lpRoadRulesHighScoreAction->mbOnlineLossButOfflineWin;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 282 -> GuiEventRoadRuleChangeMode (343): the new active rule.
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_ACTIVE_RULE_CHANGE:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesActiveRuleChangeAction* lpRuleChange =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesActiveRuleChangeAction*>(lpAction);
            BrnGui::GuiEventRoadRuleChangeMode lEvent;
            lEvent.meScoreType = lpRuleChange->meActiveRoadRule;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 284 -> GuiEventRoadRuleTickerScoreResponse (345): the ticker's per-road score line.
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_ROAD_SCORE:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesRoadScoreAction* lpScoreAction =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesRoadScoreAction*>(lpAction);
            CGS_ASSERT(lpScoreAction, "lpScoreAction");

            BrnGui::GuiEventRoadRuleTickerScoreResponse lEvent;
            memset(&lEvent, 0, sizeof(lEvent));
            lEvent.mPlayerName.Construct(lpScoreAction->mPlayerName.GetPlayerName());
            lEvent.mRoadID                 = lpScoreAction->mRoadID;
            lEvent.mRoadChallengeIndex     = lpScoreAction->mChallengeIndex;
            lEvent.meActiveRoadRule        = lpScoreAction->meActiveRoadRule;
            lEvent.miOfflineScore          = lpScoreAction->miOfflineScore;
            lEvent.miOnlineScore           = lpScoreAction->miOnlineScore;
            lEvent.mbAIRulesRoadOffline    = lpScoreAction->mbAIRulesRoadOffline;
            lEvent.mbAIRulesRoadOnline     = lpScoreAction->mbAIRulesRoadOnline;
            lEvent.mbLocalPlayerRulesRoad  = lpScoreAction->mbLocalPlayerRulesRoad;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 285 -> GuiEventRoadRuleNewRulers (346): the record is cleared, then the action's
        // changed-roads mask is OR'd in (a zero `std`, then `ld` / `or` / `std`).
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_RULERS:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesNewRulersAction* lpNewRulersAction =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesNewRulersAction*>(lpAction);
            CGS_ASSERT(lpNewRulersAction, "lpNewRulersAction");

            BrnGui::GuiEventRoadRuleNewRulers lEvent;
            lEvent.mRoadRulesChangedBitArray.UnSetAll();
            lEvent.mRoadRulesChangedBitArray.ORArrays(&lEvent.mRoadRulesChangedBitArray,
                                                      &lpNewRulersAction->mRoadRulesChangedBitArray);
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // 286 -> GuiEventSetRoadRuleScoreMode (330): the online byte becomes the panel mode.
        case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_MODE_SWITCH:
        {
            const BrnGameState::GameStateModuleIO::RoadRulesModeSwitchAction* lpRoadRuleModeSwitchAction =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesModeSwitchAction*>(lpAction);
            CGS_ASSERT(lpRoadRuleModeSwitchAction, "lpRoadRuleModeSwitchAction");

            BrnGui::GuiEventSetRoadRuleScoreMode lEvent;
            lEvent.meNewRoadRuleScoreMode = lpRoadRuleModeSwitchAction->mbIsOnline
                                                ? BrnGui::GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE
                                                : BrnGui::GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_OFFLINE;
            PushGuiEvent(lEvent, lpGuiInput);
            return true;
        }

        // =====================================================================================
        // [race wave, lane BR] RACE / BURNING ROUTE / POST-EVENT ARMS. Every id below is the
        // console jump-table case itself: GetFirstEvent / GetNextEvent return the stored action
        // id unchanged and the dispatch indexes the table with it directly (id * 4 under an
        // unsigned `<= 0x12A` bound, no rebase).
        // =====================================================================================

        // ---- 40  E_ACTION_QUIT_MODE_OFFLINE (1 byte) --------------------------------------
        // The offline quit. Null assert on the record, then, unless the byte (the round
        // manager's starting-game-due-to-player-join flag) is set, hand the HUD back to the
        // free-burn FSM: GuiEventRunFsm{ CgsIDCompress("BRNFBFSM"), 0, E_GUI_HUD_FREEBURN (1),
        // E_GUIFLOW_HUD (1) } -- zero into the initial-state slot, one into both enum slots. "BRNFBFSM" is the image literal the prologue caches for the drain.
        // (Case 25, the offline STOP, compresses the same literal and posts nothing.)
        case BrnGameState::GameStateModuleIO::E_ACTION_QUIT_MODE_OFFLINE:
        {
            CGS_ASSERT(lpAction != 0, "lpQuitModeAction");
            const u8 lbStartingGameDueToPlayerJoin = *reinterpret_cast<const u8*>(lpAction);
            if (lbStartingGameDueToPlayerJoin == 0)
            {
                PostRunFsm(lpGuiInput, "BRNFBFSM", 0,
                           BrnGui::E_GUI_HUD_FREEBURN, BrnGui::E_GUIFLOW_HUD);
            }
            return true;
        }

        // ---- 113  E_ACTION_RACE_CAR_REACHED_CHECKPOINT (16 bytes) -------------------------
        // Three words straight across: +0x00 / +0x04 / +0x08 of the action into the 12-byte
        // GuiRaceCheckpointReached (id 425). Posted for every race car, not only the player;
        // HudMessageAnalyzer::HandleRaceCheckpointReached filters on the global index.
        case BrnGameState::GameStateModuleIO::E_ACTION_RACE_CAR_REACHED_CHECKPOINT:
        {
            const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction* lpCheckpoint =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction*>(lpAction);

            BrnGui::GuiRaceCheckpointReached lEvent;
            lEvent.meActiveRaceCarIndex = lpCheckpoint->meActiveRaceCarIndex;
            lEvent.meGlobalRaceCarIndex = lpCheckpoint->meGlobalRaceCarIndex;
            lEvent.miCheckpointIndex    = lpCheckpoint->miCheckPointIndex;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 204  E_ACTION_TROPHY_UNLOCK (16 bytes) ---------------------------------------
        // the word at action+0x08 to record+0x00, then the doubleword at action+0x00 to +0x08, into
        // GuiEventTrophyCarUnlock (id 375, size 16): the unlock type first, the car second.
        case BrnGameState::GameStateModuleIO::E_ACTION_TROPHY_UNLOCK:
        {
            const BrnGameState::GameStateModuleIO::TrophyUnlockAction* lpTrophy =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::TrophyUnlockAction*>(lpAction);

            BrnGui::GuiEventTrophyCarUnlock lEvent = {};
            lEvent.meUnlockType = static_cast<s32>(lpTrophy->meUnlockType);
            lEvent.mTrophyCarID = lpTrophy->mCarToUnlock;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 206  E_ACTION_ALL_EVENT_TYPE_WON (4 bytes) -----------------------------------
        // Null assert on the record, then one word into GuiEventAllOfTypeComplete (id 305).
        case BrnGameState::GameStateModuleIO::E_ACTION_ALL_EVENT_TYPE_WON:
        {
            const BrnGameState::GameStateModuleIO::AllEventTypeWonAction* lpAllEventTypeWonAction =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::AllEventTypeWonAction*>(lpAction);
            CGS_ASSERT(lpAllEventTypeWonAction != 0, "lpAllEventTypeWonAction != NULL");

            AllOfTypeCompleteWire305 lEvent;
            lEvent.meGameEventType = static_cast<s32>(lpAllEventTypeWonAction->leGameEventType);
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 208  E_ACTION_GAME_COMPLETION_RESULTS (8 bytes) ------------------------------
        // The word at action+0x00 is tested against -1: outside a mode the bytes at +0x04 / +0x05 go
        // to GuiEventGameCompleted (309), inside one to GuiEventGameCompletedOnline (310).
        case BrnGameState::GameStateModuleIO::E_ACTION_GAME_COMPLETION_RESULTS:
        {
            const BrnGameState::GameStateModuleIO::GameCompletionResultsAction* lpCompletion =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::GameCompletionResultsAction*>(lpAction);

            if (lpCompletion->meGameMode == BrnGameState::GameStateModuleIO::E_MODE_NONE)
            {
                GameCompletedWire309 lEvent;
                lEvent.mbGameComplete              = lpCompletion->mbGameComplete ? 1u : 0u;
                lEvent.mbCompletionAlreadyRecorded =
                    lpCompletion->mbCompletionAlreadyRecorded ? 1u : 0u;
                PushGuiEvent(lEvent, lpGuiInput);
            }
            else
            {
                GameCompletedOnlineWire310 lEvent;
                lEvent.mbGameComplete              = lpCompletion->mbGameComplete ? 1u : 0u;
                lEvent.mbCompletionAlreadyRecorded =
                    lpCompletion->mbCompletionAlreadyRecorded ? 1u : 0u;
                PushGuiEvent(lEvent, lpGuiInput);
            }

            return true;
        }

        // ---- 210  E_ACTION_ALL_RIVALS_SHUTDOWN (1 byte) -----------------------------------
        // No load off the action and no store into the frame: GuiEventAllOfRivalsShutdown
        // (id 306) from an unwritten stack byte.
        case BrnGameState::GameStateModuleIO::E_ACTION_ALL_RIVALS_SHUTDOWN:
        {
            AllOfRivalsShutdownWire306 lEvent;
            lEvent.mu8Unused = 0;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 251  the WRONG WAY message (no payload) --------------------------------------
        // No enumerator in BrnGameActions.h (its band note puts the declared WRONG_WAY at this
        // seat). The arm posts the bare CgsGui::GuiEvent<153> tag from an unwritten stack slot.
        case 251:
        {
            WrongWayWire153 lEvent;
            lEvent.mu8Unused = 0;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 262  E_ACTION_MODE_TIME_UP (1 byte) ------------------------------------------
        // The action's byte at +0x00 straight into GuiEventTimeUp (id 550, size 1).
        case BrnGameState::GameStateModuleIO::E_ACTION_MODE_TIME_UP:
        {
            TimeUpWire550 lEvent;
            lEvent.mbSucceeded = *reinterpret_cast<const u8*>(lpAction);
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        // ---- 272  E_ACTION_WRONG_CAR_FOR_CHALLENGE (8 bytes) ------------------------------
        // The action's doubleword at +0x00 into GuiEventFailedToStartEvent (id 316, size 8):
        // the car the junction's event demands, straight across. Its consumer is
        // HudMessageAnalyzer case 316 -> HandleFailedToStartBurningRoute ("BRWrongCar",
        // naming "CAR_CAPS_<car>").
        case BrnGameState::GameStateModuleIO::E_ACTION_WRONG_CAR_FOR_CHALLENGE:
        {
            const BrnGameState::GameStateModuleIO::WrongCarForChallengeAction* lpWrongCar =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::WrongCarForChallengeAction*>(lpAction);

            BrnGui::GuiEventFailedToStartEvent lEvent;
            lEvent.mSpecialCarID = lpWrongCar->mSpecialEventCarId;
            PushGuiEvent(lEvent, lpGuiInput);

            return true;
        }

        default:
            // Not an event-flow action: the free-roam sibling gets it next.
            return TranslateFreeRoamGameActionToGuiEvent(liActionType, lpAction, lpGuiInput);
        }
    }
} // namespace BrnGame

// FOLDED FROM GameBridgeGameStateToX_EventStartsGuiEvents.cpp
// ARTIST 0x823EF1A0..0x823EF1DC: the valid flag is the only gate.
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include <cstring>

namespace BrnGame
{
void BridgeGameStateToGui_EventStarts(
    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    if (lpGameStateOutput->GetSetUpAllEventStartsInterfaceIsValid())
    {
        BrnGui::GuiEventUpdateEventStarts lEvent;
        const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface& lrInterface =
            lpGameStateOutput->GetSetUpAllEventStartsInterface();
        std::memcpy(&lEvent.mEventStarts, &lrInterface, sizeof(lEvent.mEventStarts));
        PushGuiEvent(lEvent, lpGuiInput); // 203 / 8416, including a valid empty/same-count table
    }
}
}

// FOLDED FROM GameBridgeGameStateToX_EventStatusGuiEvents.cpp
// ARTIST 0x823EE9C8..0x823EF19C: direct status/output-building portion of
// BrnGameModule::BridgeGameStateToGui. The wrapper performs the original input
// assertions and raw GUI-queue append before entering this portion.
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGame
{
namespace
{
    using namespace BrnGameState::GameStateModuleIO;

    // DecFIGS names this inline helper. ARTIST 0x823EEBB4..0x823EEC10:
    // the first valid blue-team lane is the runner in mode 13.
    EActiveRaceCarIndex GetOnlineRunnerRaceCarIndex(
        const ScoringOutputInterface* lpScoringOutputInterface,
        const OnlineScoringOutputInterface* lpOnlineScoringOutputInterface)
    {
        for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
             leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leCar++)
        {
            if (lpScoringOutputInterface->mabValid[leCar] &&
                lpOnlineScoringOutputInterface->maePlayerTeam[leCar] == E_PLAYER_TEAM_BLUE_TEAM)
                return leCar;
        }
        return E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }

    // Existing PC boundary retained pending the below-stage-8 owner/index trace.
    // ARTIST has bare player-indexed reads at 823EEB14, 823EEB74 and 823EEC78.
    // This guard is not asserted to be original behavior.
    bool IsPlayerRaceCarIndexInRange(s32 liIndex)
    {
        return liIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
    }
}

void BridgeGameStateToGui_EventStatus(
    const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    using namespace BrnGameState::GameStateModuleIO;
    const ScoringOutputInterface* lpScoringOutputInterface = lpGameStateOutput->GetScoringOutputInterface();
    const OnlineScoringOutputInterface* lpOnlineScoringOutputInterface = lpGameStateOutput->GetOnlineScoringOutputInterface();

    BrnGui::GuiEventRaceDistanceRemaining lDistanceRemaining;
    for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
         leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leCar++)
    {
        if (lpScoringOutputInterface->mabValid[leCar] == 1)
        {
            const EGameModeType leMode = lpScoringOutputInterface->meGameModeType;
            lDistanceRemaining.mafDistanceToFinish[leCar] =
                (leMode == E_MODE_ONLINE_FREE_BURN_LOBBY || leMode == E_MODE_ONLINE_SHOWTIME)
                ? static_cast<f32>(lpScoringOutputInterface->maiNumRoadsRuled[leCar])
                : lpScoringOutputInterface->maCarScoreData[leCar].GetDistanceToFinishLive();
            lDistanceRemaining.maCarId[leCar] = lpScoringOutputInterface->maCarIds[leCar];
            lDistanceRemaining.mabPlayerEliminated[leCar] = lpScoringOutputInterface->mabPlayerEliminated[leCar];
            lDistanceRemaining.mabValid[leCar] = true;
            lDistanceRemaining.maiOnlineStuntScore[leCar] = lpScoringOutputInterface->maCarScoreData[leCar].GetOnlineStuntScore();
        }
        else
        {
            lDistanceRemaining.mafDistanceToFinish[leCar] = 0.0f;
            lDistanceRemaining.maCarId[leCar] = 0;
            lDistanceRemaining.mabPlayerEliminated[leCar] = false;
            lDistanceRemaining.mabValid[leCar] = false;
            lDistanceRemaining.maiOnlineStuntScore[leCar] = 0;
        }
    }
    PushGuiEvent(lDistanceRemaining, lpGuiInput); // 239 / 144

    const s32 liPlayer = static_cast<s32>(lpScoringOutputInterface->mePlayerRaceCarIndex);
    BrnGui::GuiEventRaceDistanceToCheckpoint lDistanceToCheckpoint;
    lDistanceToCheckpoint.mfDistanceToCheckpoint = 0.0f;
    if (IsPlayerRaceCarIndexInRange(liPlayer))
        lDistanceToCheckpoint.mfDistanceToCheckpoint = lpScoringOutputInterface->maCarScoreData[liPlayer].GetDistanceToNextCheckpointLive();
    PushGuiEvent(lDistanceToCheckpoint, lpGuiInput); // 240 / 4

    const CgsSystem::TimerStatus* lpGameTimerStatus = lpTimerStatusInterface->GetGameTimerStatus();
    CGS_ASSERT_AT(lpGameTimerStatus != 0, "mTimerStatusInterface.GetGameTimerStatus()", CGS_ARTIST_FILE, 481);
    const EGameModeType leGameMode = lpScoringOutputInterface->meGameModeType;
    BrnGui::GuiEventCurrentStatus lCurrentStatus;
    const CgsSystem::Time lGameTime = lpGameTimerStatus->GetTime();
    lCurrentStatus.miGameTimeSeconds = lGameTime.GetSeconds();
    lCurrentStatus.mfGameTimeFraction = lGameTime.GetFraction();
    lCurrentStatus.mfGameTimeStep = lpGameTimerStatus->GetCurrentTimeStep();
    lCurrentStatus.mfDistanceToFinishLive = 0.0f;
    if (IsPlayerRaceCarIndexInRange(liPlayer))
        lCurrentStatus.mfDistanceToFinishLive = lpScoringOutputInterface->maCarScoreData[liPlayer].GetDistanceToFinishLive();
    lCurrentStatus.mfDistanceDrivenInCurrentCar = lpScoringOutputInterface->mfDistanceDrivenInCurrentCar;
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        lCurrentStatus.maePlayerTeam[liCar] = static_cast<s32>(lpOnlineScoringOutputInterface->maePlayerTeam[liCar]);

    // No blanket zero fill: original unused checkpoint-tail bytes are not initialized.
    EActiveRaceCarIndex leRunner = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    if (leGameMode == E_MODE_ONLINE_BURNING_HOME_RUN)
        leRunner = GetOnlineRunnerRaceCarIndex(lpScoringOutputInterface, lpOnlineScoringOutputInterface);
    if (leRunner == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        lCurrentStatus.miNumRemainingCheckpoints = 0;
    else
    {
        CGS_ASSERT_AT(E_ACTIVE_RACE_CAR_INDEX_COUNT > leRunner, "E_ACTIVE_RACE_CAR_INDEX_COUNT > leRunnerActiveRaceCarIndex", CGS_ARTIST_FILE, 505);
        CGS_ASSERT_AT(E_ACTIVE_RACE_CAR_INDEX_0 <= leRunner, "E_ACTIVE_RACE_CAR_INDEX_0 <= leRunnerActiveRaceCarIndex", CGS_ARTIST_FILE, 506);
        lCurrentStatus.miNumRemainingCheckpoints = lpScoringOutputInterface->maCarCheckpointData[leRunner]
            .GetAllRemainingCheckpointIndexes(lCurrentStatus.maiRemainingCheckpointIndexes);
    }
    PushGuiEvent(lCurrentStatus, lpGuiInput); // 492 / 120

    const bool lbShowtime = leGameMode == E_MODE_OFFLINE_SHOWTIME || leGameMode == E_MODE_ONLINE_SHOWTIME;
    if (lbShowtime || leGameMode != E_MODE_NONE)
    {
        BrnGui::GuiEventScoreUpdate lScore;
        lScore.meCurrentMedalTarget = static_cast<s32>(lpScoringOutputInterface->meCurrentMedalTarget);
        lScore.mfModeTime = lpScoringOutputInterface->mfModeTimeElapsed;
        lScore.mfDistanceToNextCheckpoint = 0.0f;
        if (IsPlayerRaceCarIndexInRange(liPlayer))
            lScore.mfDistanceToNextCheckpoint = lpScoringOutputInterface->maCarScoreData[liPlayer].GetDistanceToNextCheckpointLive();
        switch (leGameMode)
        {
        case E_MODE_ROAD_RAGE: case E_MODE_STUNT_ATTACK: case E_MODE_ONLINE_FUGITIVE:
        case E_MODE_ONLINE_FREE_BURN: case E_MODE_ONLINE_MODE_END:
            lScore.mfModeTime = lpScoringOutputInterface->mfModeTimeRemaining >= 0.0f
                ? lpScoringOutputInterface->mfModeTimeRemaining : 0.0f;
            break;
        default: break;
        }
        lScore.mfCurrentTargetModeTime = lpScoringOutputInterface->mfCurrentTargetModeTime;
        lScore.mbTimerActive = lpScoringOutputInterface->mbTimerActive;
        PushGuiEvent(lScore, lpGuiInput); // 424 / 20; native padding is not zero-filled

        switch (leGameMode)
        {
        case E_MODE_OFFLINE_SHOWTIME: case E_MODE_ONLINE_SHOWTIME:
        {
            BrnGui::GuiCrashScoreUpdate lScore;
            lScore.miCarsCrashed = lpScoringOutputInterface->miShowtimeCarsCrashed;
            lScore.mfDistanceTravelled = lpScoringOutputInterface->mfShowtimeDistanceTravelled;
            lScore.miComboMultiplier = lpScoringOutputInterface->miShowtimeComboMultiplier;
            lScore.miScoreMultiplier = lpScoringOutputInterface->miShowtimeScoreMultiplier;
            PushGuiEvent(lScore, lpGuiInput); // 434 / 16
            break;
        }
        case E_MODE_ROAD_RAGE:
        {
            BrnGui::GuiRoadRageScoreUpdate lScore;
            lScore.miCurrentTakedowns = lpScoringOutputInterface->miRoadRageNumTakedowns;
            lScore.miTargetTakedowns = lpScoringOutputInterface->miRoadRageTakedownTarget;
            PushGuiEvent(lScore, lpGuiInput); // 426 / 8
            break;
        }
        case E_MODE_PURSUIT:
        {
            BrnGui::GuiPursuitScoreUpdate lScore;
            lScore.miRivalDamage = lpScoringOutputInterface->miPursuitCarDamageLeft;
            PushGuiEvent(lScore, lpGuiInput); // 432 / 4
            break;
        }
        case E_MODE_STUNT_ATTACK: case E_MODE_TRAFFIC_ATTACK: case E_MODE_ONLINE_FUGITIVE:
        case E_MODE_ONLINE_FREE_BURN: case E_MODE_ONLINE_MODE_END:
        {
            BrnGui::GuiAttackScoreUpdate lScore;
            lScore.miCurrentScore = lpScoringOutputInterface->miCurrentScore;
            lScore.mfComboWarningTimeActive = lpScoringOutputInterface->mfComboWarningTimeActive;
            lScore.miTargetScore = lpScoringOutputInterface->miTargetScore;
            lScore.miComboScore = lpScoringOutputInterface->miComboScore;
            lScore.miComboMultiplier = lpScoringOutputInterface->miComboMultiplier;
            lScore.muCurrentStunts = lpScoringOutputInterface->muCurrentStunts;
            lScore.muAllStunts = lpScoringOutputInterface->muAllStunts;
            lScore.mbComboWarningActive = lpScoringOutputInterface->mbComboWarningActive;
            lScore.mbComboInProgress = lpScoringOutputInterface->mbComboInProgress;
            lScore.meStuntToDisplayType = static_cast<s32>(lpScoringOutputInterface->maStunts[0].meStuntType);
            lScore.miStuntToDisplayScore = lpScoringOutputInterface->maStunts[0].miStuntScore;
            PushGuiEvent(lScore, lpGuiInput); // 428 / 40
            break;
        }
        default: break;
        }
    }

    if (lpScoringOutputInterface->mbIsOnlineGameMode && lpScoringOutputInterface->miNumPlayersInGame > 0)
    {
        // Do not call Clear(): this stack instance only initializes its Time members
        // before the original assignments; +2C, unused records/awards and padding remain unwritten.
        BrnGui::GuiEventOnlinePostEvent lPostEvent;
        CarScoreData lCarScoreData;
        for (s32 liTeam = 0; liTeam < E_PLAYER_TEAM_COUNT; ++liTeam)
            lPostEvent.maHeader[liTeam] = static_cast<u32>(lpScoringOutputInterface->maiTeamStuntScores[liTeam]);
        lPostEvent.miNumPlayersInEvent = lpScoringOutputInterface->miNumPlayersInGame;
        lPostEvent.miNumPlayersFinishedEvent = 0;
        lPostEvent.miNumAwardsGiven = 0;
        BrnGui::GuiEventOnlinePostEvent::Record* lpRecord = lPostEvent.maRecords;
        for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
             leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leCar++)
        {
            if (lpScoringOutputInterface->mabValid[leCar])
            {
                lCarScoreData = lpScoringOutputInterface->maCarScoreData[leCar];
                const CgsSystem::Time lFinishTime = lCarScoreData.GetFinishTime();
                lpRecord->meActiveRaceCarIndex = leCar;
                lpRecord->mfFinishingTime = static_cast<f32>(lFinishTime.GetSeconds()) + lFinishTime.GetFraction();
                lpRecord->mFinishingTime = lFinishTime;
                lpRecord->mfDistanceFromFinish = lCarScoreData.GetDistanceToFinish();
                lpRecord->muValue14 = static_cast<u32>(lCarScoreData.GetOnlineStuntScore());
                lpRecord->miPointsFromThisRound = static_cast<s32>(lCarScoreData.GetOnlineFinishPositionScore());
                lpRecord->miResultsTablePosition = static_cast<s32>(lCarScoreData.GetOnlineStandingsPosition());
                lpRecord->miTotalPoints = static_cast<s32>(lpScoringOutputInterface->maiCumulativeScoreData[leCar]);
                lpRecord->mbHasTimedOut = lCarScoreData.GetTimedOut();
                lpRecord->mbHasDisconnected = lCarScoreData.GetDisconnected();
                lpRecord->mbWasEliminated = lCarScoreData.GetEliminatorRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID;
                lpRecord->miEliminations = static_cast<s32>(lCarScoreData.GetNumEliminations());
                lpRecord->muValue28 = lCarScoreData.GetOnlinePostEventValueC0();
                lpRecord->muValue30 = static_cast<u32>(lCarScoreData.GetCumulativeCheckpoints());
                if (lpRecord->mfFinishingTime > 0.0f || lpRecord->mbHasTimedOut || lpRecord->mbHasDisconnected)
                    ++lPostEvent.miNumPlayersFinishedEvent;
                if (static_cast<s32>(lpOnlineScoringOutputInterface->maOnlineAwards[leCar]) != -1)
                {
                    BrnGui::GuiEventOnlinePostEvent::OnlineAward& lrAward = lPostEvent.maOnlineAwards[lPostEvent.miNumAwardsGiven];
                    lrAward.meOnlineAwardID = static_cast<s32>(lpOnlineScoringOutputInterface->maOnlineAwards[leCar]);
                    lrAward.mePlayerActiveRaceCarIndex = leCar;
                    lrAward.miAwardVariable = lpOnlineScoringOutputInterface->maiOnlineAwardVariables[leCar];
                    ++lPostEvent.miNumAwardsGiven;
                }
                ++lpRecord;
            }
        }
        PushGuiEvent(lPostEvent, lpGuiInput); // 318 / 568
    }
}
}

// FOLDED FROM GameBridgeGameStateToX_Sound.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_Sound.cpp
//
// SIBLING SPLIT of GameBridgeGameStateToX.cpp (the established precedent --
// see GameBridgeGameStateToX_TrainingStringIds.cpp; when this split was made
// the parent TU was not mountable, which it now is).
// BridgeGameStateToSound gained its caller
// with the faithful-audio-engine phase C4b in-game leg (DoUpdate_Sound
// @0x823DCEC0 is its ONLY console call site), so it is MOVED -- not copied --
// here so it can be mounted on its own. Folding it back later is a delete.
//
// Bodied here (1 ledger function):
//   BrnGame::BrnGameModule::BridgeGameStateToSound @ 0x823CDE50  (DWARF home
//   GameSource/Game/GameBridgeGameStateToX.cpp)
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"   // RootInputBuffer + the interface twins
#include "GameSource/GameState/BrnGameStateModuleIO.h"      // GameStateModuleIO::OutputBuffer accessors

namespace BrnGame
{
    // @ 0x823CDE50 (bodied 2026-08-25, faithful-audio-engine phase C3b). The
    // game-state -> sound input bridge (the caller holds the game-state output's
    // read lock + the sound input's write lock):
    //   [1] the 13312 game-action queue append (both ends are the same
    //       VariableEventQueue<13312,16> instantiation -- the console append
    //       symbol pins them; the root side's GetGameActionQueue write accessor).
    //   [2] the three interface installs -- game-mode (+176344), scoring
    //       (+173240), online-scoring (+175976), each opaque view cast onto its
    //       root-side twin (same console record, spans match).
    //   [3] the UpdateInfo byte: actual current/previous stall flags at9A0628/27.
    void BrnGameModule::BridgeGameStateToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundInputBuffer,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer)
    {
        typedef BrnSound::Module::Io::RootInputBuffer RootIn;

        lpSoundInputBuffer->GetGameActionQueue().Append(
            *lpGameStateOutputBuffer->GetGameActionQueue());

        lpSoundInputBuffer->SetGameModeInterface(
            reinterpret_cast<const RootIn::GameModeOutputInterface*>(
                lpGameStateOutputBuffer->GetGameModeOutputInterface()));
        lpSoundInputBuffer->SetScoringInterface(
            reinterpret_cast<const RootIn::ScoringOutputInterface*>(
                lpGameStateOutputBuffer->GetScoringOutputInterface()));
        lpSoundInputBuffer->SetOnlineScoringInterface(
            reinterpret_cast<const RootIn::OnlineScoringOutputInterface*>(
                lpGameStateOutputBuffer->GetOnlineScoringOutputInterface()));

        RootIn::UpdateInfo lUpdateInfo;
        lUpdateInfo.mData[0] = (mbStalled || mbPrevStalled) ? 1 : 0;
        lpSoundInputBuffer->SetUpdateInfo(&lUpdateInfo);
    }
}

// FOLDED FROM GameBridgeGameStateToX_StuntGuiEvents.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp
//
// ⭐⭐ [gateui] THE GAME-STATE -> GUI STUNT-COLLECTIBLE ARMS. Two BrnGame::BrnGameModule
// members whose DWARF home is GameSource/Game/GameBridgeGameStateToX.cpp:
//
//   MapStuntEnumsFromGameplayToGui   0x823AA4A8   (a real X360 symbol, whole)
//   TranslateGameActionsToGuiEvents  0x823E9CE0   (PARTIAL; see split-dispatch inventory)
//
// WHY A SIBLING TU RATHER THAN THE PARENT FILE. Historical: GameBridgeGameStateToX.cpp
// neither compiled nor linked when this slice landed. Both are fixed -- the parent TU
// compiles and is MOUNTED, its takedown translator having dropped the two invented
// accessors that were half of its link hole. What is still un-homed lives in the OTHER
// sibling, GameBridgeGameStateToX_Controller.cpp (see its banner for the four symbols),
// which is why that one is unmounted and this one is not. The functions HERE are called
// every sub-step by BrnGameModule::Update, so they must stay linkable. Split, exactly as
// ConvertTrainingTypeToStringId was split into GameBridgeGameStateToX_TrainingStringIds.cpp
// on 2026-08-16 -- MOVED, not copied, so folding these siblings back into the parent is a
// delete, not a duplicate-symbol hunt.
//
// The shared GUI-event push (PushGuiEvent) lives in GameBridgeGameStateToX.h so both TUs
// use one copy; its banner explains why the queue is written directly instead of through
// CgsGui::GuiModule::AddGuiEvent<T>.
// ============================================================================

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // original formatted rivalry assertions
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"               // BrnGui::GuiAutosaveRequestEvent (id 356)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // BrnGui::StuntType / GuiEventStunt*
#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h" // GuiEventRankProgressResponse (case 181)
#include "GameSource/Gui/Events/BrnGuiEventStatsResponse.h"        // GuiEventStatsResponse (case 180)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"        // GameStateModuleIO::GameStats (the case-180 record)
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // ProgressionManager / Profile (case 180)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                  // GenericRegion::Type (the drive-thru set query)
#include "GameSource/GameState/BrnGameActions.h"                   // the action payload homes
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // InviteOrJoinParams (action 92)
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer / GameActionQueue
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Gui/CgsGuiModule.h"               // streaming handshake publishers
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint
#include "GameSource/Gui/Events/BrnGuiChallengeEvents.h"           // GUI 574 / 576 / 577 (free-burn challenge arms)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"           // CgsSystem::Time (the action-152 record)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                          // ModeManager::GetScoringSystem (case 279)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"                // ScoringSystem::GetCrashScorer (case 279)
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"  // CrashModeScoring (case 279)
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"                  // HUDMessageLogic::ComboPerformedMessage (case 133)
#include <cstring>                                                 // memset/strncpy (the 537 ticker record)

namespace BrnGame
{
namespace
{
    // =========================================================================
    // ⭐⭐ [tut-ticker] the on-queue record of GUI event 537, TU-LOCAL (the exact precedent:
    // BrnCarSelectLivery_Components.cpp's GuiTickerCustomMessagePayload -- the canonical
    // BrnGui::GuiEventTickerCustomMessage home is in BrnGuiDemangledEventTypes.h, whose
    // opaque 12B-header shape does NOT match the wire: the X360
    // AddGuiEvent<GuiEventTickerCustomMessage> @0x823D1D08 posts `AddEvent(q, a2, 537, 2072)`
    // with a2 = the PAYLOAD BASE (case 148 hands it `addi r4, r1, var_2680`), i.e. the queued
    // 2072 bytes open with maiStringTypes, not with a GuiEvent header).
    // Layout recovered from GuiEventTickerCustomMessage::AddString @0x823A6940 (count at
    // +0x810 == a1+2064, types stride 4 at +0, strings stride 512 at +0x10; its three asserts
    // bake BrnGuiEventTypeDefs.h:390/391/392).
    // =========================================================================
    struct TickerCustomMessageWire537
    {
        s32  maiStringTypes[4];                 // +0x000
        char maacStrings[4][512];               // +0x010
        s8   mi8NumStrings;                     // +0x810
        u8   maFlags[4];                        // +0x811
        u8   maPad815[3];                       // +0x815

        s32 GetEventType() const { return 537; }

        // X360 0x823A6940, transcribed (the console's own bounds asserts, then
        // strncpy(base + 0x10 + count*512, str, 512); types[count] = type; ++count).
        void AddString(const char* lpString, s32 liType)
        {
            CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                        // h:390
            CGS_ASSERT(mi8NumStrings < 4, "mi8NumStrings < KI_MAX_NUM_STRINGS");         // h:391
            CGS_ASSERT(lpString != 0, "lpString");                                       // h:392
            std::strncpy(maacStrings[mi8NumStrings], lpString, 512);
            maiStringTypes[mi8NumStrings] = liType;
            ++mi8NumStrings;
        }
    };
    CGS_STATIC_ASSERT((sizeof(TickerCustomMessageWire537) == 2072),
                  "X360 AddGuiEvent<GuiEventTickerCustomMessage> posts 2072 bytes (id 537)");

    // GUI event 356 is posted as one byte, id 356 size 1, through its canonical record.
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiAutosaveRequestEvent) == 1), "id 356 size 1");

    // ARTIST case 224 calls the one-byte AddGuiEvent<GuiEvent<152>> instantiation.
    // Keep this one-byte legacy tag local: the normal GUI 152 header is 12 bytes and
    // GuiHudMessage uses it, while this translator passes only its untouched stack byte.
    // No matching source consumer for this one-byte route is claimed.
    struct GuiEvent152UnwrittenByte
    {
        u8 mu8Unwritten;
        s32 GetEventType() const { return 152; }
    };
    CGS_STATIC_ASSERT((sizeof(GuiEvent152UnwrittenByte) == 1),
                  "ARTIST action 224 posts GUI 152 as one unwritten byte");

    // =========================================================================
    // [drive-thru wave 2026-08-29] The byte the BODY-SHOP drive-thru action (97) carries its
    // "the repair did something" flag in. The 144-byte shop payload is a serialised action
    // blob, not a C++ class: transform @+0x00, an identity block @+0x40, per-action scalars
    // from +0x80. For action 97 the producer (DriveThruManager::PostShopAction) writes the
    // player entity id at +0x80, the not-online flag at +0x84 and this byte at +0x85, and the
    // console's bridge arm reads exactly it back (`lbz r11, 0x85(r31)` @0x823EB5C8).
    // =========================================================================
    const s32 KI_SHOP_ACTION_EFFECTIVE_BYTE = 0x85;

    // =========================================================================
    // ⭐⭐⭐ [boost-ticker wave 2026-09-14] THE SIX BOOST-TICKER GUI WIRE RECORDS, TU-LOCAL. The
    // canonical BrnGui::GuiNearMissEvent / GuiDriftingEvent / GuiSpinningEvent / GuiInAirEvent /
    // GuiOncomingEvent / GuiTailgatingEvent / GuiTrafficCheckEvent live in
    // BrnGuiDemangledEventTypes.h (now included above). Each record below is field-for-field its
    // canonical twin -- no fork of meaning, only of scope (file-static in an anonymous
    // namespace, so there is no ODR fork either).
    //
    // WIRE PROOF for every id + size: the console's own AddGuiEvent<T> instantiation, whose
    // ONLY xref is TranslateGameActionsToGuiEvents @0x823E9CE0:
    //     GuiTrafficCheckEvent 0x823D3F88 -> AddEvent(q, e, 383, 4)
    //     GuiNearMissEvent     0x823D4040 -> AddEvent(q, e, 384, 8)
    //     GuiDriftingEvent     0x823D40F8 -> AddEvent(q, e, 385, 4)
    //     GuiSpinningEvent     0x823D41B0 -> AddEvent(q, e, 386, 4)
    //     GuiInAirEvent        0x823D4268 -> AddEvent(q, e, 387, 8)
    //     GuiOncomingEvent     0x823D4320 -> AddEvent(q, e, 388, 4)
    //     GuiTailgatingEvent   0x823D43D8 -> AddEvent(q, e, 389, 4)
    // and the far end is BrnGui::BoostMessageManager::RecvEvent @0x824204E8, whose cases
    // 383..389 read exactly these fields (already committed, already routed by
    // BrnRaceMainHudState_wS3.cpp's LABEL_120).
    // =========================================================================
    struct TrafficCheckEventWire383
    {
        s32 miCount;                      // +0x00
        s32 GetEventType() const { return 383; }
    };
    struct NearMissEventWire384
    {
        s32 miCount;                      // +0x00
        s32 meNearMissType;               // +0x04 (BrnWorld::ENearMissType; RecvEvent treats
                                          //        2 and 3 as the crash-escape flavours)
        s32 GetEventType() const { return 384; }
    };
    struct DriftingEventWire385
    {
        f32 mfDistance;                   // +0x00
        s32 GetEventType() const { return 385; }
    };
    struct SpinningEventWire386
    {
        f32 mfSpinAngle;                  // +0x00
        s32 GetEventType() const { return 386; }
    };
    struct InAirEventWire387
    {
        f32 mfCumulativeAirTime;          // +0x00
        f32 mfCurrentJumpAirTime;         // +0x04
        s32 GetEventType() const { return 387; }
    };
    struct OncomingEventWire388
    {
        f32 mfDistance;                   // +0x00
        s32 GetEventType() const { return 388; }
    };
    struct TailgatingEventWire389
    {
        f32 mfDistance;                   // +0x00
        s32 GetEventType() const { return 389; }
    };
    CGS_STATIC_ASSERT((sizeof(TrafficCheckEventWire383) == 4), "GUI 383 size 4");
    CGS_STATIC_ASSERT((sizeof(NearMissEventWire384)     == 8), "GUI 384 size 8");
    CGS_STATIC_ASSERT((sizeof(DriftingEventWire385)     == 4), "GUI 385 size 4");
    CGS_STATIC_ASSERT((sizeof(SpinningEventWire386)     == 4), "GUI 386 size 4");
    CGS_STATIC_ASSERT((sizeof(InAirEventWire387)        == 8), "GUI 387 size 8");
    CGS_STATIC_ASSERT((sizeof(OncomingEventWire388)     == 4), "GUI 388 size 4");
    CGS_STATIC_ASSERT((sizeof(TailgatingEventWire389)   == 4), "GUI 389 size 4");

    // =========================================================================
    // [crash-parity 2026-09-22] the on-queue record of GUI event 374
    // (BrnGui::GuiShutdownFinishedEvent), TU-LOCAL like the records above; the canonical home
    // is BrnGuiDemangledEventTypes.h.
    // Wire shape: AddGuiEvent<GuiShutdownFinishedEvent> @0x823D8A48 posts ONE byte
    // (`li r5, 0x176 ; li r6, 1` @0x823D8AE4..0x823D8AE8).
    // =========================================================================
    struct ShutdownFinishedEventWire374
    {
        u8 mu8Unwritten;                        // +0x00 (never written by the console's arm)
        s32 GetEventType() const { return 374; }
    };
    CGS_STATIC_ASSERT((sizeof(ShutdownFinishedEventWire374) == 1), "GUI 374 size 1");

    // [FX-FLOW 2026-09-24, G13-X5 remainder] the on-queue record of GUI event 373
    // (BrnGui::GuiShutdownEvent, DWARF BrnGuiEventTypeDefs.h:3618 {CgsID mVictimCarID}), TU-LOCAL
    // beside its 374 sibling. Wire shape: AddGuiEvent<GuiShutdownEvent>
    // @0x823D8990 posts the 8-byte id (`li r6, 8 ; li r5, 0x175` @0x823D8A2C..0x823D8A30).
    struct ShutdownEventWire373
    {
        CgsID mVictimCarID;                     // +0x00
        s32 GetEventType() const { return 373; }
    };
    CGS_STATIC_ASSERT((sizeof(ShutdownEventWire373) == 8), "GUI 373 size 8");

    // [FX-FLOW 2026-09-24, G11-D1 remainder] the on-queue record of GUI event 421
    // (BrnGui::GuiInEventNeckAndNeck, no members), TU-LOCAL
    // (its home is BrnGuiDemangledEventTypes.h). Wire shape:
    // AddGuiEvent<GuiInEventNeckAndNeck> @0x823D5688 posts ONE byte (`li r6, 1 ; li r5, 0x1A5`
    // @0x823D5724..0x823D5728); the translator's case 246 hands it a stack byte it never writes.
    struct NeckAndNeckEventWire421
    {
        u8 mu8Unwritten;                        // +0x00 (never written by the console's arm)
        s32 GetEventType() const { return 421; }
    };
    CGS_STATIC_ASSERT((sizeof(NeckAndNeckEventWire421) == 1), "GUI 421 size 1");


    // The free-burn challenge GUI records the 152..160 arms post, TU-local (their canonical
    // opaque homes are in BrnGuiDemangledEventTypes.h). Member
    // names are the original ones. Sizes are the console AddGuiEvent<T> sizes.
    struct OnlineTimeoutWire108
    {
        f32 mfTimeLeft;                         // +0x00
        s32 GetEventType() const { return 108; }
    };
    CGS_STATIC_ASSERT((sizeof(OnlineTimeoutWire108) == 4), "GUI 108 size 4");

    struct ShowFreeburnChallengeWire582
    {
        CgsID mChallengeID;                     // +0x00
        s32 GetEventType() const { return 582; }
    };
    CGS_STATIC_ASSERT((sizeof(ShowFreeburnChallengeWire582) == 8), "GUI 582 size 8");

    struct FburnChallengeEveryPlayerStatusWire581
    {
        BrnGameState::GameStateModuleIO::FburnChallengeEveryPlayerStatusData mStatus;   // +0x00
        s32 GetEventType() const { return 581; }
    };
    CGS_STATIC_ASSERT((sizeof(FburnChallengeEveryPlayerStatusWire581) == 2104), "GUI 581 size 2104");

    // GUI 104, 579 and 584 are tag-only GuiEvent<N> records: the console queues one
    // unwritten byte for each.
    void PushGuiTag(s32 liEventId, CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
    {
        const u8 lu8Tag = 0;
        lpGuiInput->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lu8Tag), liEventId, 1);
    }
}

    // =========================================================================
    // ⭐ [gateui] BrnGameModule::MapStuntEnumsFromGameplayToGui  @ X360 0x823AA4A8
    //
    // The gameplay-side StuntElementType -> BrnGui::StuntType map. Asm-exact
    // (@0x823AA4B8..0x823AA57C): `cmplwi r27,1 / blt -> 0 / beq -> 1 / cmplwi r27,3 /
    // blt -> 2` and otherwise the streamed assert then `li r3,3`. An UNSIGNED compare, so a
    // negative gameplay value takes the assert arm, not the `< 1` one.
    // The assert text is built with the offending value streamed in
    // ("Invalid Stunt Enum : " << v << "\n") at GameBridgeGameStateToX.cpp:404.
    // It is a non-static member whose body never touches `this` -- reproduced as declared.
    // =========================================================================
    BrnGui::StuntType BrnGameModule::MapStuntEnumsFromGameplayToGui(u32 luGameplayStuntType) const
    {
        if (luGameplayStuntType < 1u)
        {
            return BrnGui::E_STUNTTYPE_JUMP;    // 0
        }
        if (luGameplayStuntType == 1u)
        {
            return BrnGui::E_STUNTTYPE_SMASH;   // 1
        }
        if (luGameplayStuntType < 3u)
        {
            return BrnGui::E_STUNTTYPE_STUNT;   // 2
        }

        // The console streams the offending value into the message before firing.
        CGS_FAIL("Invalid Stunt Enum");
        return BrnGui::E_STUNTTYPE_COUNT;       // 3 -- out of range on purpose (the analyzer
                                                //      bound-asserts it downstream)
    }

    // =========================================================================
    // ⭐⭐ [gateui] BrnGameModule::TranslateGameActionsToGuiEvents  @ X360 0x823E9CE0
    //
    // Drain the game-state output buffer's GameActionQueue (VariableEventQueue<13312,16>,
    // OutputBuffer +0x04, accessor X360 sub_823B96F0) and post the matching BrnGui event for
    // each recognised game action. Signature from the asm prologue @0x823E9D04..0x823E9D10
    // (r29 = this, r20 = lpGuiInput, r31 = lpGameStateOutput); the two null asserts are
    // GameBridgeGameStateToX.cpp:754/755. The queue walk is the console's own
    // GetFirstEvent / GetNextEvent pair (@0x823E9D88), and `r30 = 0x6EAA20` (7252512) is the
    // embedded CgsGui::GuiModule every arm pushes through -- see PushGuiEvent above for why
    // the push is written against the input buffer's queue instead.
    // Sole caller: BridgeGameStateToGui @0x823EE880 (call site @0x823EF22C).
    //
    // PARTIAL RECONSTRUCTION. ARTIST has 188 unique explicit top-level action ids.
    // This source snapshot represents 140/188; the report records each handled arm and the
    // 48 explicit ids still unrepresented. The disassembly's separate default-arm list remains
    // default behavior and is not conflated with the explicit-case inventory.
    // This count concerns action ids, independent of debug-menu registration counts. The queue
    // walk is active every frame in the current PC source, so omitted live actions remain gaps;
    // do not infer a no-op from a caption or missing direct xref.
    //
    // ⓘ ACTION IDS ARE THE X360 ONES, taken from the enum, never from literals:
    // BrnGameActions.h carries E_ACTION_ON_STUNT_ELEMENT_COMPLETE = 58 / _FOR_COUNTY = 59 /
    // _BY_TYPE = 60 (the PS3 DWARF values are 53/54/55 -- X360 == DWARF + 5 in this range).
    // Game EVENT ids are NOT shifted; only ACTION ids are.
    // =========================================================================
    void BrnGameModule::TranslateGameActionsToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput)
    {
        CGS_ASSERT_AT(lpGuiInput != 0, "lpGuiInput", CGS_ARTIST_FILE, 754);                   // :754
        CGS_ASSERT_AT(lpGameStateOutput != 0, "lpGameStateOutput", CGS_ARTIST_FILE, 755);     // :755
        if (lpGuiInput == 0 || lpGameStateOutput == 0)
        {
            return;
        }

        const BrnGameState::GameStateModuleIO::GameActionQueue* lpActionQueue =
            lpGameStateOutput->GetGameActionQueue();
        if (lpActionQueue == 0)
        {
            return;
        }


        // ⭐ [minimap blips, issue #9, 2026-09-07] THE PENDING SAT-NAV RECORD. The console keeps ONE
        // GuiEventUpdateSatNav (2320 bytes) on its frame across the whole drain, with its icon count
        // zeroed at entry; case45 appends into it and the tail posts it once when nonempty.
        BrnGui::GuiEventUpdateSatNav lPendingSatNavEvent;
        s32 liPendingSatNavIcons = 0;

        const CgsModule::Event* lpAction     = 0;
        s32                     liActionSize = 0;
        s32                     liActionType = lpActionQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != 0)
        {
            switch (liActionType)
            {
            // ARTIST 0x823ED21C..0x823ED270: player-car change.
            case 1:
                CGS_ASSERT(*reinterpret_cast<const CgsID*>(lpAction) != 0,
                           "lChangeCarEvent.mCarId != kCGSID_NULL");
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 415, sizeof(CgsID));
                break;
            // ARTIST 0x823ED278..0x823ED2CC: a car unlock is a separate
            // notification; it must not replace the car currently being driven.
            case 2:
                CGS_ASSERT(*reinterpret_cast<const CgsID*>(lpAction) != 0,
                           "lCarUnlockEvent.mCarId != kCGSID_NULL");
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 76, sizeof(CgsID));
                break;
            // ARTIST 0x823ED2D4..0x823ED328: a respawn also publishes the
            // selected model from ResetPlayerCarAction+0x20.
            case BrnGameState::GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR:
            {
                const BrnGameState::GameStateModuleIO::ResetPlayerCarAction& lrReset = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::ResetPlayerCarAction*>(lpAction);
                CGS_ASSERT(lrReset.mCarModelId != 0,
                           "lChangeCarEvent.mCarId != kCGSID_NULL");
                lpGuiInput->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lrReset.mCarModelId),
                    415, sizeof(CgsID));
                break;
            }
            // ARTIST823EBBD0..823EBC0C: request the GUI's fourth streaming
            // reply, then translate the loading-state byte without changing the wait.
            case BrnGameState::GameStateModuleIO::E_ACTION_WAIT_FOR_STREAMING:
            {
                CgsGui::GuiEvent<134> lEvent;
                mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_SET_LOADING_STATE:
            {
                if (*reinterpret_cast<const u8*>(lpAction) != 0)
                {
                    CgsGui::GuiEvent<136> lEvent;
                    mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                }
                else
                {
                    CgsGui::GuiEvent<137> lEvent;
                    mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                }
                break;
            }
            // ARTIST 0x823EBBA4..0x823EBBC8 copies the native action's two pointer words and
            // availability byte into the typed 12-byte X360 GUI 350 record. The named action and
            // event types widen their pointers together on PC; no second profile producer is added.
            case BrnGameState::GameStateModuleIO::E_ACTION_PROGRESSION_PROFILE_DATA:
            {
                const BrnGameState::GameStateModuleIO::ProgressionProfileDataAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::ProgressionProfileDataAction*>(lpAction);
                BrnGui::GuiEventProgressionProfileData lEvent;
                lEvent.mpProfile = lrAction.mpProgressionProfile;
                lEvent.mpProgressionData = lrAction.mpProgressionData;
                lEvent.mbRoadRulesAvailable = lrAction.mbAreRoadRulesAvailable;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED838-0x823ED84C: release the GUI's model-change wait.
            case 66:
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 565, sizeof(CgsID));
                break;

            // ARTIST 0x823EC5C8..0x823EC610, action 11: preserve the two setter calls and their
            // order. The 8-byte GUI record is {active-car index, network player id} (event 267).
            case BrnGameState::GameStateModuleIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED:
            {
                CGS_ASSERT(lpAction != 0, "lpNetworkRemotePlayerDisconnectAction");
                const BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::RemotePlayerDisconnectedAction*>(lpAction);
                BrnGui::GuiNetworkRemotePlayerDisconnectEvent lEvent = BrnGui::GuiNetworkRemotePlayerDisconnectEvent();
                lEvent.SetNetworkPlayerID(lrAction.mPlayerID);
                lEvent.SetActiveRaceCarIndex(lrAction.meActiveRaceCarIndex);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB074..0x823EB0E4, action 15 -> GUI 390 / 32 bytes. The final three
            // source bytes are unwritten stack tail in the console's local; the typed event's
            // padding stays zero here, while every defined field follows the original stores.
            case 15:
            {
                CGS_ASSERT(lpAction != 0, "lpCompletedStuntAction != NULL");
                const BrnGameState::GameStateModuleIO::CompletedStuntAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::CompletedStuntAction*>(lpAction);
                BrnGui::GuiCompletedStuntEvent lEvent = BrnGui::GuiCompletedStuntEvent();
                lEvent.muStuntActionComplete = lrAction.muStuntActionComplete;
                lEvent.mfCompletedBarrelRollAngle = lrAction.mfCompletedBarrelRollAngle;
                lEvent.mfCompletedAirSpinAngle = lrAction.mfCompletedAirSpinAngle;
                lEvent.mfCompletedHandbreakTurnAngle = lrAction.mfCompletedHandbreakTurnAngle;
                std::memcpy(&lEvent.mbSuccessfulLanding, &lrAction.mbSuccessfulLanding,
                            sizeof(lEvent.mbSuccessfulLanding));
                lEvent.mfCompletedDriftTime = lrAction.mfCompletedDriftTime;
                lEvent.mfCompletedDriftDistance = lrAction.mfCompletedDriftDistance;
                lEvent.miCompletedBarrelRolls = lrAction.miCompletedBarrelRolls;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB104..0x823EB148, action 16 -> GUI 391 / 24 bytes. The action's
            // console-only tail (+0x18/+0x1C) is not part of the GUI record and is not forwarded.
            case 16:
            {
                CGS_ASSERT(lpAction != 0, "lpInProgressStuntAction != NULL");
                const BrnGameState::GameStateModuleIO::InProgressStuntAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::InProgressStuntAction*>(lpAction);
                BrnGui::GuiInProgressStuntEvent lEvent = BrnGui::GuiInProgressStuntEvent();
                lEvent.muStuntActionInProgress = lrAction.muStuntActionInProgress;
                lEvent.mfInProgressBarrelRollAngle = lrAction.mfInProgressBarrelRollAngle;
                lEvent.mfInProgressAirSpinAngle = lrAction.mfInProgressAirSpinAngle;
                lEvent.mfInProgressHandbreakTurnAngle = lrAction.mfInProgressHandbreakTurnAngle;
                lEvent.mfInProgressDriftTime = lrAction.mfInProgressDriftTime;
                lEvent.mfInProgressDriftDistance = lrAction.mfInProgressDriftDistance;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EAD6C..0x823EAD78 posts the canonical one-byte GuiEvent<91> tag.
            // Its payload is unwritten; preserve that representation without assigning a value.
            case 22:
            {
                CgsGui::GuiEvent<91> lEvent;
                mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EACB0..0x823EACB8 compresses BRNFBFSM and discards the result; it
            // posts no GUI record. Preserve that original leftover explicitly.
            case 25:
                (void)CgsIDCompress("BRNFBFSM");
                break;

            // ARTIST 0x823EACBC..0x823EAD1C, case 26 gates on words {2 or 16, not 10}, then
            // posts RunFsm (GUI 144 / 24).
            case 26:
            {
                const s32* lpActionWords = reinterpret_cast<const s32*>(lpAction);
                if ((lpActionWords[0] == 2 || lpActionWords[0] == 16) && lpActionWords[1] != 10)
                {
                    BrnGui::GuiEventRunFsm lEvent = BrnGui::GuiEventRunFsm();
                    lEvent.mFsmId = CgsIDCompress("BRNEVENTFSM");
                    lEvent.mInitialStateId = 0;
                    lEvent.meFsmToRun = BrnGui::E_GUI_HUD_EVENT;
                    lEvent.meFlowToUse = BrnGui::E_GUIFLOW_HUD;
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                break;
            }

            // ARTIST 0x823EB390..0x823EB3A0, action 32 reaches the shared label that writes
            // E_SPLASH_STATE_STOP (1), then posts GUI 269 / 4 bytes.
            case 32:
            {
                BrnGui::GuiEventNetworkSplashEvent lEvent = BrnGui::GuiEventNetworkSplashEvent();
                lEvent.meSplashState = BrnGui::GuiEventNetworkSplashEvent::E_SPLASH_STATE_STOP;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EAC60..0x823EACAC, case 41: byte +4 enables the post; byte +5 selects
            // the optional PAUSED state.
            case 41:
            {
                const u8* lpActionBytes = reinterpret_cast<const u8*>(lpAction);
                if (lpActionBytes[4] != 0)
                {
                    BrnGui::GuiEventRunFsm lEvent = BrnGui::GuiEventRunFsm();
                    lEvent.mFsmId = CgsIDCompress("BRNFBFSM");
                    lEvent.mInitialStateId = 0;
                    if (lpActionBytes[5] == 1)
                        lEvent.mInitialStateId = CgsIDCompress("PAUSED");
                    lEvent.meFsmToRun = BrnGui::E_GUI_HUD_FREEBURN;
                    lEvent.meFlowToUse = BrnGui::E_GUIFLOW_HUD;
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                break;
            }

            // ARTIST 0x823ED8E0..0x823ED920, action 49: the leading qword is an already-compressed
            // overlay id. Do not call the string overload or compress it a second time.
            case 49:
            {
                CGS_ASSERT(lpAction != 0, "lpPopupRequestAction");
                CgsID lOverlayId = 0;
                std::memcpy(&lOverlayId, lpAction, sizeof(lOverlayId));
                BrnGui::GuiOverlayRequest lRequest;
                lRequest.Construct(lOverlayId);
                PushGuiEvent(lRequest, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED1C8..0x823ED1E8: copy CrashComboAction's {entry type, value}
            // words to the raw 8-byte GUI 347 record. No downstream case-347 reader was found.
            case BrnGameState::GameStateModuleIO::E_ACTION_CRASH_COMBO:
            {
                const BrnGameState::GameStateModuleIO::CrashComboAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::CrashComboAction*>(lpAction);
                BrnGui::GuiCrashComboEvent lEvent = BrnGui::GuiCrashComboEvent();
                std::memcpy(lEvent.maData, &lrAction.meEntryType, sizeof(lrAction.meEntryType));
                std::memcpy(lEvent.maData + sizeof(lrAction.meEntryType), &lrAction.mfValue,
                            sizeof(lrAction.mfValue));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EC178..0x823EC1AC forwards the 24-byte StuntInfo image as GUI 429.
            case 132:
            {
                typedef BrnGameState::HUDMessageLogic::StuntPerformedMessage StuntPerformedAction;
                CGS_STATIC_ASSERT((sizeof(StuntPerformedAction) == 24), "X360 action 132 payload size");
                const StuntPerformedAction& lrAction = *reinterpret_cast<const StuntPerformedAction*>(lpAction);
                BrnGui::GuiHUDMessageStuntPerformed lEvent = BrnGui::GuiHUDMessageStuntPerformed();
                lEvent.mStuntInfo = lrAction.mStuntInfo;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EC1B0..0x823EC1DC posts GUI 430 / 8 bytes, then passes the
            // original action base to the embedded developer-challenge owner. The producer is
            // HUDMessageLogic::GenerateStuntMessage: ARTIST posts its three WasComboRecentlyPerformed
            // out-slots as one 12-byte action (f32, s32, bool), matching ComboPerformedMessage.
            case 133:
            {
                typedef BrnGameState::HUDMessageLogic::ComboPerformedMessage ComboPerformedAction;
                CGS_STATIC_ASSERT((sizeof(ComboPerformedAction) == 12), "X360 action 133 payload size");
                CGS_STATIC_ASSERT((offsetof(ComboPerformedAction, mfComboTime) == 0), "X360 action 133 float");
                CGS_STATIC_ASSERT((offsetof(ComboPerformedAction, miComboScore) == 4), "X360 action 133 score");
                CGS_STATIC_ASSERT((offsetof(ComboPerformedAction, mbValidCombo) == 8), "X360 action 133 validity");
                const ComboPerformedAction& lrAction = *reinterpret_cast<const ComboPerformedAction*>(lpAction);
                BrnGui::GuiHUDMessageComboPerformed lEvent = BrnGui::GuiHUDMessageComboPerformed();
                lEvent.miScore = lrAction.miComboScore;
                lEvent.mbBestScore = lrAction.mbValidCombo;
                PushGuiEvent(lEvent, lpGuiInput);
                GetGameStateModule().GetDeveloperChallengeManager()->OnStuntRunComboPerformed(
                    &lrAction.mfComboTime);
                break;
            }

            // ARTIST 0x823EC1E4..0x823EC1F4 posts one byte as GUI 431. The source local is
            // unwritten; HudMessageAnalyzer's case 431 ignores the byte and triggers a fixed tag.
            case 134:
            {
                BrnGui::GuiHUDMessageStuntTimeUp lEvent = BrnGui::GuiHUDMessageStuntTimeUp();
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED6F8..0x823ED710 copies ShowtimeIntroAction::mbStart (+0x10) to
            // GUI 392 / 1 byte; copy the field representation rather than converting bool.
            case BrnGameState::GameStateModuleIO::E_ACTION_SHOWTIME_INTRO_START:
            {
                const BrnGameState::GameStateModuleIO::ShowtimeIntroAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::ShowtimeIntroAction*>(lpAction);
                BrnGui::GuiShowtimeTriggered lEvent = BrnGui::GuiShowtimeTriggered();
                std::memcpy(lEvent.maData, &lrAction.mbStart, sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED758..0x823ED7B4 publishes GUI 404 / 8 bytes before selecting
            // the online or offline stunt scorer from current GameMode::mbIsOnline (+172).
            case BrnGameState::GameStateModuleIO::E_ACTION_POWER_PARK_RESULT:
            {
                const BrnGameState::GameStateModuleIO::PowerParkResultAction* const lpcAction = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::PowerParkResultAction*>(lpAction);
                BrnGui::GuiPowerParkResult lEvent = BrnGui::GuiPowerParkResult();
                lEvent.meOutcome = lpcAction->meOutcome;
                lEvent.miOverallRating = lpcAction->miOverallRating;
                PushGuiEvent(lEvent, lpGuiInput);

                BrnGameState::ModeManager* const lpModeManager = GetGameStateModule().GetModeManager();
                BrnGameState::GameMode* const lpCurrentGameMode = lpModeManager->GetCurrentGameMode();
                BrnGameState::ScoringSystem* const lpScoringSystem = lpModeManager->GetScoringSystem();
                if (lpCurrentGameMode != 0 && lpCurrentGameMode->IsOnline())
                    lpScoringSystem->GetOnlineStuntScorer()->DealWithPowerPark(lpcAction);
                else
                    lpScoringSystem->GetStuntScorer()->DealWithPowerPark(lpcAction);
                break;
            }

            // ARTIST 0x823EBFF4..0x823EC008 unconditionally fires the original assertion and
            // posts no GUI record. Preserve its original message, source path and line.
            case 185:
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "Part of offline custom events - no longer exist, and certainly not handled by GUI",
                    CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                    2284));
                CgsDev::Assert::EndAssert();
                break;

            // ARTIST 0x823ED854..0x823ED86C: online car id -> GUI 566 (8 bytes).
            case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED_ONLINE:
            {
                const BrnGameState::GameStateModuleIO::CarSelectionChangedOnlineAction* lpChanged = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::CarSelectionChangedOnlineAction*>(lpAction);
                BrnGui::GuiCarSelectionChangedOnline lEvent = BrnGui::GuiCarSelectionChangedOnline();
                std::memcpy(lEvent.maData, &lpChanged->mCarId, sizeof(lpChanged->mCarId));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA6E8..0x823EA6FC: start junkyard car select with word 1.
            case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECT_START_GUI_ON_GAME_START:
            {
                BrnGui::GuiCarSelectStartEvent lEvent = BrnGui::GuiCarSelectStartEvent();
                const s32 liCarSelectFlow = 1;
                std::memcpy(lEvent.maData, &liCarSelectFlow, sizeof(liCarSelectFlow));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED818..0x823ED834: zero input posts the one-byte ready-to-exit signal.
            case 74:
                if (*reinterpret_cast<const u8*>(lpAction) == 0)
                {
                    BrnGui::GuiCarSelectReadyToExitEvent lEvent = BrnGui::GuiCarSelectReadyToExitEvent();
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                break;

            // ARTIST 0x823ED870..0x823ED888: forward the action byte as GUI 84.
            case 78:
            {
                BrnGui::GuiCarSelectAbortEvent lEvent = BrnGui::GuiCarSelectAbortEvent();
                lEvent.maData[0] = *reinterpret_cast<const u8*>(lpAction);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA71C..0x823EA72C: GUI 85 is a one-byte tag.
            case 80:
                PushGuiTag(85, lpGuiInput);
                break;

            // ⭐⭐ [boost-wave2 2026-09-14] 287 -> GUI 380, the SHORTCUT latch. ARTIST
            // 0x823ED554..0x823ED56C, the console's `case 287`:
            //     lbz  r11, 0(r31)                 ; the action's one byte
            //     stb  r11, var_35D8(r1)
            //     bl   AddGuiEvent<BrnGui::GuiPlayerInShortcutEvent>   -> AddEvent(q, rec, 380, 1)
            // A raw one-byte payload, no GuiEvent header -- same shape as case 81 below.
            //
            // BOTH ENDS WERE ALREADY LIVE AND COULD NOT HEAR EACH OTHER, the same defect the
            // boost-ticker wave found on its seven arms:
            //   PRODUCER  BrnGameState::StreetManager::UpdateUpcomingStreets @0x82350A88 posts
            //             action 287 on each edge of the player's shortcut membership (AISection
            //             flag bit 0) -- reconstructed and running at
            //             BrnGameStateStreetManager_wB_10.cpp:206-222.
            //   CONSUMER  BrnGui::GuiCache::RecvEvent case 380 (BrnGuiCache.cpp:1637) latches
            //             mbInShortcut, and GuiModule forwards 380 in its whitelist
            //             (BrnGuiModule.cpp:1972).
            // This arm was the only missing link, so the GUI's "player is in a shortcut" state
            // was stuck false for the whole game.
            case 287:
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 380, 1);
                break;
            // [image-gallery response path] These records are produced by
            // GameStateImageManagerBase (289..293) and consumed by the image renderer /
            // ImageGalleryState. The output IDs and record layouts are the X360 translator's
            // typed AddGuiEvent instantiations; the source producer's raw stack records are
            // statically compared with the named action types in that producer TU.
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_RENDER_IMAGE:
            {
                const BrnGameState::GameStateModuleIO::ImageGalleryRenderImageAction* lpRenderImageAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryRenderImageAction*>(
                        lpAction);
                CGS_ASSERT(lpRenderImageAction != 0, "lpRenderImageAction");

                BrnGui::GuiEventNetworkPlayerImage lEvent;
                lEvent.mpTexture      = lpRenderImageAction->mpTexture;
                lEvent.miTextureIndex = lpRenderImageAction->miTextureIndex;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_IMAGE_INFO:
            {
                const BrnGameState::GameStateModuleIO::ImageGalleryImageInfoAction* lpImageInfoAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryImageInfoAction*>(
                        lpAction);
                CGS_ASSERT(lpImageInfoAction != 0, "lpImageInfoAction");

                BrnGui::GuiImageGalleryImageInfoEvent lEvent;
                lEvent.mCaptureDate = lpImageInfoAction->mCaptureDate;
                lEvent.mWorldRegion = lpImageInfoAction->mWorldRegion;
                lEvent.miNumCaptures = lpImageInfoAction->miNumCaptures;
                lEvent.miSlotIndex = lpImageInfoAction->miSlotIndex;
                lEvent.mPlayerName = lpImageInfoAction->mPlayerName;
                lEvent.mbLocked = lpImageInfoAction->mbLocked;
                lEvent.mbHasDeleted = lpImageInfoAction->mbHasDeleted;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_COUNT_ACTION:
            {
                const BrnGameState::GameStateModuleIO::ImageGalleryCountAction* lpCountAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryCountAction*>(
                        lpAction);
                BrnGui::GuiImageGalleryCollectedCountEvent lEvent;
                lEvent.meImageGalleryImageType = lpCountAction->meImageType;
                lEvent.miCollectedCount = lpCountAction->miUsedCount;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_DATA_ACTION:
            {
                const BrnGameState::GameStateModuleIO::ImageGalleryDataAction* lpDataAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageGalleryDataAction*>(
                        lpAction);
                BrnGui::GuiImageGalleryCollectedDataEvent lEvent;
                lEvent.meImageGalleryImageType = lpDataAction->meImageType;
                lEvent.mActiveBitArray = lpDataAction->mActiveBitArray;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_IMAGE_GALLERY_LOAD_COMPLETE:
            {
                // ARTIST reads no action byte here; the one-byte load marker only advances
                // ImageGalleryState's pending-load notification.
                CgsGui::GuiEvent<523> lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            // ARTIST 0x823EA760 / 0x823EBC50: colour and unlocked-livery replies.
            // These are raw GUI payloads, without a GuiEvent header.
            case 81:
                lpGuiInput->GetGuiEvents()->AddEvent(lpAction, 414, 8);
                break;
            // ARTIST 0x823EA730..0x823EA748: copy the time word as GUI 82 (four bytes).
            case 82:
            {
                BrnGui::GuiCarSelectOnlineTimeLeftEvent lEvent = BrnGui::GuiCarSelectOnlineTimeLeftEvent();
                std::memcpy(lEvent.maData, lpAction, sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA74C..0x823EA75C: GUI 83 is a one-byte tag.
            case 83:
                PushGuiTag(83, lpGuiInput);
                break;

            case 183:
            {
                const CgsContainers::Array<CgsID, 8>& lrCars =
                    *reinterpret_cast<const CgsContainers::Array<CgsID, 8>*>(lpAction);
                CgsContainers::Array<CgsID, 8> lReply;
                lReply.Clear();
                for (s32 liCar = 0; liCar < lrCars.GetLength(); ++liCar)
                    lReply.Append(lrCars.GetItem(liCar));
                lpGuiInput->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lReply), 413, sizeof(lReply));
                break;
            }

            // ARTIST 0x823EBCA4..0x823EBD40 rebuilds GUI 412 from the 128-car action:
            // append exactly the live IDs, copy both 128-bit sets, then copy the maximum.
            case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECTION_RESPONSE:
            {
                typedef BrnGameState::GameStateModuleIO::CarSelectionResponseAction CarSelectionAction;
                CGS_STATIC_ASSERT((sizeof(CarSelectionAction) == 1072), "X360 action 184 payload size");
                const CarSelectionAction& lrAction = *reinterpret_cast<const CarSelectionAction*>(lpAction);
                BrnGui::GuiCarSelectionEvent lEvent;
                lEvent.maCars.Clear();

                // ARTIST GetLength returns in r27 and compares with signed cmpwi against zero.
                const s32 liCarCount = static_cast<s32>(lrAction.maCars.GetLength());
                for (s32 liCar = 0; liCar < liCarCount; ++liCar)
                {
                    lEvent.maCars.Append(lrAction.maCars.GetItem(static_cast<u32>(liCar)));
                }

                lEvent.mHasBeenDrivenBitArray = lrAction.mHasBeenDrivenBitArray;
                lEvent.mWreckedArray = lrAction.mWreckedArray;
                lEvent.miMaxAvailableCars = lrAction.miMaxAvailableCars;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBD98/0x823EBDA4 posts the one-byte Blue Team Escaping tag (GUI 446).
            // The local byte is unwritten on X360; default-initialize the existing raw event type.
            case 162:
            {
                BrnGui::GuiBlueTeamIsEscapingEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBDAC/0x823EBDB8 posts the one-byte Blue Team Behind You tag (GUI 447).
            case 163:
            {
                BrnGui::GuiBlueTeamIsBehindYouEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST case 164 reads the typed LeaderReachedBoundaryAction record emitted by
            // HUDMessageLogic::GenerateOnlineLeaderMilestoneMessage: {metres f32 @+0,
            // active-car index @+4, miles bool @+8}. Preserve its filtered distance diagnostics
            // after the GUI event, including the console's metres-to-display-unit factors.
            case 164:
            {
                typedef BrnGameState::GameStateModuleIO::LeaderReachedBoundaryAction LeaderBoundaryAction;
                CGS_STATIC_ASSERT((sizeof(LeaderBoundaryAction) == 12), "X360 action 164 payload size");
                const LeaderBoundaryAction& lrAction = *reinterpret_cast<const LeaderBoundaryAction*>(lpAction);
                if (lrAction.mbIsBoundaryInMiles)
                {
                    BrnGui::GuiLeaderPassedMileBoundaryEvent lEvent = BrnGui::GuiLeaderPassedMileBoundaryEvent();
                    lEvent.meActiveRaceCarIndex = lrAction.meRaceCarIndex;
                    lEvent.mfDistanceToFinishInMetres = lrAction.mfDistanceInMetres;
                    PushGuiEvent(lEvent, lpGuiInput);
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint << "****** LEADER PASSED "
                                                  << (lrAction.mfDistanceInMetres * 0.0006213712f)
                                                  << "mi boundary! ********!\n";
                    }
                }
                else
                {
                    BrnGui::GuiLeaderPassedKMBoundaryEvent lEvent = BrnGui::GuiLeaderPassedKMBoundaryEvent();
                    lEvent.meActiveRaceCarIndex = lrAction.meRaceCarIndex;
                    lEvent.mfDistanceToFinishInMetres = lrAction.mfDistanceInMetres;
                    PushGuiEvent(lEvent, lpGuiInput);
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint << "****** LEADER PASSED "
                                                  << (lrAction.mfDistanceInMetres * 0.001f)
                                                  << "km boundary! ********!\n";
                    }
                }
                break;
            }

            // ARTIST 0x823EBE88..0x823EBEF4 posts the player-eliminated record/tag, then the
            // optional last-blue-team-member tag. Preserve its order and unwritten tag bytes.
            case BrnGameState::GameStateModuleIO::E_ACTION_PLAYER_ELIMINATED:
            {
                const BrnGameState::GameStateModuleIO::PlayerEliminatedAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::PlayerEliminatedAction*>(lpAction);
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "****** PLAYER ELIMINATED - TRANSLATING TO GUI EVENT! ********!\n";
                }
                if (lrAction.mbLocalPlayerEliminated)
                {
                    BrnGui::GuiLocalPlayerEliminatedEvent lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                else
                {
                    BrnGui::GuiPlayerEliminatedEvent lEvent = BrnGui::GuiPlayerEliminatedEvent();
                    std::memcpy(lEvent.maData, &lrAction.meActiveRaceCarIndex,
                                sizeof(lrAction.meActiveRaceCarIndex));
                    PushGuiEvent(lEvent, lpGuiInput);
                }
                if (lrAction.mbLastBlueTeamMember)
                {
                    BrnGui::GuiLastBlueTeamMemberEvent lEvent;
                    PushGuiEvent(lEvent, lpGuiInput);
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "****** LAST BLUE TEAM MEMBER - TRANSLATING TO GUI EVENT! ********!\n";
                    }
                }
                break;
            }

            // ARTIST 0x823EBEF8..0x823EBF2C preserves the original assert and forwards the
            // aggressor's active-car index as GUI 453 / four bytes.
            case BrnGameState::GameStateModuleIO::E_ACTION_TRAITOROUS_TAKEDOWN:
            {
                if (lpAction == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "lpTraitorousTDAction",
                        CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                        2219));
                    CgsDev::Assert::EndAssert();
                }
                const BrnGameState::GameStateModuleIO::TraitorousTakedownAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::TraitorousTakedownAction*>(lpAction);
                BrnGui::GuiTraitorousTakedownEvent lEvent = BrnGui::GuiTraitorousTakedownEvent();
                lEvent.meAggActiveRaceCarIndex = lrAction.meAggrActiveRaceCarIndex;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED62C..0x823ED670 posts GUI 541 only when action flag 0x04 is set.
            // It copies the 56-byte score block and maps flag 0x08 to the HUD-message byte.
            case BrnGameState::GameStateModuleIO::E_ACTION_BURNOUT_SKILLZ:
            {
                if (lpAction == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "lpBurnoutSkillzAction",
                        CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                        3503));
                    CgsDev::Assert::EndAssert();
                }
                typedef BrnGameState::GameStateModuleIO::BurnoutSkillzAction BurnoutSkillzAction;
                CGS_STATIC_ASSERT((sizeof(BurnoutSkillzAction) == 68 &&
                              offsetof(BurnoutSkillzAction, meActiveRaceCarIndex) == 0x40),
                              "X360 action 168 input layout");
                const BurnoutSkillzAction& lrAction = *reinterpret_cast<const BurnoutSkillzAction*>(lpAction);
                if ((lrAction.miActionFlags & BurnoutSkillzAction::KI_ACTION_TYPE_UPDATED_VALUE) !=
                    BurnoutSkillzAction::KI_ACTION_TYPE_UPDATED_VALUE)
                {
                    break;
                }

                BrnGui::GuiNewBurnoutSkillzEvent lEvent;
                lEvent.mNetworkPlayerID = lrAction.mNetworkPlayerID;
                lEvent.meActiveRaceCarIndex = lrAction.meActiveRaceCarIndex;
                lEvent.mSkillzData = lrAction.mBurnoutSkillzData;
                lEvent.mbUpdateHUDMessage =
                    (lrAction.miActionFlags & BurnoutSkillzAction::KI_ACTION_TYPE_SHOW_HUD_MESSAGE) != 0;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED678..0x823ED6F0 posts GUI 542 / 24 bytes. The action's producer
            // type is not present in current source; this view names only the fields and offsets
            // consumed by the original arm and leaves its input-domain owner unresolved.
            case 169:
            {
                if (lpAction == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "lpRoadRulesSkillzAction",
                        CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                        3526));
                    CgsDev::Assert::EndAssert();
                }
                struct NewBurnoutHudMessageActionView
                {
                    CgsID mRoadID;  // +0x00
                    u32 muActionValue;  // +0x08; ARTIST maps zero / one / other to message types 1/2/3
                    BrnGameState::BurnoutSkillzData::EBurnoutSkillType meSkill;  // +0x0C
                    EActiveRaceCarIndex meNewOwner;  // +0x10
                };
                CGS_STATIC_ASSERT((offsetof(NewBurnoutHudMessageActionView, muActionValue) == 0x08 &&
                              offsetof(NewBurnoutHudMessageActionView, meSkill) == 0x0C &&
                              offsetof(NewBurnoutHudMessageActionView, meNewOwner) == 0x10),
                              "ARTIST action 169 consumed input offsets");
                const NewBurnoutHudMessageActionView& lrAction =
                    *reinterpret_cast<const NewBurnoutHudMessageActionView*>(lpAction);
                BrnGui::GuiNewBurnoutHudMessageEvent lEvent = BrnGui::GuiNewBurnoutHudMessageEvent();
                lEvent.mRoadID = lrAction.mRoadID;
                if (lrAction.muActionValue == 0)
                    lEvent.meMessageType = BrnGui::GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YOUR;
                else if (lrAction.muActionValue == 1)
                    lEvent.meMessageType = BrnGui::GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_GOT;
                else
                    lEvent.meMessageType = BrnGui::GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_YOU_GOT;
                lEvent.meSkill = lrAction.meSkill;
                lEvent.meNewOwner = lrAction.meNewOwner;
                lEvent.mePreviousOwner = static_cast<EActiveRaceCarIndex>(-1);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED184..0x823ED194: payback lost -> one unwritten GUI 178 tag.
            case 211:
            {
                CgsGui::GuiEvent<178> lEvent;
                mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED198..0x823ED1A8: payback victim left -> one unwritten GUI 180 tag.
            case 212:
            {
                CgsGui::GuiEvent<180> lEvent;
                mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ECECC..0x823ED01C: Payback mugshot control. Response 0 is an
            // intentional no-post arm; later capture/show steps copy the road id, response,
            // image type, selected car index and local-aggressor byte into GUI 325.
            case BrnGameState::GameStateModuleIO::E_ACTION_PAYBACK_MUGSHOT:
            {
                const BrnGameState::GameStateModuleIO::PaybackMugshotAction* lpPaybackMugshotAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::PaybackMugshotAction*>(
                        lpAction);
                if (lpPaybackMugshotAction->meMugshotResponse ==
                    BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_PREPARE_CAPTURE)
                {
                    break;
                }

                const bool lbLocalAggressor =
                    lpPaybackMugshotAction->mbIsTakedownAggressorLocalPlayer;
                const s32 liAggressorCarIndex =
                    lbLocalAggressor ? lpPaybackMugshotAction->meTakedownVictimIndex
                                     : lpPaybackMugshotAction->meTakedownAggressorIndex;

                BrnGui::GuiMugshotControlEvent lEvent;
                lEvent.mRoadId = lpPaybackMugshotAction->mRoadRuleBeatenRoadID;
                lEvent.meResponse = lpPaybackMugshotAction->meMugshotResponse;
                lEvent.meImageType = lpPaybackMugshotAction->meImageType;
                lEvent.meOpponentARCI = static_cast<EActiveRaceCarIndex>(liAggressorCarIndex);
                lEvent.mbLocalAggressor = lbLocalAggressor;

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE GAME STATE TO X (1)\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "IMAGE TYPE : " << static_cast<s32>(lpPaybackMugshotAction->meImageType) << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << lbLocalAggressor << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "AGG ARCI   : " << liAggressorCarIndex << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "RESPONSE   : "
                        << static_cast<s32>(lpPaybackMugshotAction->meMugshotResponse) << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "ROAD?      : "
                        << static_cast<u64>(lpPaybackMugshotAction->mRoadRuleBeatenRoadID) << "\n";
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED020..0x823ED180: abort an in-flight capture. The console's r19
            // zero register writes the first qword; response 4 and the action fields follow.
            case BrnGameState::GameStateModuleIO::E_ACTION_ABORT_MUGSHOT_CAPTURE:
            {
                const BrnGameState::GameStateModuleIO::AbortMugshotCaptureAction* lpAbortCaptureAction =
                    reinterpret_cast<const
                        BrnGameState::GameStateModuleIO::AbortMugshotCaptureAction*>(lpAction);
                CGS_ASSERT(lpAbortCaptureAction != 0, "lpAbortCaptureAction");

                const bool lbLocalAggressor =
                    lpAbortCaptureAction->mbIsTakedownAggressorLocalPlayer;
                const s32 liAggressorCarIndex =
                    lbLocalAggressor ? lpAbortCaptureAction->meTakedownVictimIndex
                                     : lpAbortCaptureAction->meTakedownAggressorIndex;
                const s32 liResponse = static_cast<s32>(
                    BrnGameState::GameStateModuleIO::E_MUGSHOT_RESPONSE_STOP_MY_MUGSHOT);

                BrnGui::GuiMugshotControlEvent lEvent;
                lEvent.mRoadId = 0;
                lEvent.meResponse = static_cast<BrnGameState::GameStateModuleIO::EMugshotResponse>(liResponse);
                lEvent.meImageType = lpAbortCaptureAction->meImageType;
                lEvent.meOpponentARCI = static_cast<EActiveRaceCarIndex>(liAggressorCarIndex);
                lEvent.mbLocalAggressor = lbLocalAggressor;

                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "MUGSHOT EVENT : BRIDGE GAME STATE TO X (2)\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "IMAGE TYPE : " << static_cast<s32>(lpAbortCaptureAction->meImageType) << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "LOCAL AGG  : " << lbLocalAggressor << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint
                        << "AGG ARCI   : " << liAggressorCarIndex << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "RESPONSE   : " << liResponse << "\n";
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    *CgsDev::Log::gpDebugPrint << "ROAD?      : " << static_cast<u64>(0) << "\n";
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB598..0x823EB5AC forwards the activated dirty-trick type as GUI 182 / 4 bytes.
            case BrnGameState::GameStateModuleIO::E_ACTION_PAYBACK_ACTIVATED:
            {
                typedef BrnGameState::GameStateModuleIO::PaybackActivatedAction PaybackAction;
                CGS_STATIC_ASSERT((sizeof(PaybackAction) == 12), "X360 action 215 payload size");
                const PaybackAction& lrAction = *reinterpret_cast<const PaybackAction*>(lpAction);
                BrnGui::GuiPaybackReceivedEvent lEvent = BrnGui::GuiPaybackReceivedEvent();
                std::memcpy(lEvent.maData, &lrAction.mePaybackType, sizeof(lrAction.mePaybackType));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB5B4..0x823EB5C0: payback over -> one unwritten GUI 183 tag.
            case 216:
            {
                CgsGui::GuiEvent<183> lEvent;
                mGuiModule.AddGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED1AC..0x823ED1C4: online player added -> lobby-join chatter.
            case BrnGameState::GameStateModuleIO::E_ACTION_ONLINE_PLAYER_ADDED:
            {
                const BrnGameState::GameStateModuleIO::OnlinePlayerAddedAction& lrAction = *reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::OnlinePlayerAddedAction*>(lpAction);
                BrnGui::GuiEventNetworkPlayerJoinedLobby lEvent;
                lEvent.mNetworkPlayerID = lrAction.mAddedPlayerNetworkID;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EB7C8..0x823EB7D8: notify the crash HUD that skipping is available.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_PLAYER_CAN_SKIP_CRASH:
            {
                // The console does not write this one-byte payload; the receiver ignores it.
                BrnGui::GuiEventCanSkipCrash lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA87C..0x823EA890 and 0x823EA894..0x823EA8A8: pause-state edges.
            case 86:
            case 87:
            {
                BrnGui::GuiGamePausedEvent lEvent = BrnGui::GuiGamePausedEvent();
                lEvent.maData[4] = (liActionType == 86) ? 1u : 0u;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EA8AC..0x823EA8C0: literal true -> GUI show-HUD event 148.
            case 89:
            {
                BrnGui::GuiEventShowHideHud lEvent = BrnGui::GuiEventShowHideHud();
                lEvent.mbShow = true;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF54..0x823EBF70: construct the named offline-string overlay.
            case 90:
            {
                BrnGui::GuiOverlayRequest lRequest;
                lRequest.Construct("GMStrOffline");
                PushGuiEvent(lRequest, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF34..0x823EBF50: construct the invite-accept overlay.
            case 91:
            {
                BrnGui::GuiOverlayRequest lRequest;
                lRequest.Construct("GMInvAccept");
                PushGuiEvent(lRequest, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF74..0x823EBF8C, case 92 reads InviteOrJoinParams+0x98 and posts the
            // one-byte GUI 128 event.
            // Its real producer copies that same 0x9C-byte record from mInviteOrJoinParams.
            case 92:
            {
                const BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams* lpParams = reinterpret_cast<const
                    BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams*>(lpAction);
                BrnGui::GuiEventPrepareForInvite lEvent = BrnGui::GuiEventPrepareForInvite();
                std::memcpy(lEvent.maData, &lpParams->mbIsLocalUserChangeNeeded,
                            sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EBF90..0x823EBFA0: update-prepare-for-invite sends tag 129.
            case BrnGameState::GameStateModuleIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE:
                PushGuiTag(129, lpGuiInput);
                break;

            // ARTIST 0x823EBFA4..0x823EBFB4: perform-invite sends tag 131; action bytes are unread.
            case BrnGameState::GameStateModuleIO::E_ACTION_PERFORM_INVITE:
                PushGuiTag(131, lpGuiInput);
                break;

            // ARTIST 0x823EBFB8..0x823EBFF0: assert the record, then relay its success byte.
            case BrnGameState::GameStateModuleIO::E_ACTION_INVITE_COMPLETE:
            {
                CGS_ASSERT(lpAction != 0, "lpInviteCompleteAction");
                BrnGui::GuiEventInviteComplete lEvent = BrnGui::GuiEventInviteComplete();
                std::memcpy(lEvent.maData, lpAction, sizeof(lEvent.maData));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 45  E_ACTION_SET_UP_ALL_DRIVE_THRUS (1112 bytes) -------------------------
            // ⭐⭐ [minimap blips, issue #9] THE DRIVE-THRU ICON TABLE HOP. While the pending count
            // is below 48, one SatNavIconInfo per DriveThruInfo:
            //   * the GenericRegion::Type (info +0x10) becomes the icon-type byte (+0x28):
            //     0 JUNK_YARD -> 7 JUNKYARD, 1 GAS_STATION -> 10, 2 BODY_SHOP -> 9,
            //     3 PAINT_SHOP -> 11, 4 CAR_PARK -> 8, 16 TIRE_SHOP -> 12, anything else -> 4
            //     LANDMARK after a message-stream print of the type;
            //   * the position lane (+0x00) is {x, 0, z, 0} from the info's two floats;
            //   * the CgsID (+0x10) is copied whole; rotation (+0x18) and speed (+0x1C) are 0.0;
            //     the design-index (+0x22) and hidden-drive-thru (+0x23) bytes are 0.
            // Unlike the neighbouring arms this one posts NOTHING here -- see the tail.
            case BrnGameState::GameStateModuleIO::E_ACTION_SET_UP_ALL_DRIVE_THRUS:
            {
                typedef BrnGui::GuiEventUpdateSatNav::SatNavIconInfo SatNavIconInfo;
                typedef BrnGameState::GameStateModuleIO::SetUpAllDriveThrusAction SetUpAction;

                const SetUpAction* lpSetUp = reinterpret_cast<const SetUpAction*>(lpAction);
                const s32 liNumDriveThrus  = static_cast<s32>(lpSetUp->maDriveThrus.GetLength());

                for (s32 liDriveThru = 0; liDriveThru < liNumDriveThrus; ++liDriveThru)
                {
                    if (liPendingSatNavIcons >= BrnGui::GuiEventUpdateSatNav::KI_MAX_SAT_NAV_ICONS)
                    {
                        break;   // the pending record is full
                    }

                    const SetUpAction::DriveThruInfo& lrInfo =
                        lpSetUp->maDriveThrus.GetItem(static_cast<u32>(liDriveThru));
                    SatNavIconInfo& lrIcon = lPendingSatNavEvent.maIconInfo[liPendingSatNavIcons];

                    SatNavIconInfo::SatNavIconType leIconType;
                    switch (lrInfo.meType)
                    {
                    case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:   // 0
                        leIconType = SatNavIconInfo::E_SATNAVICON_JUNKYARD;      // 7
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION: // 1
                        leIconType = SatNavIconInfo::E_SATNAVICON_GAS_STATION;   // 10
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:   // 2
                        leIconType = SatNavIconInfo::E_SATNAVICON_BODYSHOP;      // 9
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:  // 3
                        leIconType = SatNavIconInfo::E_SATNAVICON_PAINT_SHOP;    // 11
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:    // 4
                        leIconType = SatNavIconInfo::E_SATNAVICON_CAR_PARK;      // 8
                        break;
                    case BrnTrigger::GenericRegion::E_TYPE_TIRE_SHOP:   // 16
                        leIconType = SatNavIconInfo::E_SATNAVICON_TIRE_SHOP;     // 12
                        break;
                    default:
                        // The console prints the offending type on the message stream first
                        // (gxMessageFilterFlags & 1; the prefix string is not recovered).
                        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[drivethru] BRIDGE: unexpected drive-thru region type "
                                << static_cast<s32>(lrInfo.meType) << "\n";
                        }
                        leIconType = SatNavIconInfo::E_SATNAVICON_LANDMARK;      // 4
                        break;
                    }

                    const Vector4 lv4Position(lrInfo.mfXCoord, 0.0f, lrInfo.mfZCoord, 0.0f);
                    lrIcon.SetPositionLane(lv4Position);
                    lrIcon.SetCgsId(lrInfo.mDriveThruId);
                    lrIcon.SetRotation(0.0f);
                    lrIcon.SetSpeedMph(0.0f);
                    lrIcon.SetDesignIndex(0);
                    lrIcon.SetHiddenDriveThru(false);
                    lrIcon.SetIconType(leIconType);

                    ++liPendingSatNavIcons;
                }
                break;
            }

            // ---- 58  E_ACTION_ON_STUNT_ELEMENT_COMPLETE (24 bytes) ------------------------
            // ⭐⭐ THE BILLBOARD / SMASH-GATE HUD POPUP. @0x823EB870..0x823EB96C.
            //   lwz r11, 0x14(r31)   -- meCurrentGameMode picks the presentation
            //   cmplwi r11, 0x11 ; bgt default            (UNSIGNED: E_MODE_NONE == -1 -> default)
            //   jump table jpt_823EB894: cases 0-8,10-14,16,17 -> the BOOST-BAR arm,
            //                            cases 9,15 (+ anything > 17) -> the plain arm
            //   both arms: { miCurrentCount@+0x00 <- action+0x0C,
            //                miTotalCount  @+0x04 <- action+0x10,
            //                meStuntType   @+0x08 <- MapStuntEnumsFromGameplayToGui(action+0x08) }
            //   both arms then post a 1-byte GuiAutosaveRequestEvent (356) whose byte is 0.
            // The two events are identically shaped but distinct ids with distinct consumers:
            // 217 dispatches to HudMessageAnalyzer::HandleStuntInfo ("Billboards Smashed 12/45"),
            // 218 goes to the HUD boost-bar component.
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE:
            {
                const BrnGameState::GameStateModuleIO::OnStuntElementCompleteAction* lpStunt =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OnStuntElementCompleteAction*>(lpAction);

                const BrnGui::StuntType leGuiStuntType =
                    MapStuntEnumsFromGameplayToGui(
                        static_cast<u32>(lpStunt->meStuntElementType));

                // The autosave request is built ONCE at the head of the case with a zero byte
                // and posted from both presentation arms, after the stunt-info event.
                BrnGui::GuiAutosaveRequestEvent lAutosaveRequest;
                lAutosaveRequest.maData[0] = 0;

                bool lbBoostBarPresentation;
                switch (lpStunt->meCurrentGameMode)
                {
                // jpt_823EB894 cases 0-8, 10-14, 16, 17.
                case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:
                case BrnGameState::GameStateModuleIO::E_MODE_FACE_OFF:
                case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:
                case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:
                case BrnGameState::GameStateModuleIO::E_MODE_PURSUIT:
                case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:
                case BrnGameState::GameStateModuleIO::E_MODE_ELIMINATOR:
                case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:
                case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:
                    lbBoostBarPresentation = true;
                    break;
                // jpt_823EB894 default: cases 9 (traffic attack) and 15 (free-burn lobby), plus
                // everything the `cmplwi 0x11 / bgt` pre-test rejected.
                default:
                    lbBoostBarPresentation = false;
                    break;
                }

                if (lbBoostBarPresentation)
                {
                    BrnGui::GuiEventBoostBarStuntInfo lEvent;           // id 218, 12 bytes
                    lEvent.miCurrentCount = lpStunt->miCurrentCount;
                    lEvent.miTotalCount   = lpStunt->miTotalCount;
                    lEvent.meStuntType    = leGuiStuntType;

                    PushGuiEvent(lEvent, lpGuiInput);
                    PushGuiEvent(lAutosaveRequest, lpGuiInput);
                }
                else
                {
                    BrnGui::GuiEventStuntInfo lEvent;                   // id 217, 12 bytes
                    lEvent.miCurrentCount = lpStunt->miCurrentCount;
                    lEvent.miTotalCount   = lpStunt->miTotalCount;
                    lEvent.meStuntType    = leGuiStuntType;

                    PushGuiEvent(lEvent, lpGuiInput);
                    PushGuiEvent(lAutosaveRequest, lpGuiInput);
                }


                break;
            }

            // =====================================================================================
            // ⭐⭐⭐ [boost-ticker wave 2026-09-14] THE SEVEN BOOST-TICKER ARMS -- X360
            // TranslateGameActionsToGuiEvents @0x823E9CE0 cases 108 and 171..176.
            //
            // These are the second missing link of the boost hint strip. The first
            // (world event -> game action) is the boost-ticker arms of
            // GameStateModule::ProcessGameEvents; the far end (GUI event -> the hint strip) has been
            // committed and routed for weeks. Every arm below is the console's own body,
            // which in six of the seven cases is literally "copy the words, post the event":
            //
            //   console arm                                    ->  GUI event (id, size)
            //   case 108 @0x823EB..  v329 = *action                383 GuiTrafficCheckEvent (4)
            //   case 171 @0x823ED..  v463[0]=*a; v463[1]=a[1]      384 GuiNearMissEvent     (8)
            //   case 172             v377 = *a                     385 GuiDriftingEvent     (4)
            //   case 173             v423 = *a                     386 GuiSpinningEvent     (4)
            //   case 174             assert(a); v451[0..1]=a[0..1] 387 GuiInAirEvent        (8)
            //   case 175             v409 = *a                     388 GuiOncomingEvent     (4)
            //   case 176             v381 = *a                     389 GuiTailgatingEvent   (4)
            //
            // ⚠️ NOTE THE WIDTH ASYMMETRY ON 176 AND IT IS THE CONSOLE'S: the TailgatingAction
            // is EIGHT bytes {mfDistance, meTailgatedCarIndex} but the console's arm copies
            // ONE word (`v381 = *v7`) into a FOUR-byte GuiTailgatingEvent -- the tailgated car
            // index never reaches the GUI. Do not "complete" it.
            // =====================================================================================

            // ---- 108  E_ACTION_ON_TRAFFIC_CHECKING_CHAIN (4 bytes) -> GUI 383 ---------------
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_TRAFFIC_CHECKING_CHAIN:
            {
                const BrnGameState::GameStateModuleIO::TrafficCheckingChainAction* lpChain =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::TrafficCheckingChainAction*>(lpAction);
                TrafficCheckEventWire383 lEvent;
                lEvent.miCount = lpChain->miChainSize;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 171  E_ACTION_NEAR_MISS (8 bytes) -> GUI 384 -------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_NEAR_MISS:
            {
                const BrnGameState::GameStateModuleIO::NearMissAction* lpNearMiss =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::NearMissAction*>(lpAction);
                NearMissEventWire384 lEvent;
                lEvent.miCount        = lpNearMiss->miCount;
                lEvent.meNearMissType = lpNearMiss->meNearMissType;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 172  E_ACTION_DRIFTING (4 bytes) -> GUI 385 --------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_DRIFTING:
            {
                const BrnGameState::GameStateModuleIO::DriftingAction* lpDrift =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::DriftingAction*>(lpAction);
                DriftingEventWire385 lEvent;
                lEvent.mfDistance = lpDrift->mfDistance;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 173  E_ACTION_SPINNING (4 bytes) -> GUI 386 --------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_SPINNING:
            {
                const BrnGameState::GameStateModuleIO::SpinningAction* lpSpin =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::SpinningAction*>(lpAction);
                SpinningEventWire386 lEvent;
                lEvent.mfSpinAngle = lpSpin->mfSpinAngle;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 174  E_ACTION_IN_AIR (8 bytes) -> GUI 387 ----------------------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_IN_AIR:
            {
                CGS_ASSERT(lpAction != 0, "lpInAirAction");   // GameBridgeGameStateToX.cpp:1983
                const BrnGameState::GameStateModuleIO::InAirAction* lpInAir =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::InAirAction*>(lpAction);
                InAirEventWire387 lEvent;
                lEvent.mfCumulativeAirTime  = lpInAir->mfCumulativeAirTime;
                lEvent.mfCurrentJumpAirTime = lpInAir->mfCurrentJumpAirTime;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 175  E_ACTION_ONCOMING (4 bytes) -> GUI 388 --------------------------------
            // THE "ONCOMING 800m" COUNTER: BoostMessageManager::UpdateOncoming composes
            // "<ONCOMING> GENERAL_SEPARATOR <distance>m" once the latched distance passes 50 m.
            case BrnGameState::GameStateModuleIO::E_ACTION_ONCOMING:
            {
                const BrnGameState::GameStateModuleIO::OncomingAction* lpOncoming =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OncomingAction*>(lpAction);
                OncomingEventWire388 lEvent;
                lEvent.mfDistance = lpOncoming->mfDistance;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 176  E_ACTION_TAILGATING (8 bytes) -> GUI 389 (4 bytes) --------------------
            case BrnGameState::GameStateModuleIO::E_ACTION_TAILGATING:
            {
                const BrnGameState::GameStateModuleIO::TailgatingAction* lpTailgating =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::TailgatingAction*>(lpAction);
                TailgatingEventWire389 lEvent;
                lEvent.mfDistance = lpTailgating->mfDistance;   // the index is NOT forwarded
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 178  AFTERTOUCH (12-byte action) -> GUI 403 (32-byte raw event) ----------------
            // ARTIST writes {sideways, forward, zero-qword, race-car index} at offsets 0/4/8/16;
            // the final 12 bytes of the 32-byte event remain stack residue.
            case BrnGameState::GameStateModuleIO::E_ACTION_AFTERTOUCH:
            {
                const BrnGameState::GameStateModuleIO::AftertouchAction* lpAftertouchAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::AftertouchAction*>(lpAction);
                BrnGui::GuiAftertouchEvent lEvent;
                lEvent.mfSidewaysAftertouch = lpAftertouchAction->mfSidewaysAftertouch;
                lEvent.mfForwardAftertouch = lpAftertouchAction->mfForwardAftertouch;
                lEvent.muZero = 0;
                lEvent.miRaceCarIndex = lpAftertouchAction->meRaceCarIndex;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 59  E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY (8 bytes) --------------
            // @0x823EB98C..0x823EB9B0: { meStuntElementType@+0x00 <- Map(action+0x00),
            //                            meCounty@+0x04 <- action+0x04 }. No autosave request.
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY:
            {
                const BrnGameState::GameStateModuleIO::OnStuntElementCompleteForCountyAction* lpAreaDone =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OnStuntElementCompleteForCountyAction*>(lpAction);

                BrnGui::GuiEventStuntAreaComplete lEvent;               // id 219, 8 bytes
                lEvent.meStuntElementType =
                    MapStuntEnumsFromGameplayToGui(static_cast<u32>(lpAreaDone->meStuntElementType));
                lEvent.meCounty           = lpAreaDone->meCounty;
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 60  E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE (4 bytes) ----------------
            // @0x823EB9B8..0x823EB9D4: { meStuntElementType@+0x00 <- Map(action+0x00) }.
            case BrnGameState::GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE:
            {
                const BrnGameState::GameStateModuleIO::OnStuntElementCompleteByTypeAction* lpAllDone =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::OnStuntElementCompleteByTypeAction*>(lpAction);

                BrnGui::GuiEventStuntAllComplete lEvent;                // id 220, 4 bytes
                lEvent.meStuntElementType =
                    MapStuntEnumsFromGameplayToGui(static_cast<u32>(lpAllDone->meStuntElementType));
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 55  E_ACTION_REQUEST_AUTOSAVE (1 byte) ----------------------------------
            // ⭐⭐ [profile-save 2026-08-27] THE PROFILE-AUTOSAVE REQUEST. X360 @0x823EB818:
            //     case 55:
            //       HIBYTE(v282) = *v7;                                  // the action's own byte
            //       AddGuiEvent<BrnGui::GuiAutosaveRequestEvent>(module+7252512, &v282, input);
            //       goto LABEL_512;
            // -- a straight one-byte relay of the action payload onto GUI id 356. This is the
            // console's ONE general-purpose "the profile just changed, save it" path: the
            // producers are CarSelectManager's exit state (already live in this build --
            // BrnCarSelectManager.cpp posts KI_ACTION_AUTOSAVE on car confirm),
            // DriveThruManager::ProcessDriveThru (x2), DriveThruManager::UnlockCarChallengeForCar
            // and StreetManager::ProcessNewRoadScore. The consumer is GuiModule::Update's case
            // 356, which raises the module's autosave-pending latch; its tail then runs the
            // 60-second throttle and calls ProfileManager::Autosave.
            // The banner above names this arm as the nearest sibling for whoever came next.
            case BrnGameState::GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE:
            {
                BrnGui::GuiAutosaveRequestEvent lEvent;
                lEvent.maData[0] = *reinterpret_cast<const u8*>(lpAction);   // the action's own byte
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 6  E_ACTION_SET_TAKEDOWN_CAMERA_STATE -> GUI 377 payloads 2 / 3 ---------
            // ⭐⭐ [takedown HUD wave 2026-09-13] THE MISSING HALF OF GUI EVENT 377. The world
            // bridge (BridgeWorldVehicleDataToGui) posts only the two CRASH payloads,
            // 0 START_CRASHED / 1 LEAVE_CRASHED, on the player-crashing edge. The two TAKEDOWN
            // payloads, 2 START_TAKEDOWN / 3 LEAVE_TAKEDOWN, come from HERE and nowhere else --
            // those are the image's only two AddGuiEvent<GuiPlayerCrashingStateChangeEvent>
            // sites. With this arm absent, payload 2 was never posted anywhere in the build, so
            // HudMessageAnalyzer::HandleCrashedEvent's START_TAKEDOWN case could not run: the
            // takedown line parked by HandleTakedown (mbTakedownMessagePending +
            // mPendingTakedownEvent) had no flush and the "TDGdShutD" shutdown line never fired.
            // It also left the analyzer's meCrashEntryState unable to reach 2, which is the test
            // HandleTakedown uses to decide "fire immediately" instead of "park".
            //
            // The console arm, transcribed: it reads ONE byte -- the action's mbActive at +0x04
            // -- and posts `2 + (mbActive == 0)`:
            //     lbz r11, 4(record) ; cntlzw ; extrwi 1,26 ; addi r11, r11, 2 ; stw
            // i.e. it ignores the victim index and the signature/revenge flags entirely; only
            // the director's own case-6 arm consumes those. The pairing is exact, because the
            // SAME action drives both: TakedownManager::StartTakedownCamera posts active = 1
            // (-> 2) and EndTakedownCamera / ClearAllTakedowns post active = 0 (-> 3), so the
            // bar's takedown enter/leave edges cannot drift from the camera's.
            case BrnGameState::GameStateModuleIO::E_ACTION_SET_TAKEDOWN_CAMERA_STATE:      // 6
            {
                const BrnGameState::GameStateModuleIO::SetTakedownCameraAction* lpTakedownCamera =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::SetTakedownCameraAction*>(lpAction);

                BrnGui::GuiPlayerCrashingStateChangeEvent lEvent;
                lEvent.meCurrentState =
                    lpTakedownCamera->mbActive
                        ? BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_TAKEDOWN
                        : BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_TAKEDOWN;
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 120 -> GUI 373, A FREE-BURN RIVAL WAS SHUT DOWN -------------------------------
            // [FX-FLOW 2026-09-24, crash-parity G13-X5 remainder] ARTIST 0x823ED88C..0x823ED8A0
            // (jpt_823EA1F0 entry 120):
            //     ld   r11, 0(r31)        ; ShutdownAction::mVictimCarID (record +0)
            //     std  r11, var_35D8      ; the GuiShutdownEvent's one member
            //     bl   AddGuiEvent<BrnGui::GuiShutdownEvent>   -> AddEvent(q, rec, 373, 8)
            // Producer: TakedownManager::ProcessTakedownEvent @0x823940AC..0x823940D0 (action 120,
            // size 24). Consumers, in the GUI's own order: GuiCache::RecEvent case 373 stores the id
            // (mShutdownCarID, 0x8250FFA8) -- forwarded by GuiModule::DispatchInboundGuiEvents ahead
            // of the flow routing -- then BrnGui::InGame's case 373 shuts the HUD down and sends
            // "TO_RVL_POST" into BrnGui::OfflineRivalShutdown (e3b62101), whose presentation reads
            // the id back through GetShutdownCarID(). The arm was held back until both of those
            // existed (run_rcem2_rival_shutdown.py guards the order).
            case BrnGameState::GameStateModuleIO::E_ACTION_SHUTDOWN:   // 120
            {
                const BrnGameState::GameStateModuleIO::ShutdownAction* lpShutdownAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ShutdownAction*>(lpAction);
                ShutdownEventWire373 lEvent;
                lEvent.mVictimCarID = lpShutdownAction->mVictimCarID;   // `ld 0(r31)` -> `std`
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 121 -> GUI 374, THE FREE-BURN RIVAL SHUTDOWN IS FINISHED --------------------
            // [crash-parity 2026-09-22] ARTIST 0x823ED8A8..0x823ED8B4 (jpt_823EA1F0 entry 121 at
            // 0x823EA3D8):
            //     mr r5, r20 ; addi r4, r1, 0xDD ; add r3, r29, r30
            //     bl AddGuiEvent<BrnGui::GuiShutdownFinishedEvent>     -> AddEvent(q, rec, 374, 1)
            // The record is one stack byte the arm never writes, and it does not read the
            // action. The byte's only reader-side consumer, GuiCache::RecEvent case 374
            // (0x8250FFC4..0x8250FFC8), stores mbCarUnlockPending = 1 without reading it, so a
            // zero byte stands in for the residue. Producer: TakedownManager::EndTakedownCamera's
            // free-burn arm (action 121, after the car is handed back to the player).
            // [FX-FLOW 2026-09-24] The consumer half is on PC now: GuiCache::RecEvent case 374 and
            // its GuiModule::DispatchInboundGuiEvents forward.
            case BrnGameState::GameStateModuleIO::E_ACTION_SHUTDOWN_FINISHED:   // 121
            {
                ShutdownFinishedEventWire374 lEvent;
                lEvent.mu8Unwritten = 0;
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 222..224  original one-byte leftovers and the joined-car wire ------------------
            // ARTIST 0x823EC57C..0x823EC5C0. 222 and 224 pass unwritten stack bytes to one-byte
            // GUI 151 / 152 AddEvent helpers. Preserve that residue. 223 copies only action+0
            // (qword) and action+0x18 (one byte) into GUI 150; output bytes +9..+15 stay residue.
            // The upstream action producer and its declared payload size remain unlocated.
            case 222:
            {
                CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventCarEliminatedFromEvent) == 1),
                              "ARTIST GUI 151 tag is one byte");
                BrnGui::GuiEventCarEliminatedFromEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case 223:
            {
                CGS_STATIC_ASSERT((sizeof(bool) == 1),
                              "ARTIST GUI 150 camera flag is one byte");
                const u8* lpActionBytes = reinterpret_cast<const u8*>(lpAction);
                BrnGui::GuiEventCarJoinedEvent lEvent;
                std::memcpy(&lEvent.mJoiningRivalID, lpActionBytes, sizeof(lEvent.mJoiningRivalID));
                // Preserve the raw byte as the lbz/stb pair does; do not normalize it through bool.
                std::memcpy(&lEvent.mbDoCamera, lpActionBytes + 0x18, sizeof(lEvent.mbDoCamera));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case 224:
            {
                GuiEvent152UnwrittenByte lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 242 / 245 / 246 / 247 / 248 / 250  THE RACE-MODE HUD MESSAGES -----------------
            // [FX-FLOW 2026-09-24, G11-D1 remainder] HUDMessageLogic's race arm posts these into the
            // game-action queue (242 GenerateFirstOrLastMessage, 245 GenerateLeaderMessages, 247
            // GenerateFinisherMessage, 248 GenerateRivalCheckpointMessage, 250 DetectCrashes /
            // DetectOnlineCrashes); 246 has no producer in the image. The console arms, jpt_823EA1F0:
            //   242 @0x823ED9F4  assert "lpTookLeadAction" (GameBridgeGameStateToX.cpp:0xEAF) ;
            //                    `lwz 8` -> +8, `ld 0` -> +0   AddGuiEvent<GuiTookLeadEvent>       484/16
            //   245 @0x823EC034  `ld 0` / `lfs 8` / `lbz 0x10` / `lwz 0xC` to the same offsets
            //                                                  AddGuiEvent<GuiInEventLeaderSplit>  420/24
            //   246 @0x823EC068  an unwritten stack byte       AddGuiEvent<GuiInEventNeckAndNeck>  421/1
            //   247 @0x823EC07C  `lwz 0` / `lwz 4`             AddGuiEvent<GuiInEventFinisher>     423/8
            //   248 @0x823EC0A0  `ld 8` / `ld 0` / `lwz 0x10`  AddGuiEvent<GuiInEventRivalProgress> 422/24
            //   250 @0x823EC0CC  `ld 0` / `lwz 8`              AddGuiEvent<GuiNetworkPlayerCrashingEvent> 482/16
            // 243 (took last), 244 (distance to finish) and 249 (player checkpoint) are in the
            // table's DEFAULT list: the console drops them here, so they post nothing on PC either.
            // GUI consumers: HudMessageAnalyzer::Update -- 484 HandleTookLead ("AggDrTkLead"), 420
            // HandleEventLeaderSplitTime ("EventCheckPt" offline), 421 "EventNeck", 423
            // HandleEventFinisher (online only), 482 HandleNetworkPlayerCrashedEvent (online only);
            // nothing reads 422.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_TOOK_LEAD:   // 242
            {
                CGS_ASSERT(lpAction != 0, "lpTookLeadAction");   // GameBridgeGameStateToX.cpp:3759
                const BrnGameState::GameStateModuleIO::HUDMessageTookLeadAction* lpTookLeadAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageTookLeadAction*>(lpAction);
                BrnGui::GuiTookLeadEvent lEvent;
                lEvent.meLeadActiveRaceCarIndex = lpTookLeadAction->meActiveRaceCarIndex;   // `lwz 8`
                lEvent.mOfflineRivalCarID       = lpTookLeadAction->mCarId;                 // `ld 0`
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_LEADING:   // 245
            {
                const BrnGameState::GameStateModuleIO::HUDMessageLeadingAction* lpLeadingAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageLeadingAction*>(lpAction);
                BrnGui::GuiInEventLeaderSplit lEvent;
                lEvent.mfLeadTime                 = lpLeadingAction->mfLeadTime;               // `lfs 8`
                lEvent.mLeadersCarID              = lpLeadingAction->mLeadingCarID;            // `ld 0`
                lEvent.mbLocalPlayerIsLeading     = lpLeadingAction->mbLocalPlayerIsLeading;   // `lbz 0x10`
                lEvent.meLeaderActiveRaceCarIndex = lpLeadingAction->meLeadingCarIndex;        // `lwz 0xC`
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_NECK_AND_NECK:   // 246
            {
                NeckAndNeckEventWire421 lEvent;
                lEvent.mu8Unwritten = 0;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_FINISHES:   // 247
            {
                const BrnGameState::GameStateModuleIO::HUDMessageXFinishesAction* lpFinishesAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageXFinishesAction*>(lpAction);
                BrnGui::GuiInEventFinisher lEvent;
                lEvent.meActiveRaceCarIndex = lpFinishesAction->meRivalRaceCarIndex;   // `lwz 0`
                lEvent.miFinishPosition     = lpFinishesAction->miFinishPosition;      // `lwz 4`
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_REACHES_CHECKPOINT:   // 248
            {
                const BrnGameState::GameStateModuleIO::HUDMessageXReachesCheckpointAction* lpCheckpointAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageXReachesCheckpointAction*>(lpAction);
                BrnGui::GuiInEventRivalProgress lEvent;
                lEvent.mLandmarkID                = lpCheckpointAction->mLandmarkID;            // `ld 8`
                lEvent.mRivalID                   = lpCheckpointAction->mRivalID;               // `ld 0`
                lEvent.meRivalActiveRaceCarIndex  = lpCheckpointAction->meRivalRaceCarIndex;    // `lwz 0x10`
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_CRASHES:   // 250
            {
                const BrnGameState::GameStateModuleIO::HUDMessageXCrashesAction* lpCrashesAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageXCrashesAction*>(lpAction);
                BrnGui::GuiNetworkPlayerCrashingEvent lEvent;
                lEvent.mRivalCarID                        = lpCrashesAction->mRivalID;               // `ld 0`
                lEvent.meNetworkPlayerActiveRaceCarIndex  = lpCrashesAction->meRivalRaceCarIndex;    // `lwz 8`
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 252..254  Burning Home Run and team-change HUD records -------------------------
            // ARTIST 0x823EC0F0..0x823EC230: preserve the three source assertions and raw payloads.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_BHR_CHECKPOINT:
            {
                CGS_ASSERT(lpAction != 0, "lpHUDMessageBHRCheckpoint");
                const BrnGameState::GameStateModuleIO::HUDMessageBHRCheckpointAction* lpBHRAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageBHRCheckpointAction*>(lpAction);
                BrnGui::GuiBHRCheckpointReachedEvent lEvent;
                std::memcpy(&lEvent, lpBHRAction, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_BHR_RUNNER_CRASHED:
            {
                CGS_ASSERT(lpAction != 0, "lpHUDMessageBHRRunnerCrashesEvent");
                const BrnGameState::GameStateModuleIO::HUDMessageBHRRunnerCrashedAction* lpBHRAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageBHRRunnerCrashedAction*>(lpAction);
                BrnGui::GuiHUDMessageBHRRunnerCrashed lEvent;
                std::memcpy(&lEvent, lpBHRAction, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_ONLINE_TEAM_CHANGE:
            {
                CGS_ASSERT(lpAction != 0, "lpHUDMessageOnlineTeamChangeAction");
                const BrnGameState::GameStateModuleIO::HUDMessageOnlineTeamChangeAction* lpTeamChangeAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageOnlineTeamChangeAction*>(lpAction);
                BrnGui::GuiOnlineTeamChangeEvent lEvent;
                std::memcpy(&lEvent, lpTeamChangeAction, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 256..260  rivalry status records -----------------------------------------------
            // ARTIST loads two CgsIDs at +0/+8, then appends the status values 0..3 for GUI 439;
            // the fleeing case forwards the same pair to GUI 440 without an added status word.
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_FIRST_HIT:
            {
                const BrnGameState::GameStateModuleIO::HUDMessageFirstHitOnRivalAction* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageFirstHitOnRivalAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_NEW;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_A_RIVAL:
            {
                const BrnGameState::GameStateModuleIO::HUDMessageRivalIsARivalAction* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsARivalAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_RIVAL;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_A_TARGET:
            {
                const BrnGameState::GameStateModuleIO::HUDMessageRivalIsATargetAction* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsATargetAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_TARGET;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_FLEEING:
            {
                const BrnGameState::GameStateModuleIO::HUDMessageRivalIsFleeingAction* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsFleeingAction*>(lpAction);
                BrnGui::GuiRivalIsFleeing lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::GameStateModuleIO::E_ACTION_HUD_MESSAGE_RIVAL_IS_WRECKED:
            {
                const BrnGameState::GameStateModuleIO::HUDMessageRivalIsWreckedAction* lpRivalAction = reinterpret_cast<const BrnGameState::GameStateModuleIO::HUDMessageRivalIsWreckedAction*>(lpAction);
                BrnGui::GuiRivalryStatusChange lEvent;
                lEvent.mRivalID = lpRivalAction->mRivalID;
                lEvent.mCarID = lpRivalAction->mCarID;
                lEvent.meNewStatus = BrnGui::GuiRivalryStatusChange::E_RIVAL_LEVEL_WRECKED;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 266..271  Online Stunt Run HUD records ------------------------------------------
            // The producer's 12-byte StuntRunTeamMessage maps by offset to the three typed GUI
            // receiver records; only bytes 0..9 are written by ARTIST and the two tail bytes remain
            // stack residue. Cases 270/271 build the receiver's score/time triple explicitly.
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_ELIMINATION:
            {
                CGS_ASSERT(lpAction != 0, "lpEliminationAction");
                const BrnGameState::HUDMessageLogic::StuntRunTeamMessage* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTeamMessage*>(lpAction);
                BrnGui::GuiNetworkStuntRunEliminationEvent lEvent;
                lEvent.mbIsLocalPlayer = lpMessage->miTeam;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                std::memcpy(&lEvent.mbIsTeammate, &lpMessage->mbLocalPlayerOnTeam, sizeof(bool));
                std::memcpy(&lEvent.mbIsLastPlayer, &lpMessage->mbLocalPlayerIsSubject, sizeof(bool));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_LEADING:
            {
                CGS_ASSERT(lpAction != 0, "lpLeadingAction");
                const BrnGameState::HUDMessageLogic::StuntRunTeamMessage* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTeamMessage*>(lpAction);
                BrnGui::GuiNetworkStuntRunLeadingEvent lEvent;
                lEvent.mbIsLocalPlayer = lpMessage->miTeam;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                std::memcpy(&lEvent.mbIsTeammate, &lpMessage->mbLocalPlayerOnTeam, sizeof(bool));
                std::memcpy(&lEvent.mbIsLastPlayer, &lpMessage->mbLocalPlayerIsSubject, sizeof(bool));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_VICTORY:
            {
                CGS_ASSERT(lpAction != 0, "lpVictoryAction");
                const BrnGameState::HUDMessageLogic::StuntRunTeamMessage* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTeamMessage*>(lpAction);
                BrnGui::GuiNetworkStuntRunVictoryEvent lEvent;
                lEvent.mbIsLocalPlayer = lpMessage->miTeam;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                std::memcpy(&lEvent.mbIsTeammate, &lpMessage->mbLocalPlayerOnTeam, sizeof(bool));
                std::memcpy(&lEvent.mbIsLastPlayer, &lpMessage->mbLocalPlayerIsSubject, sizeof(bool));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_COMBO_END:
            {
                // ARTIST posts one unwritten byte as GUI 490; the GUI receiver is pointer-only.
                BrnGui::GuiNetworkLastStunRunEvent lEvent;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_SCORE:
            {
                CGS_ASSERT(lpAction != 0, "lpMessageAction");
                const BrnGameState::HUDMessageLogic::StuntRunScoreMessage* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunScoreMessage*>(lpAction);
                BrnGui::GuiNetworkSuntRunInfoMessageEvent lEvent;
                lEvent.mPlayerId = lpMessage->mNetworkPlayerID;
                lEvent.meMessageType = BrnGui::GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_SCORE;
                lEvent.miValue = lpMessage->miScore;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            case BrnGameState::HUDMessageLogic::E_HUD_MESSAGE_STUNT_RUN_TIME:
            {
                CGS_ASSERT(lpAction != 0, "lpMessageAction");
                const BrnGameState::HUDMessageLogic::StuntRunTimeMessage* lpMessage = reinterpret_cast<const BrnGameState::HUDMessageLogic::StuntRunTimeMessage*>(lpAction);
                BrnGui::GuiNetworkSuntRunInfoMessageEvent lEvent;
                lEvent.mPlayerId = -1;
                lEvent.meMessageType = BrnGui::GuiOnlineStuntRunMessageEvent::E_ONLINE_STUNT_RUN_MESSAGE_TIME;
                lEvent.miValue = static_cast<s32>(lpMessage->mfWarningTimeSeconds); // ARTIST fctiwz (30.0 -> 30)
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }
            // ---- 112  the DISTRICT CHANGE (8 bytes: {county, district}) -------------------
            // ⭐ [H1 district wave 2026-08-25] X360 case 112 @0x823EA-range (h1_dump2.txt),
            // verbatim: copy the action's 8-byte {county, district} pair, zero the third
            // word (the consumed flag), AddGuiEvent<GuiEventChangeDistrict> (id 169, 12B).
            // Producer: GameStateModule's case-115 arm; consumer: GuiCache::RecEvent case
            // 169 -> FBurnMainHudState's marker refresh (the HUD "you have entered
            // <district>" panel).
            case 112:
            {
                const s32* lpiRegion = reinterpret_cast<const s32*>(lpAction);

                BrnGui::GuiEventChangeDistrict lEvent;
                lEvent.meCounty    = lpiRegion[0];
                lEvent.meDistrict  = lpiRegion[1];
                lEvent.mu8Consumed = 0;
                lEvent.maPad[0] = lEvent.maPad[1] = lEvent.maPad[2] = 0;
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 148  the TRAINING TICKER (4 bytes: the BrnProgression::ETrainingType) ----
            // ⭐⭐ [tut-ticker] @0x823EA8C4..0x823EA930, instruction for instruction:
            //   lwz r3, 0(r31); cmpwi cr6, r3, 0x4D; bge default    -- type >= 77 -> drop
            //   bl ConvertTrainingTypeToStringId (r3 = the type -- Hex-Rays DROPPED this arg)
            //   beq default on NULL                                 -- no ticker string -> drop
            //   build the 2072-byte record: memset(strings, 0, 0x800); std 0 -> types[0..3];
            //     count(+0x810) = 0; flags(+0x811..814) = {0, 1, 1, 0}   (r19 == 0, r14 == 1)
            //   AddString(record, id, 2); AddGuiEvent<GuiEventTickerCustomMessage> -> id 537
            // Producer: TrainingManager::SendTrainingTickerMessage (GameAction 148); consumer:
            // CustomRendererManager::RecvEvent case 537 -> BlackBar + InGameMessage renderers.
            case 148:
            {
                const s32 liTrainingType = *reinterpret_cast<const s32*>(lpAction);
                if (liTrainingType < 77)
                {
                    const char* lpcStringId = ConvertTrainingTypeToStringId(
                        static_cast<BrnProgression::ETrainingType>(liTrainingType));
                    if (lpcStringId != 0)
                    {
                        TickerCustomMessageWire537 lEvent;
                        std::memset(&lEvent, 0, sizeof(lEvent));
                        lEvent.maFlags[1] = 1;   // +0x812 <- r14
                        lEvent.maFlags[2] = 1;   // +0x813 <- r14
                        lEvent.AddString(lpcStringId, 2);
                        PushGuiEvent(lEvent, lpGuiInput);

                    }
                }
                break;
            }

            // ---- 97 / 98 / 100 / 101  THE DRIVE-THRU ON-SCREEN RESPONSE ------------------
            // ⭐⭐⭐ [drive-thru wave 2026-08-29] THIS IS WHAT A SUCCESSFUL DRIVE-THRU SAYS.
            // The mechanic has worked for a day (a gas station refills boost 35 -> 70 in one
            // session) and the game said nothing, because these four arms did not exist -- the
            // CONSUMER side has been complete and mounted the whole time:
            //   GUI 366 -> HudMessageAnalyzer::Update case 366 (BrnGuiHudMessageAnalyzer_wB_12
            //   .cpp) -> HandleDriveThrough @0x8251D570 (..._gUI_03.cpp) -> the three message
            //   tables in ..._wB_res.cpp: KAPC_DRIVE_THROUGH_MESSAGES ("DriThrGasStn",
            //   "DriThrBdyShp", ...), KAPC_DRIVE_THROUGH_MAGIC_MESSAGES (the rare flavour line,
            //   car wash + paint shop only) and KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES
            //   ("DriThrBdyShX") when mbEffective is false.
            // So the whole missing link was four `case` arms in an already-mounted, already-
            // running drain loop.
            //
            // X360 @0x823EB5C8..0x823EB644, instruction for instruction. All four build the
            // same 8-byte GuiDriveThroughEvent {meDriveThroughType@+0, mbEffective@+4} and post
            // it through AddGuiEvent<GuiDriveThroughEvent> (id 366):
            //   case 97  BODY_SHOP    lbz r11, 0x85(r31) ; stw r14(=1) ; stb r11
            //                         -> type 1, EFFECTIVE FROM THE PAYLOAD BYTE AT +0x85
            //   case 98  PAINT_SHOP   li r11,2 ; stb r14(=1) ; stw r11   -> type 2, effective 1
            //   case 100 GAS_STATION  li r11,3 ; stb r14(=1) ; stw r11   -> type 3, effective 1
            //   case 101 (stop pres)  li r11,5 ; stb r19(=0) ; stw r11   -> type 5, effective 0
            // ⚠️ ONLY THE BODY SHOP READS THE PAYLOAD. The other three carry compile-time
            // constants; do not "tidy" them into one shared arm that reads +0x85, because the
            // gas-station payload's +0x85 is zero and the message would become the FAILED one.
            //
            // ⓘ The body shop's +0x85 byte is a HARDCODED 1 at the producer
            // (DriveThruManager::ProcessDriveThru @0x8239BAA8 `li r26,1`, reproduced in
            // BrnDriveThruManager.cpp's PostShopAction as `lacPayload[133] = 1`). So on this
            // build the ineffective "DriThrBdyShX" line is unreachable THROUGH THIS PATH -- and
            // that is the console's own shape, not a gap here. Noted because a reader chasing
            // the ineffective message will otherwise suspect this arm.
            //
            // ⛔ Action 101 is E_ACTION_STOP_DRIVE_THRU_PRES -- posted by ProcessDriveThru's
            // `mbIsClosed` early-out, i.e. "this drive-thru is closed, cancel the presentation".
            // It maps to E_DRIVE_THROUGH_TYPE_FAILED, which is what makes the FAILED message a
            // reachable arm of HandleDriveThrough rather than dead table rows.
            case BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU:     // 97
            case BrnGameState::GameStateModuleIO::E_ACTION_PAINT_SHOP_DRIVE_THRU:    // 98
            case BrnGameState::GameStateModuleIO::E_ACTION_GAS_STATION_DRIVE_THRU:   // 100
            case BrnGameState::GameStateModuleIO::E_ACTION_STOP_DRIVE_THRU_PRESENTATION: // 101
            {
                BrnGui::GuiDriveThroughEvent lEvent;   // id 366, 8 bytes

                if (liActionType ==
                    BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU)
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_BODY_SHOP;
                    // The 144-byte shop payload's per-action scalars start at +128; +0x85 (133)
                    // is the "the repair did something" byte the console reads back here.
                    lEvent.mbEffective =
                        (reinterpret_cast<const u8*>(lpAction)[KI_SHOP_ACTION_EFFECTIVE_BYTE] != 0);
                }
                else if (liActionType ==
                         BrnGameState::GameStateModuleIO::E_ACTION_PAINT_SHOP_DRIVE_THRU)
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_PAINT_SHOP;
                    lEvent.mbEffective        = true;
                }
                else if (liActionType ==
                         BrnGameState::GameStateModuleIO::E_ACTION_GAS_STATION_DRIVE_THRU)
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_GAS_STATION;
                    lEvent.mbEffective        = true;
                }
                else
                {
                    lEvent.meDriveThroughType = BrnGui::GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_FAILED;
                    lEvent.mbEffective        = false;
                }

                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 180  the GAME-STATS RESPONSE (432 bytes) ---------------------------------
            // ⭐⭐⭐ [pause-stats wave 2026-08-29] X360 case 180 @0x823EC8A0, the arm the Driver
            // Details pause panel's NUMBERS come out of. Unlike its 181 neighbour there is no
            // Construct call: the console builds the whole 432-byte record inline (~150
            // instructions of `lwz`/`stw` off the action, five `lwzx` off the module, four
            // Set::GetLength calls, two Profile::GetDriveThrusFound calls, five literals, then a
            // five-iteration loop that fills the six district columns) and hands it straight to
            // AddGuiEvent<GuiEventStatsResponse> (id 436, 432 bytes) @0x823ECC88.
            //
            // THE LAST HOP of the START-button pause screen's stat panel:
            //   GUI 435 -> game event 79 -> game action 180 -> GUI 436 (here). Without it
            // CrashNavDriverDetails::HandleStatData never runs and every stat field stays blank.
            //
            // ⚠️ THE ACTION RECORD *IS* A GameStats, not a wrapper -- see E_ACTION_GAME_STATS_
            // RESPONSE's banner in BrnGameActions.h. Named GameStats accessors replace the
            // console's raw `lwz <off>(r31)` throughout; every mapping below is the asm's.
            //
            // ⚠️ FOUR FIELDS ARE HARD-CODED CONSTANTS IN THE CONSOLE, AND THEY CROSS-CHECK:
            // `li 0xB / 0xE / 5 / 5` for the body-shop / gas-station / paint-shop / junkyard
            // totals, then `li 0x23` (35) for the drive-thru grand total -- and 11+14+5+5 == 35
            // exactly. They also equal the CAPACITIES of the four Profile drive-thru Sets whose
            // GetLength supplies the matching "found" counts (Set<CgsID,11>, <14>, <5>, <5>),
            // which is what pins each count to its category.
            //
            // ⚠️ SEVEN DESTINATION FIELDS ARE FED FROM THE MODULE, NOT THE ACTION (the console's
            // `lwzx r11, r29, <const>` reads, r29 == this): miCarsTotal and miDriversTot from
            // ProgressionManager::miMaxCarCount (+133468, the second halved with
            // `srawi 1 / addze`), the five "won" counters from the profile's
            // maGameModeTypeAmountCompletedSinceTheStart at modes 0/3/5/7/8, and
            // miBestRoadRageTakedownCount from Profile+118020.
            case BrnGameState::GameStateModuleIO::E_ACTION_GAME_STATS_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::GameStats* lpStats =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::GameStats*>(lpAction);

                typedef BrnGameState::GameStateModuleIO::GameStats GS;
                namespace GsmIO = BrnGameState::GameStateModuleIO;

                BrnProgression::ProgressionManager* lpProgressionManager =
                    GetGameStateModule().GetProgressionManager();
                const BrnProgression::Profile* lpProfile = lpProgressionManager->GetProfile();

                BrnGui::GuiEventStatsResponse lEvent;    // id 436, 432 bytes
                std::memset(&lEvent, 0, sizeof(lEvent));

                // the three ids (`ld`/`std` at +0x00/+0x08/+0x10)
                lEvent.mFaveCarId       = lpStats->GetValue(GS::E_ID_VALUE_TYPE_FAVOURITE_CAR);
                lEvent.mForgottenCarId  = lpStats->GetValue(GS::E_ID_VALUE_TYPE_FORGOTTEN_CAR);
                lEvent.mGreatestRivalId = lpStats->GetValue(GS::E_ID_VALUE_TYPE_NEMESIS);

                lEvent.miDistanceOnline  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_ONLINE);
                lEvent.miDistanceOffline = lpStats->GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE);
                lEvent.miTimePlayed      = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TIME_PLAYED);
                lEvent.miCarsCollected   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_CARS_COLLECTED);
                // `lwzx r7, r29, 0x69598C` -- the module's own car total, and `srawi r7,r7,1 /
                // addze r8, r7` (a signed halving that rounds toward zero) for the drivers total.
                lEvent.miCarsTotal  = lpProgressionManager->GetMaxCarCount();
                lEvent.miDriversTot = lEvent.miCarsTotal / 2;
                lEvent.miDrivers    = 0;                                   // `stw r19` (r19 == 0)

                lEvent.miPowerParkingBest = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_POWER_PARKING);
                lEvent.miPowerParkingBest_BetweenOtherPlayers =
                    lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_POWER_PARKING_BETWEEN_OTHER_PLAYERS);

                lEvent.miGolds   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_MEDALS_GOLD);
                lEvent.miSilvers = lpStats->GetValue(GS::E_INT_VALUE_TYPE_MEDALS_SILVER);
                lEvent.miBronzes = lpStats->GetValue(GS::E_INT_VALUE_TYPE_MEDALS_BRONZE);
                // `add r6, r7, r8` then `add r8, r6, r8` -- silver + bronze + gold.
                lEvent.miAllMedalsEarned = lEvent.miGolds + lEvent.miSilvers + lEvent.miBronzes;

                // ⚠️ NOT A TYPO: the console writes the EVENT-medal total into BOTH the
                // all-medals total and the event-medals total (one `lwz r11, 0x40(r31)` feeding
                // `stw r11` at +0x38 AND at +0x40). Same register, two destinations.
                lEvent.miAllMedalsTotal      = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_EVENT_MEDALS);
                lEvent.miEventMedalsEarned   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_NUM_EVENT_MEDALS);
                lEvent.miEventMedalsTotal    = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_EVENT_MEDALS);
                // and likewise the two road-rule pairs are each written twice (+0x44/+0x4C and
                // +0x48/+0x50 from the same two registers).
                lEvent.miRoadRuleMedalsEarned = lpStats->GetValue(GS::E_INT_VALUE_TYPE_NUM_ROAD_RULE_MEDALS);
                lEvent.miRoadRuleMedalsTotal  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_ROAD_RULE_MEDALS);
                lEvent.miRoadRules            = lEvent.miRoadRuleMedalsEarned;
                lEvent.mRoadsRuledTotal       = lEvent.miRoadRuleMedalsTotal;

                lEvent.miJumps    = lpStats->GetValue(GS::E_INT_VALUE_TYPE_JUMPS);
                lEvent.miJumpTot  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_JUMPS_MAX);
                lEvent.miSmashes  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_SMASHES);
                lEvent.miSmashTot = lpStats->GetValue(GS::E_INT_VALUE_TYPE_SMASHES_MAX);
                lEvent.miStunts   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_STUNTS);
                lEvent.miStuntTot = lpStats->GetValue(GS::E_INT_VALUE_TYPE_STUNTS_MAX);

                lEvent.miSignatureTDs    = 0;   // `stw r19`
                lEvent.miSignatureTDsTot = 0;   // `stw r19`

                lEvent.miTotalTakedowns    = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TAKEDOWNS);
                lEvent.miStandardTakedowns = lpStats->GetTakedownTypeCount(0);
                lEvent.miVerticalTakedowns = lpStats->GetTakedownTypeCount(3);
                lEvent.miTBoneTakedowns    = lpStats->GetTakedownTypeCount(2);
                lEvent.miAftertouchTakedowns = 0;                       // `stw r19`
                lEvent.miCarTakedowns      = lpStats->GetTakedownTypeCount(10);
                lEvent.miVanTakedowns      = lpStats->GetTakedownTypeCount(11);
                lEvent.miBusTakedowns      = lpStats->GetTakedownTypeCount(12);
                lEvent.miBigRigTakedowns   = 0;                         // `stw r19`

                lEvent.mRoadsRuledTime     = lpStats->GetRoadsRuledCount(0);
                lEvent.mRoadsRuledCrash    = lpStats->GetRoadsRuledCount(1);
                lEvent.mRoadsRuledComplete = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTALROADSRULED);
                lEvent.mNumberOfRoads      = lpStats->GetTotalRoads();

                lEvent.miWinsToNextRank = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK);
                lEvent.miCarsToShutdown = lpStats->GetValue(GS::E_INT_VALUE_TYPE_TOTAL_CARS_TO_SHUTDOWN);
                // `lfs f0, 0xA4(r31) / fctiwz / stfiwx` -- the float percentage is TRUNCATED to an
                // int here, which is why the panel shows a whole number.
                lEvent.miPercentageComplete =
                    static_cast<s32>(lpStats->GetValue(GS::E_FLOAT_VALUE_PERCENTAGE_COMPLETE));

                lEvent.miDriveThrusFound = lpProfile->GetDriveThrusFound();   // first of two calls

                // the five "won" counters -- Profile+336/+348/+356/+364/+368, i.e. modes
                // 0 / 3 / 5 / 7 / 8 of maGameModeTypeAmountCompletedSinceTheStart.
                lEvent.miRacesWon     = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_OFFLINE_RACE);
                lEvent.miRoadRagesWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_ROAD_RAGE);
                lEvent.miMarkedManWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_MARKED_MAN);
                lEvent.miChallengesWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_BURNING_ROUTE);
                lEvent.miStuntRunsWon = lpProfile->GetGameModeTypeCompletedSinceTheStart(GsmIO::E_MODE_STUNT_ATTACK);

                lEvent.miBestShowtime = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_SHOWTIME);
                lEvent.miBestRoadRageTakedownCount = lpProfile->GetHighestNumberOfTakeDownsInRoadRage();
                lEvent.miBestBoostChain = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_BOOST_CHAIN);
                lEvent.miBestDrift      = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_DRIFT);
                lEvent.miBestOncoming   = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_ONCOMING);
                // the ONLY two `stfs` in the arm -- these stay floats end to end.
                lEvent.mfBestAirtime = lpStats->GetValue(GS::E_FLOAT_VALUE_TYPE_BEST_AIRTIME);
                lEvent.mfBestSpin    = lpStats->GetValue(GS::E_FLOAT_VALUE_TYPE_BEST_SPIN);
                lEvent.miBestNumBarrelRolls = lpStats->GetValue(GS::E_INT_VALUE_TYPE_BEST_NO_BARREL_ROLLS);
                lEvent.miHighestStuntScore  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_HIGHEST_STUNT_SCORE);
                lEvent.miEventsFound  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_EVENTS_FOUND);
                lEvent.miTotalEvents  = lpStats->GetValue(GS::E_INT_VALUE_TYPE_EVENTS_TOTAL);

                // the four discovered-drive-thru Set lengths. The console inlines
                // Set<CgsID,11/14/5/5>::GetLength on Profile+42568/+42712/+42664/+42520, which is
                // exactly Profile::GetNumDriveThrusDiscovered's four non-car-park arms.
                lEvent.miBodyShopsFound   = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP);
                lEvent.miGasStationsFound = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_GAS_STATION);
                lEvent.miPaintShopsFound  = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP);
                lEvent.miJunkYardsFound   = lpProfile->GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD);

                lEvent.miBodyShopsTotal   = 11;   // `li r11, 0xB`
                lEvent.miGasStationsTotal = 14;   // `li r11, 0xE`
                lEvent.miPaintShopsTotal  = 5;    // `li r11, 5`
                lEvent.miJunkYardsTotal   = 5;    // `li r11, 5`
                lEvent.miTotalDriveThrus  = 35;   // `li r11, 0x23`  (== 11 + 14 + 5 + 5)
                lEvent.miTotalDriveThrusFound = lpProfile->GetDriveThrusFound();   // second call

                // the six district columns, one district per iteration (the console walks ONE
                // word cursor over the action's two 3x5 grids and six destination bases).
                for (s32 liDistrict = 0;
                     liDistrict < BrnGui::GuiEventStatsResponse::KI_NUM_DISTRICTS;
                     ++liDistrict)
                {
                    lEvent.mBillboardStunts[liDistrict] =
                        lpStats->GetCurrentStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD, liDistrict);
                    lEvent.mJumpStunts[liDistrict] =
                        lpStats->GetCurrentStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP, liDistrict);
                    lEvent.mSmashStunts[liDistrict] =
                        lpStats->GetCurrentStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH, liDistrict);
                    lEvent.mMaxBillboardStunts[liDistrict] =
                        lpStats->GetMaxStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD, liDistrict);
                    lEvent.mMaxJumpStunts[liDistrict] =
                        lpStats->GetMaxStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP, liDistrict);
                    lEvent.mMaxSmashStunts[liDistrict] =
                        lpStats->GetMaxStuntElementPerCounty(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH, liDistrict);
                }

                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ---- 181  the RANK-PROGRESS RESPONSE (36 bytes) -------------------------------
            // ⭐⭐ [driver-details pause wave 2026-08-28] X360 case 181 @0x823ECC90, and it is
            // exactly two calls:
            //   0x823ECC90  mr   r4, r31                      ; the action record
            //   0x823ECC94  addi r3, r1, var_35C0             ; a stack-local event
            //   0x823ECC98  bl   GuiEventRankProgressResponse::Construct
            //   0x823ECC9C  mr   r5, r20                      ; lpGuiInput
            //   0x823ECCA0  addi r4, r1, var_35C0
            //   0x823ECCA4  add  r3, r29, r30                 ; the embedded GuiModule
            //   0x823ECCA8  bl   AddGuiEvent<GuiEventRankProgressResponse>   ; id 438, 36 bytes
            // Construct is the nine-word rotation documented in the event's own header.
            //
            // THIS IS THE LAST HOP of the START-button pause screen's licence ladder:
            //   GUI 437 -> game event 80 -> game action 181 -> GUI 438 (here). Without it
            // CrashNavDriverDetails::UpdateSetupLicense never advances past
            // E_INTERNALSTATE_SETUPLICENSE and the screen never loads its apt movie.
            // ---- 179  E_ACTION_EVENT_STATE_RESPONSE (1404 bytes) -> GUI event 556 --------------
            // X360 TranslateGameActionsToGuiEvents @0x823E9CE0 case 179: assert the record
            // (GameBridgeGameStateToX.cpp:3610), `memcpy(local, action, 1404)`, then
            // AddGuiEvent<GuiEventEventStateResponse> @0x823D85F8 == AddEvent(queue, local, 556,
            // 1404). The payload IS the Array<ProfileEvent,175> the game state built; GuiCache's
            // case 556 copies the same 1404 bytes into its profile-event array.
            case BrnGameState::GameStateModuleIO::E_ACTION_EVENT_STATE_RESPONSE:
            {
                CGS_ASSERT(lpAction != 0, "lpEventStateResponse");   // cpp:3610
                struct EventStateResponseWire556
                {
                    u8 mau8Payload[1404];
                    s32 GetEventType() const { return 556; }
                };
                CGS_STATIC_ASSERT((sizeof(EventStateResponseWire556) == 1404),
                              "X360 AddGuiEvent<GuiEventEventStateResponse> posts 1404 bytes (id 556)");
                EventStateResponseWire556 lEvent;
                std::memcpy(lEvent.mau8Payload, lpAction, sizeof(lEvent.mau8Payload));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_RANK_INFO_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::RankInfoResponseAction* lpRankInfo =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::RankInfoResponseAction*>(lpAction);

                BrnGui::GuiEventRankProgressResponse lEvent;    // id 438, 36 bytes
                lEvent.Construct(lpRankInfo);
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ARTIST 0x823EBC10..0x823EBC48 posts the 64-byte action-182 image as GUI 406.
            // The first 56 bytes are the PlayerInfo payload itself (the GUI event's first 12
            // bytes are not a queue header here); the console then writes only damage at +0x38.
            case BrnGameState::GameStateModuleIO::E_ACTION_PLAYER_INFO_RESPONSE:
            {
                typedef BrnGameState::GameStateModuleIO::PlayerInfoResponseAction PlayerInfoAction;
                CGS_STATIC_ASSERT((sizeof(PlayerInfoAction) == 64), "X360 action 182 payload size");
                CGS_STATIC_ASSERT((sizeof(BrnGui::GuiPlayerInfoResponse) == 64),
                              "X360 GUI 406 payload size");
                const PlayerInfoAction& lrAction = *reinterpret_cast<const PlayerInfoAction*>(lpAction);
                BrnGui::GuiPlayerInfoResponse lEvent;
                std::memcpy(&lEvent, &lrAction.mPlayerInfo, sizeof(lrAction.mPlayerInfo));
                std::memcpy(reinterpret_cast<u8*>(&lEvent) + 0x38,
                            &lrAction.mbIsCarDamaged, sizeof(lrAction.mbIsCarDamaged));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 188 / 189  RIVALRY DATA RESPONSES ------------------------------------
            // X360 cases 188/189 at 0x823ECCB0/0x823ECE0C post GUI 442/688 and 444/32.
            // The GameState producers already post these typed records: SendAllRivalryData
            // (action 188, 1288 bytes) and ProcessGameEvents case 102 (action 189, 32 bytes).
            case BrnGameState::GameStateModuleIO::E_ACTION_ALL_RIVALRY_DATA_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::RivalryOverviewAction& lrAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::RivalryOverviewAction*>(lpAction);
                BrnGui::GuiEventRivalryFullInfoResponse lEvent;   // GUI 442, 688 bytes; plain record

                // ARTIST compares signed count > 34, asserts at the original line, then still
                // iterates the supplied count. Keep that continuation and do not clamp it.
                if (lrAction.miRivalsReturned > BrnGui::GuiEventRivalryFullInfoResponse::KI_MAX_NUM_RIVALS_IN_EVENT)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "BrnGui::GuiEventRivalryFullInfoResponse::KI_MAX_NUM_RIVALS_IN_EVENT >= lpRivalOverview->miRivalsReturned",
                        CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                        3047));
                    CgsDev::Assert::EndAssert();
                }

                lEvent.miRivalsReturned = lrAction.miRivalsReturned;
                for (s32 liRival = 0; liRival < lrAction.miRivalsReturned; ++liRival)
                {
                    lEvent.maRivalIds[liRival]    = lrAction.mRivalIDs[liRival];
                    lEvent.maRivalCarIds[liRival] = lrAction.mRivalCarIDs[liRival];

                    s32 liGuiStatus;
                    bool lbGuiStatusWasWritten = true;
                    switch (lrAction.mRivalryStatus[liRival])
                    {
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_UNKNOWN:
                        liGuiStatus = 1; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_DRIVER:
                        liGuiStatus = 2; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_RIVAL:
                        liGuiStatus = 3; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_TARGET:
                        liGuiStatus = 4; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_WRECKED:
                        liGuiStatus = 5; break;
                    case BrnGameState::GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_INVALID:
                        liGuiStatus = 0; break;
                    default:
                        // The original asserts with the numeric stage, continues, and does not
                        // store this output slot. Leave the stack bytes unwritten on this path.
                        CgsDev::Assert::BeginAssert();
                        {
                            CgsDev::StrStream lAssertStream(CgsDev::Assert::gpcMessageBuffer,
                                                            CgsDev::Assert::giMessageBufferSize);
                            lAssertStream << "Unknown rivalry type "
                                          << static_cast<s32>(lrAction.mRivalryStatus[liRival]) << "\n";
                        }
                        CgsDev::Assert::FireAssert(
                            CgsDev::Assert::gpcMessageBuffer,
                            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                            3099));
                        CgsDev::Assert::EndAssert();
                        lbGuiStatusWasWritten = false;
                        break;
                    }
                    if (lbGuiStatusWasWritten)
                        lEvent.maRivalryStatus[liRival] = liGuiStatus;
                }

                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_ONE_RIVALRY_DATA_RESPONSE:
            {
                const BrnGameState::GameStateModuleIO::RivalryOneInDepthAction& lrAction =
                    *reinterpret_cast<const BrnGameState::GameStateModuleIO::RivalryOneInDepthAction*>(lpAction);
                BrnGui::GuiEventRivalInfoResponse lEvent;   // GUI 444, 32 bytes; plain record

                lEvent.mRivalId      = lrAction.mId;
                lEvent.mCarId        = lrAction.mCarId;
                lEvent.meCountyIndex = lrAction.meCountyIndex;
                lEvent.miRaceWins    = lrAction.miRaceWins;

                // ARTIST sign-extends the s8 field before asserting, then stores that byte even
                // after the assertion; preserve both the full signed range check and raw value.
                if (lrAction.miProgressStatus < 0 || lrAction.miProgressStatus > 100)
                {
                    CgsDev::Assert::BeginAssert();
                    {
                        CgsDev::StrStream lAssertStream(CgsDev::Assert::gpcMessageBuffer,
                                                        CgsDev::Assert::giMessageBufferSize);
                        lAssertStream << "Invalid progress"
                                      << static_cast<s32>(lrAction.miProgressStatus) << "\n";
                    }
                    CgsDev::Assert::FireAssert(
                        CgsDev::Assert::gpcMessageBuffer,
                        CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
                        3120));
                    CgsDev::Assert::EndAssert();
                }

                lEvent.miProgressStatus = lrAction.miProgressStatus;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // =====================================================================
            // ⭐⭐⭐ 127 / 128 / 139-144 -- THE SHOWTIME FAMILY (showtime-score wave 2026-08-29)
            // =====================================================================
            // jpt_823EA1F0's "cases 127,128,139-144" arm, @0x823ED714..0x823ED754, transcribed
            // instruction for instruction:
            //     lwz  r11, var_3534(r1)   ; the local set once in the prologue @0x823E9D84 to
            //                                `r31 + 0x30000 - 0x5B48` == lpGameStateOutput+173240
            //                                == OutputBuffer::GetScoringOutputInterface()
            //     lwz  r11, 0xA3C(r11)     ; +0xA3C is ScoringOutputInterface::meGameModeType
            //                                (the committed member run pins mePlayerRaceCarIndex
            //                                 at +0xA34 and miNumPlayersInGame at +0xA38, so the
            //                                 next word is meGameModeType -- and the sibling
            //                                 event-status slice already reads it by that name)
            //     cmpwi 2 / beq ; cmpwi 0x10 / bne -> default   ; the offline/online showtime pair
            //     mr r4, r3 ; mr r5, r31 ; mr r6, r20 ; mr r3, r29
            //     bl TranslateShowtimeActionToGuiEvent
            //
            // ⚠️ Read through the SCORING OUTPUT INTERFACE, not through
            // GetModeManager()->GetCurrentGameModeType(), even though both words track the same
            // mode: the console reads the published output-buffer copy at this seat, and the
            // ModeManager route would be a different member on a different object with a
            // different publish latency. Same rule the GUI-377 producer's own note states.
            //
            // ⛔ NOT MODE-GATED BY US TWICE: the arm below is the whole gate; the callee has none.
            case BrnGameState::GameStateModuleIO::E_ACTION_WORLD_STUNT_PERFORMED:   // 127
            case BrnGameState::GameStateModuleIO::E_ACTION_OVERHEAD_SIGN_HIT:       // 128
            case BrnGameState::GameStateModuleIO::E_ACTION_VEHICLE_LEAPT:           // 139
            case BrnGameState::GameStateModuleIO::E_ACTION_VEHICLE_HIT:             // 140
            case BrnGameState::GameStateModuleIO::E_ACTION_ENTER_NEW_ROAD:          // 141
            case BrnGameState::GameStateModuleIO::E_ACTION_SHOWTIME_UPDATE:         // 142
            case BrnGameState::GameStateModuleIO::E_ACTION_SHOWTIME_MODE_SWITCH:    // 143
            case BrnGameState::GameStateModuleIO::E_ACTION_JUST_BOUNCED:            // 144
            {
                const BrnGameState::GameStateModuleIO::ScoringOutputInterface* const lpScoring =
                    lpGameStateOutput->GetScoringOutputInterface();

                // The console does not null-test here (it computes the address inline off a
                // buffer it already asserted), so neither does this -- adding a test would be an
                // invented arm. The accessor returns the address of an embedded sub-object.
                const s32 liGameModeType = static_cast<s32>(lpScoring->meGameModeType);
                const bool lbShowtime =
                    (liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
                     liGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);


                if (lbShowtime)
                {
                    TranslateShowtimeActionToGuiEvent(liActionType, lpAction, lpGuiInput);
                }
                break;
            }

            // ARTIST 0x823ED570..0x823ED5C4: action294 is the image-save request that becomes
            // GUI358. The action producer widens its pointer to the host width; copy the whole
            // 24-byte ImageFileInfo-shaped record, including the unwritten trailing X360 word.
            case 294:
            {
                const BrnGameState::GameStateModuleIO::ImageSaveRequestAction* lpSaveImageAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::ImageSaveRequestAction*>(lpAction);
                CGS_ASSERT(lpSaveImageAction != 0, "lpSaveImageAction");
                BrnGui::GuiEventSaveImageFileAndAutosave lEvent;
                std::memcpy(&lEvent.mImageFileInfo, lpSaveImageAction, sizeof(lEvent.mImageFileInfo));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823ED5C8..0x823ED608: assert the action then copy the whole image-file
            // record array to GUI 359. The X360 3x16-byte record widens to 3x24 bytes here.
            case 295: // ImageManager::KI_EVENT_PREPARE_RENDER
            {
                CGS_ASSERT(lpAction != 0, "lpLoadImagesAction");
                BrnGui::GuiEventLoadImageFiles lEvent;
                std::memcpy(lEvent.maImageFileInfo, lpAction, sizeof(lEvent.maImageFileInfo));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ---- 152..160  the free-burn challenge family (online free-burn lobby) ------------
            // The online time-out clock: the {seconds, fraction} record becomes GUI 108.
            case BrnGameState::GameStateModuleIO::E_ACTION_MODE_TIME_REMAINING:
            {
                CGS_ASSERT(lpAction != 0, "lpTimeOutTimer != NULL");
                OnlineTimeoutWire108 lEvent;
                lEvent.mfTimeLeft =
                    reinterpret_cast<const CgsSystem::Time*>(lpAction)->GetFloatVal();
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // The challenge lifecycle: begin -> 574, trigger -> 576, ended -> 108 (-1.0) + 578 +
            // 104, results finished -> 579 + 104. The other event types post nothing.
            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE:
            {
                CGS_ASSERT(lpAction != 0, "lpChallengeAction");
                const BrnGameState::GameStateModuleIO::FreeburnChallengeAction* lpChallengeAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::FreeburnChallengeAction*>(lpAction);

                switch (lpChallengeAction->meEventType)
                {
                case 0:
                {
                    BrnGui::GuiChallengeStartEvent lEvent;
                    lEvent.mChallengeID  = lpChallengeAction->mChallengeID;
                    lEvent.mbIsLocalHost = lpChallengeAction->mbIsHost;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 1:
                {
                    BrnGui::GuiChallengeTriggerResponse lEvent;
                    lEvent.mChallengeID  = lpChallengeAction->mChallengeID;
                    lEvent.mbIsLocalHost = lpChallengeAction->mbIsHost;
                    PushGuiEvent(lEvent, lpGuiInput);
                    break;
                }
                case 5:
                {
                    OnlineTimeoutWire108 lTimeout;
                    lTimeout.mfTimeLeft = -1.0f;
                    PushGuiEvent(lTimeout, lpGuiInput);

                    BrnGui::GuiChallengeEndEvent lEvent;
                    lEvent.mChallengeID                  = lpChallengeAction->mChallengeID;
                    lEvent.meChallengeStatus             = lpChallengeAction->meChallengeStatus;
                    lEvent.miNumChallengesComplete       = lpChallengeAction->miNumChallengesComplete;
                    lEvent.miTotalNumChallenges          = lpChallengeAction->miTotalNumChallenges;
                    lEvent.mbAbortingToStartNewChallenge = lpChallengeAction->mbAbortingToStartNewChallenge;
                    PushGuiEvent(lEvent, lpGuiInput);

                    PushGuiTag(104, lpGuiInput);
                    break;
                }
                case 6:
                    PushGuiTag(579, lpGuiInput);
                    PushGuiTag(104, lpGuiInput);
                    break;
                default:
                    break;
                }
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_END_NOT_ACTIVE:
                PushGuiTag(584, lpGuiInput);
                break;

            // The per-frame progress block becomes GUI 577 (one target slot per used target),
            // then the time left goes out as GUI 108.
            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_UPDATE:
            {
                const BrnGameState::GameStateModuleIO::FreeburnChallengeUpdateAction* lpUpdateAction =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::FreeburnChallengeUpdateAction*>(lpAction);
                CGS_STATIC_ASSERT((sizeof(BrnGui::GuiChallengeUpdateEvent) == 144), "GUI 577 size 144");

                BrnGui::GuiChallengeUpdateEvent lUpdateEvent = BrnGui::GuiChallengeUpdateEvent();
                lUpdateEvent.miCurrentAction = lpUpdateAction->miCurrentActionIndex;
                CGS_ASSERT(lUpdateEvent.miCurrentAction < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                           "lUpdateEvent.miCurrentActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");
                lUpdateEvent.miNumTargetsUsed = lpUpdateAction->miNumTargetsUsed;
                for (s32 liTarget = 0; liTarget < lpUpdateAction->miNumTargetsUsed; ++liTarget)
                {
                    lUpdateEvent.maiOverallTargetRemaining[liTarget] =
                        lpUpdateAction->maiOverallTargetRemaining[liTarget];
                    std::memcpy(lUpdateEvent.maafIndividualTargetContributions[liTarget],
                                lpUpdateAction->maafIndividualTargetContributions[liTarget],
                                sizeof(lUpdateEvent.maafIndividualTargetContributions[liTarget]));
                    std::memcpy(lUpdateEvent.maaeComplete[liTarget],
                                lpUpdateAction->maaeCompleted[liTarget],
                                sizeof(lUpdateEvent.maaeComplete[liTarget]));
                }
                PushGuiEvent(lUpdateEvent, lpGuiInput);

                OnlineTimeoutWire108 lTimeout;
                lTimeout.mfTimeLeft = lpUpdateAction->mfTimeLeftInChallenge;
                PushGuiEvent(lTimeout, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_EVERY_PLAYER_COMPLETION_STATUS:
            {
                FburnChallengeEveryPlayerStatusWire581 lEvent;
                std::memcpy(&lEvent.mStatus, lpAction, sizeof(lEvent.mStatus));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            case BrnGameState::GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR:
            {
                CGS_ASSERT(lpAction != 0, "lpShowSelectorAction");
                ShowFreeburnChallengeWire582 lEvent;
                lEvent.mChallengeID =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::FburnChallengeShowSelectorAction*>(
                        lpAction)->mChallengeID;
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // [RR-GUI] 279 -> GUI 338, the per-frame road-rule score. The only road-rule arm that
            // reads past the action: the crash half comes off the game module's own crash scorer
            // (the embedded ModeManager's ScoringSystem CrashModeScoring raw score and
            // multiplier), not off the action's crash score, which this arm never reads. The time
            // half is the action's, unless the debug worst-case-HUD switch is up, when its time
            // stands in. The record is a frame local on the console: the fields a cleared flag
            // skips carry stack residue there, zero here.
            case BrnGameState::GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE:
            {
                const BrnGameState::GameStateModuleIO::RoadRulesUpdateAction* lpUpdate =
                    reinterpret_cast<const BrnGameState::GameStateModuleIO::RoadRulesUpdateAction*>(lpAction);

                BrnGui::GuiEventRoadRuleUpdate lEvent;
                memset(&lEvent, 0, sizeof(lEvent));
                if (BrnGui::GuiEventRoadRuleUpdate::msbWorstCaseHudActive)
                {
                    lEvent.mfCurrentTimeElapsed = BrnGui::GuiEventRoadRuleUpdate::msfWorstCaseTimeElapsed;
                }
                else if (lpUpdate->IsScoreActive(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    lEvent.mfCurrentTimeElapsed = lpUpdate->GetScore(BrnStreetData::E_SCORE_TYPE_TIME);
                }
                if (lpUpdate->IsScoreActive(BrnStreetData::E_SCORE_TYPE_CRASH))
                {
                    const BrnGameState::CrashModeScoring* lpCrashScorer =
                        mGameStateModule.GetModeManager()->GetScoringSystem()->GetCrashScorer();
                    lEvent.miCrashMultiplier = lpCrashScorer->GetScoreMultiplier();
                    lEvent.mfCurrentCrash    = static_cast<f32>(lpCrashScorer->GetRawScore());
                }
                PushGuiEvent(lEvent, lpGuiInput);

                break;
            }

            // ARTIST 0x823EDC74..0x823EDCA8: publish the pending developer-challenge bitset.
            case BrnGameState::GameStateModuleIO::E_ACTION_DEVELOPER_CHALLENGES_COMPLETED:
            {
                CGS_ASSERT(lpAction != 0, "lpDevCompAction");
                CGS_STATIC_ASSERT((sizeof(BrnGui::GuiDeveloperChallengesCompleted) == 8),
                              "ARTIST action 298 forwards one FastBitArray<15> word as GUI 596");
                BrnGui::GuiDeveloperChallengesCompleted lEvent;
                std::memcpy(&lEvent.mCompletedDeveloperChallenges, lpAction,
                            sizeof(lEvent.mCompletedDeveloperChallenges));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EDA38..0x823EDA80: action 240 publishes the 16-byte holder name
            // followed by the removal flag as raw GUI event 122 (17 bytes total).
            case BrnGameState::GameStateModuleIO::TargetEventScoreChangedResponse::KI_GAME_ACTION_TYPE:
            {
                CGS_ASSERT(lpAction != 0, "lpTargetScoreAction");
                const BrnGameState::GameStateModuleIO::TargetEventScoreChangedResponse* lpResponse = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::TargetEventScoreChangedResponse*>(lpAction);
                BrnGui::GuiEventScoreboardResponseEvScoreTarget lEvent;
                std::memcpy(lEvent.maData, &lpResponse->mPlayerName, sizeof(lpResponse->mPlayerName));
                lEvent.maData[16] = static_cast<u8>(lpResponse->mbRemoved);
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EDA84..0x823EDAC4: action 241's 16 raw name bytes are copied over
            // the full GUI-event-123 object before AddGuiEvent queues all 16 bytes.
            case BrnGameState::GameStateModuleIO::DldScoreboardResponse::KI_GAME_ACTION_TYPE:
            {
                CGS_ASSERT(lpAction != 0, "lpDldChallengeableAction");
                const BrnGameState::GameStateModuleIO::DldScoreboardResponse* lpResponse = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::DldScoreboardResponse*>(lpAction);
                BrnGui::GuiEventScoreboardDownloadedChallengeable lEvent;
                CGS_STATIC_ASSERT((sizeof(lEvent) == sizeof(*lpResponse)),
                              "ARTIST case 241 copies the same 16-byte record to GUI event 123");
                std::memcpy(&lEvent, lpResponse, sizeof(lEvent));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            // ARTIST 0x823EC84C..0x823EC89C: action 288 answers an event-data request with
            // {event id, score, player name[16]}. The GUI-332 consumer reads those bytes at
            // +0/+8/+0xC; the final four bytes of this 32-byte local are never written.
            case BrnGameState::GameStateModuleIO::TargetEventDataResponse::KI_GAME_ACTION_TYPE:
            {
                CGS_ASSERT(lpAction != 0, "lpChallengeEventDataAction");
                const BrnGameState::GameStateModuleIO::TargetEventDataResponse* lpResponse = reinterpret_cast<const
                    BrnGameState::GameStateModuleIO::TargetEventDataResponse*>(lpAction);
                BrnGui::GuiEventChallengedEventDataResponse lEvent;
                std::memcpy(&lEvent.mu64EventID, &lpResponse->mEventId, sizeof(lpResponse->mEventId));
                lEvent.miScore = lpResponse->miScore;
                std::memcpy(lEvent.maPlayerName, &lpResponse->mPlayerName, sizeof(lpResponse->mPlayerName));
                PushGuiEvent(lEvent, lpGuiInput);
                break;
            }

            default:
                // [stuntrace wave E1, 2026-08-26] the EVENT-FLOW arms (23/37/38/39/44/47/200/201)
                // live in the sibling GameBridgeGameStateToX_EventFlowGuiEvents.cpp -- one drain
                // walk, split across sibling TUs exactly like this one.
                // [FLAG] original explicit action cases without an arm remain unresolved; their absence here is not a no-op disposition.
                BrnGame::TranslateEventFlowGameActionToGuiEvent(
                    liActionType, lpAction, lpGuiInput, lpGameStateOutput);
                break;
            }

            const CgsModule::Event* lpNextAction = 0;
            liActionType = lpActionQueue->GetNextEvent(lpAction, &lpNextAction, &liActionSize);
            lpAction     = lpNextAction;
        }

        // ---- the tail: the pending sat-nav record, posted once ------------------------------
        // The console posts the pending GuiEventUpdateSatNav (id 199, 2320 bytes) after the drain
        // when its icon count is > 0. (It follows the console's unconditional
        // GuiEventRacePositionInfo post above; the two are
        // independent.) The count is written into the record's own +0x900 word, which is what
        // the cache's case-199 arm reads.
        // ARTIST0x823EDCE0..0x823EDD1C: publish standings even on frames with no actions.
        const BrnGameState::GameStateModuleIO::ScoringOutputInterface* scoring = lpGameStateOutput->GetScoringOutputInterface();
        BrnGui::GuiEventRacePositionInfo positions;
        for (s32 i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
        {
            positions.maiPositions[i] = static_cast<s8>(scoring->maCarScoreData[i].GetRacePosition());
            positions.mabFinished[i] = scoring->maCarScoreData[i].GetHasFinished();
            positions.mabValid[i] = scoring->mabValid[i];
        }
        PushGuiEvent(positions, lpGuiInput);

        if (liPendingSatNavIcons > 0)
        {
            lPendingSatNavEvent.miNumIcons = liPendingSatNavIcons;
            PushGuiEvent(lPendingSatNavEvent, lpGuiInput);

        }
    }
} // namespace BrnGame

// FOLDED FROM GameBridgeGameStateToX_TrainingStringIds.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_TrainingStringIds.cpp
//
// PER-FUNCTION SIBLING of GameSource/Game/GameBridgeGameStateToX.cpp, split out on
// 2026-08-16 (tutorial-ticker leg). Homes exactly one X360 function plus the two rodata
// tables it indexes:
//
//   BrnGame::ConvertTrainingTypeToStringId   0x823AA3B8
//
// ⭐ WHAT THIS IS FOR. It is the one step that turns a training-tip enum into the GUI
// string id the bottom-of-screen tutorial ticker looks up in the language data. The full
// console chain the game shows the "how to start the car" text through is:
//
//   TrainingManager::Update @0x823937D0 (state 1)
//     -> TrainingManager::SendTrainingTickerMessage @0x82388940
//          -> GameAction 148, payload = the 4-byte BrnProgression::ETrainingType
//   BrnGameModule::BridgeGameStateToGui @0x823EE880
//     -> TranslateGameActionsToGuiEvents @0x823E9CE0, case 148
//          -> **ConvertTrainingTypeToStringId (THIS FUNCTION)**
//          -> GuiEventTickerCustomMessage::AddString(id, 2) -> GUI event 537
//   BrnGui::CustomRendererManager::RecvEvent -> case 537
//     -> BrnGui::InGameMessageRenderer (the Im2d ticker) + BrnGui::BlackBarRenderer
//
// ⛔ ONLY THIS STEP IS LIVE. Nothing on this build produces GameAction 148 (the
// TrainingManager is neither constructed nor updated) and nothing consumes GUI event 537
// (InGameMessageRenderer has 15 X360 functions and none is reconstructed; its draw path
// needs CgsGraphics::TextRenderer, which has no reconstructed home in this tree at all).
// This TU is deliberately mounted anyway: it is correct, complete, costs zero unresolved
// externals, and it is the piece that was previously blocked on data nobody had.
//
// WHY IT IS A SEPARATE TU: the owning GameBridgeGameStateToX.cpp did not compile when this
// leg landed (an ODR fork on BrnGui::GuiTakedownEvent plus a stale mpCgsGuiModule reference,
// both inside TranslateTakedownsToGuiEvents). Both are repaired: that parent TU now compiles
// and is mounted. Folding this file back into it is a delete, not a duplicate-symbol hunt,
// because the function was MOVED and not copied.
// ============================================================================

#include "GameSource/Game/GameBridgeGameStateToX.h"        // BrnGame::ConvertTrainingTypeToStringId decl

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "SharedClasses/Progression/BrnTrainingTypes.h"    // BrnProgression::ETrainingType

namespace BrnGame
{
    // ------------------------------------------------------------------------
    // Two file-scope string-ID lookup tables (X360 rodata off_82CDBF40 /
    // dword_82FAE290). Both are const char*[] with a 4-byte element stride
    // (asm: slwi rX, rX, 2 ; lwzx).
    //   KAC_SPECIFIC_TRAINING_TEXT : const char*[77]   (@0x82CDBF40)
    //   KAC_GENERAL_TRAINING_TEXT  : const char*[128]  (@0x82FAE290)
    //
    // ⭐ RECOVERED FROM THE IMAGE (2026-08-16, tutorial-ticker leg). The previous banner
    // said the contents were "UNRECOVERABLE from this TU's dossier" and left both arrays
    // `extern`-only -- which made this TU unlinkable and left ConvertTrainingTypeToStringId
    // with no data. They are not unrecoverable: the IDA function-only export set has no
    // .rdata, but the UNPACKED .i64 image bytes are readable, and both tables are plain
    // arrays of big-endian pointers into the string blob.
    //
    // ⚠️ NOT TRUSTED ON THE READER'S WORD -- three independent controls, all passed:
    //   1. Element 0 is "TRAINING_LEAVES_JUNKYARD", the ONE entry the old banner already
    //      attested from the dossier. The recovery reproduces it exactly.
    //   2. MEASURED, not eyeballed: 53 of the 54 non-null entries spell the matching
    //      BrnProgression::ETrainingType enumerator (index 2 == TRAINING_START_ENGINE ==
    //      E_TRAINING_TYPE_START_ENGINE, 5/6/7 == the three DRIVES_*_CAR tips, 41/42/43 ==
    //      INTRO_TO_ONLINE_1..3, 71..76 == the six TRAFFIC_*_LICENSE tips). The single
    //      non-match is index 34, "TRAINING_FOUND_CAR_PARK" against
    //      E_TRAINING_TYPE_FINDS_CAR_PARK -- a tense difference between the authored string
    //      id and the enumerator, not a misalignment (both sit at 34, and every neighbour
    //      matches). That enum was derived independently (DWARF + the RequestTraining /
    //      DoesTrainingPauseGame switch arms), so the two agreeing is a real cross-check,
    //      not a restatement.
    //   3. Every recovered id resolves in the SHIPPED language data: CRC-32(id) is present
    //      exactly once in LANGUAGE/0002.bundle (already loaded at boot), and the string it
    //      points at is the tutorial line -- e.g. TRAINING_START_ENGINE resolves to
    //      "Okay, let's just check this thing still starts. Hold the accelerator (right
    //      trigger) to fire up the engine."
    //
    // The NULL slots are null IN THE IMAGE (types with no ticker string: 4, 9, 16..19, 32,
    // 33, 39, 48, 56, 57, 59..69). ConvertTrainingTypeToStringId returns them verbatim and
    // its caller (TranslateGameActionsToGuiEvents case 148) tests the result for NULL before
    // building the ticker event, so a null is a live, load-bearing value -- not a hole.
    static const char* const KAC_SPECIFIC_TRAINING_TEXT[77] =
    {
        /*  0 */ "TRAINING_LEAVES_JUNKYARD",
        /*  1 */ "TRAINING_MAP_APPEARS",
        /*  2 */ "TRAINING_START_ENGINE",
        /*  3 */ "TRAINING_FIRST_USE_AUTOREPAIR",
        /*  4 */ 0,
        /*  5 */ "TRAINING_DRIVES_STUNT_CAR",
        /*  6 */ "TRAINING_DRIVES_DANGER_CAR",
        /*  7 */ "TRAINING_DRIVES_AGGRESSION_CAR",
        /*  8 */ "TRAINING_DISCOVERS_EVENT",
        /*  9 */ 0,
        /* 10 */ "TRAINING_USES_GAS_STATION",
        /* 11 */ "TRAINING_USES_BODY_SHOP",
        /* 12 */ "TRAINING_SUPER_JUMP",
        /* 13 */ "TRAINING_BILLBOARD",
        /* 14 */ "TRAINING_SMASH",
        /* 15 */ "TRAINING_BARREL_ROLL",
        /* 16 */ 0,
        /* 17 */ 0,
        /* 18 */ 0,
        /* 19 */ 0,
        /* 20 */ "TRAINING_DANGER_BOOST_FULL",
        /* 21 */ "TRAINING_BURNOUT",
        /* 22 */ "TRAINING_AGGRESSION_TAKEDOWN",
        /* 23 */ "TRAINING_AGGRESSION_LOST_BOOST_CHUNK",
        /* 24 */ "TRAINING_CORRECT_CAR_FOR_CHALLENGE",
        /* 25 */ "TRAINING_WRONG_CAR_FOR_CHALLENGE",
        /* 26 */ "TRAINING_ROAD_RULES_ON",
        /* 27 */ "TRAINING_ROAD_RULES_FORCE_ON",
        /* 28 */ "TRAINING_CRASH_ROAD_RULES_ON",
        /* 29 */ "TRAINING_WON_ROAD_RULE_TIME",
        /* 30 */ "TRAINING_WON_ROAD_RULE_CRASH",
        /* 31 */ "TRAINING_WON_ROAD",
        /* 32 */ 0,
        /* 33 */ 0,
        /* 34 */ "TRAINING_FOUND_CAR_PARK",
        /* 35 */ "TRAINING_USES_E_BRAKE",
        /* 36 */ "TRAINING_FLAT_SPIN",
        /* 37 */ "TRAINING_POWER_PARK",
        /* 38 */ "TRAINING_MAP_INFORMATION",
        /* 39 */ 0,
        /* 40 */ "TRAINING_TAKEDOWN",
        /* 41 */ "TRAINING_INTRO_TO_ONLINE_1",
        /* 42 */ "TRAINING_INTRO_TO_ONLINE_2",
        /* 43 */ "TRAINING_INTRO_TO_ONLINE_3",
        /* 44 */ "TRAINING_WON_ROAD_RULE_ONLINE",
        /* 45 */ "TRAINING_LOST_ROAD_RULE_ONLINE",
        /* 46 */ "TRAINING_USES_PAINT_SHOP",
        /* 47 */ "TRAINING_QUITS_EVENT",
        /* 48 */ 0,
        /* 49 */ "TRAINING_BARREL_ROLL_FAILED",
        /* 50 */ "TRAINING_BOOST",
        /* 51 */ "TRAINING_BOOST",
        /* 52 */ "TRAINING_BOOST",
        /* 53 */ "TRAINING_BOOST",
        /* 54 */ "TRAINING_BOOST",
        /* 55 */ "TRAINING_2ND_WRECK_THROUGH_AUTOREPAIR",
        /* 56 */ 0,
        /* 57 */ 0,
        /* 58 */ "TRAINING_TRY_A_FLAT_SPIN",
        /* 59 */ 0,
        /* 60 */ 0,
        /* 61 */ 0,
        /* 62 */ 0,
        /* 63 */ 0,
        /* 64 */ 0,
        /* 65 */ 0,
        /* 66 */ 0,
        /* 67 */ 0,
        /* 68 */ 0,
        /* 69 */ 0,
        /* 70 */ "ONLINE_WIN_CAR",
        /* 71 */ "TRAFFIC_D_LICENSE",
        /* 72 */ "TRAFFIC_C_LICENSE",
        /* 73 */ "TRAFFIC_B_LICENSE",
        /* 74 */ "TRAFFIC_A_LICENSE",
        /* 75 */ "TRAFFIC_BURN_LICENSE",
        /* 76 */ "TRAFFIC_ELITE_LICENSE",
    };

    // The "general" (timed-tip) table lives in the MUTABLE data segment (dword_, not off_)
    // and is ALL ZERO in the shipped image -- verified over the whole 0x82FAD000..0x82FAF800
    // window, which is uniformly zero, so this is a genuinely blank table and not a reader
    // that missed the segment. That agrees with BrnTrainingTypes.h, where the timed-tip span
    // is empty (E_TRAINING_TYPE_TIMED_TIP_1 == E_TRAINING_TYPE_TIMED_TIP_END == 128, count 0).
    // ⚠️ A zero in the image is not proof nothing writes it at run time; no writer was found,
    // but the search was not exhaustive. Reproduced as the zero-initialised array the image
    // holds -- ConvertTrainingTypeToStringId only indexes it for types >= 128, and the caller
    // NULL-checks, so an unwritten entry is a no-ticker, exactly as on the console.
    static const char* const KAC_GENERAL_TRAINING_TEXT[128] = { 0 };

    // ------------------------------------------------------------------------
    // ConvertTrainingTypeToStringId @0x823AA3B8
    //
    // Maps a training-tip enum to its GUI string-ID. Indices 0..76 select from
    // the "specific" (untimed) table; indices 128..255 select from the
    // "general" (timed-tip) table at offset (index - 128). The gap 77..127 and
    // the unused specific slots return the error sentinel.
    // ------------------------------------------------------------------------
    const char* ConvertTrainingTypeToStringId(BrnProgression::ETrainingType leTrainingType)
    {
        const int liType = static_cast<int>(leTrainingType);

        CGS_ASSERT_AT(liType >= 0 && liType < BrnProgression::E_TRAINING_TYPE_COUNT,
                   "leTrainingType >= 0 && leTrainingType < BrnProgression::E_TRAINING_TYPE_COUNT", CGS_ARTIST_FILE, 160);

        if (liType < 128)
        {
            CGS_ASSERT(liType < 77, "leTrainingType < ciNumSpecificTrainingStringIDs");

            if (liType > 76)
            {
                return "ERROR - UNKNOWN TRAINING TYPE";
            }
            return KAC_SPECIFIC_TRAINING_TEXT[liType];
        }

        const int liIndex = liType - 128;
        if (liIndex >= 128)
        {
            CGS_ASSERT_AT(liIndex < 128, "liIndex < ciNumGeneralTrainingStringIDs", CGS_ARTIST_FILE, 169);
            return "ERROR - UNKNOWN TRAINING TYPE";
        }
        return KAC_GENERAL_TRAINING_TEXT[liIndex];
    }
} // namespace BrnGame

// FOLDED FROM GameBridgeGameStateToX_wN1_01.cpp
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_wN1_01.cpp
//
// BrnGame::BrnGameModule::BridgeGameStateToNetwork -- the game-state -> network bridge,
// partfile of GameBridgeGameStateToX.cpp (network wave N1).
//
// Called by DoUpdate_NetworkPostSim with the network module's post-sim INPUT buffer
// write-locked and the game-state OUTPUT buffer read-locked. It walks the game-state output's
// game-action queue once and, for every action:
//   1. forwards the actions the network module consumes verbatim (18 ids) into the post-sim
//      buffer's own game-action queue, unchanged (type, payload and size as queued);
//   2. translates the network-bound actions into network IN-events on the post-sim buffer's
//      network event queue (telemetry, freeburn challenge, showtime, payback, road rules,
//      rich presence, gamer card, image decode, offline progression, ...);
// then appends the game state's takedown output queue and its GameStateToNetworkInterface into
// the post-sim buffer.
//
// The action switch is three console jump tables (every target dumped from the image and
// checked): the forward table spans ids 0..238, the translate switch is split at 158 (ids
// 1..157 in one table, 158 tested on its own, 159..299 in the other). No two translated ids
// share a target, so no case bodies are collapsed. Game action 275 (road-rules batch query) is
// NOT handled here: it falls to the default of the 159..299 table.
//
// Every network IN-event is queued with sizeof(event) on the host. Three records carry a
// pointer and are wider on the host than on the console (RoadRulesChallengeScoresAction /
// NetworkInRoadRulesDataEvent: console 16 bytes; DXTDecodeImageAction /
// NetworkInDxtDecodeImageEvent: console 20 bytes), so they are copied by member, never by the
// console byte count.
//
// Five of the translated actions (18, 19, 20, 21, 299 -> network IN-events 56, 57, 45, 46, 55)
// are console-only records with no reference names; their records carry FLAG names in
// BrnGameActions.h / BrnNetworkInEventTypeDefs.h, the layouts are the console's.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameSource/GameState/BrnGameActions.h"                   // the GameStateModuleIO action records + EGameActionType
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer (action queue, takedown queue, network interface)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"       // GameStateModuleIO::GameStats (action 180 payload)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // BrnGameState::GameModeParams (action 23)
#include "GameSource/Network/BrnNetworkModuleIO.h"                  // PostSimulationInputBuffer, NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"           // the NetworkIn* event records
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // TelemetryData, ETelemetryHook
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<N,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsIDConvertToString, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Core/CgsStringUtils.h"             // CgsCore::StrCpy
#include "SharedClasses/StreetData/BrnChallengeData.h"              // BrnStreetData::ScoreType, ChallengeData

#include <cstring>                                                   // std::memcpy (the console's XMemCpy block copies)

namespace BrnGame
{
    void BrnGameModule::BridgeGameStateToNetwork(
        BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer* lpNetworkInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer*       lpGameStateOutput)
    {
        namespace GsmIO = BrnGameState::GameStateModuleIO;
        namespace NetIO = BrnNetwork::BrnNetworkModuleIO;

        const GsmIO::GameActionQueue* lpInQueue  = lpGameStateOutput->GetGameActionQueue();
        GsmIO::GameActionQueue*       lpOutQueue = lpNetworkInput->GetGameActionQueue();
        CGS_ASSERT_AT(lpInQueue, "lpInQueue", CGS_ARTIST_FILE, 4284);
        CGS_ASSERT_AT(lpOutQueue, "lpOutQueue", CGS_ARTIST_FILE, 4285);

        const CgsModule::Event* lpAction = 0;
        s32 liActionSize = 0;
        s32 liActionType = lpInQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction)
        {
            // ---- 1. actions the network module reads straight off its own game-action queue ----
            switch (liActionType)
            {
                case GsmIO::E_ACTION_RESET_PLAYER_CAR:
                case GsmIO::E_ACTION_PREPARE_FOR_MODE:
                case GsmIO::E_ACTION_START_MODE_INTRO:
                case GsmIO::E_ACTION_STOP_MODE_INTRO:
                case GsmIO::E_ACTION_MARKED_MAN_LOADED:
                case GsmIO::E_ACTION_START_PLAYING_MODE:
                case GsmIO::E_ACTION_FINISHED_MODE:
                case GsmIO::E_ACTION_SHOW_MODE_RESULTS:
                case GsmIO::E_ACTION_STOP_MODE:
                case GsmIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE:
                case GsmIO::E_ACTION_CAR_SELECT_ONLINE_SELECT_CAR:
                case GsmIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE:
                case GsmIO::E_ACTION_PERFORM_INVITE:
                case GsmIO::E_ACTION_PLAYER_ELIMINATED:
                case GsmIO::E_ACTION_ACHIEVEMENTS_EARNED:
                case GsmIO::E_ACTION_ONLINE_GAME_RESULT:
                case GsmIO::E_ACTION_ONLINE_ROUND_RESULT:
                case GsmIO::E_ACTION_NETWORK_CAUGHT_FEVER:
                    lpOutQueue->AddEvent(lpAction, liActionType, liActionSize);
                    break;

                default:
                    break;
            }

            // ---- 2. actions translated into network IN-events ----------------------------------
            switch (liActionType)
            {
                case GsmIO::E_ACTION_GUI_UPDATE_PLAYER_CAR_ID:
                {
                    const GsmIO::GuiUpdatePlayerCarIDAction* lpCarUpdateAction =
                        reinterpret_cast<const GsmIO::GuiUpdatePlayerCarIDAction*>(lpAction);

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EVENT_CAR_MODEL);
                    char lacCarID[KI_CGSID_STRING_LEN];
                    CgsIDConvertToString(lpCarUpdateAction->mPlayerCarID, lacCarID);
                    lTeleEvent.mEventData.AddParameter(lacCarID);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PREPARE_FOR_MODE:
                {
                    const GsmIO::PrepareForModeAction* lpPrepAction =
                        reinterpret_cast<const GsmIO::PrepareForModeAction*>(lpAction);

                    if (lpPrepAction->IsFirstPrepareForMode())
                    {
                        const BrnGameState::GameModeParams* lpGameParams = lpPrepAction->GetGameModeParams();
                        const s32  liGameModeType = static_cast<s32>(lpGameParams->GetGameModeType());
                        const bool lbIsOnline     = lpGameParams->mbIsOnline;

                        NetIO::NetworkInTelemetryEvent lTeleEvent;
                        if (lbIsOnline)
                        {
                            lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EVENT_STARTED_ONLINE);
                            lTeleEvent.mEventData.AddParameter(liGameModeType);
                            lTeleEvent.mEventData.AddParameter(static_cast<s32>(lpGameParams->miNumNetworkPlayers));
                        }
                        else
                        {
                            lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EVENT_STARTED);
                            lTeleEvent.mEventData.AddParameter(liGameModeType);
                            lTeleEvent.mEventData.AddParameter(lpGameParams->mfProgressionRankAsRatio);
                        }
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTeleEvent, lTeleEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_FINISHED_MODE:
                {
                    const GsmIO::FinishedModeAction* lpModeResultsAction =
                        reinterpret_cast<const GsmIO::FinishedModeAction*>(lpAction);
                    CGS_ASSERT(lpModeResultsAction, "lpModeResultsAction");

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    if (lpModeResultsAction->mbTimedOut)
                    {
                        lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_GAME_FINISH_TIMEOUT);
                        lTeleEvent.mEventData.AddParameter(static_cast<s32>(lpModeResultsAction->mfDistanceFromFinish));
                    }
                    else
                    {
                        lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_GAME_FINISH_TIME);
                        lTeleEvent.mEventData.AddParameter(lpModeResultsAction->mFinishTime);
                    }
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_STOP_MODE:
                {
                    const GsmIO::StopModeAction* lpStopAction =
                        reinterpret_cast<const GsmIO::StopModeAction*>(lpAction);

                    // +0x13 is the abort-due-to-disconnects flag, +0x11 the has-aborted flag
                    // (reference member order of StopModeAction).
                    BrnNetwork::ETelemetryHook leHook;
                    if (lpStopAction->mu8Field13)
                    {
                        leHook = BrnNetwork::E_TELEMETRY_EVENT_DISCONNECT;
                    }
                    else if (lpStopAction->mu8Field11)
                    {
                        leHook = BrnNetwork::E_TELEMETRY_EVENT_QUIT;
                    }
                    else
                    {
                        leHook = BrnNetwork::E_TELEMETRY_EVENT_FINISHED;
                    }

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    lTeleEvent.mEventData.Construct(leHook);
                    lTeleEvent.mEventData.AddParameter(static_cast<s32>(lpStopAction->meGameModeType));
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_BODY_SHOP_DRIVE_THRU:
                {
                    NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                    lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_BODY_SHOP);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAINT_SHOP_DRIVE_THRU:
                {
                    NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                    lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_PAINT_SHOP);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_DRIVE_THRU_JUNK_YARD:
                {
                    const GsmIO::DriveThruJunkYardAction* lpJunkYardAction =
                        reinterpret_cast<const GsmIO::DriveThruJunkYardAction*>(lpAction);
                    if (lpJunkYardAction->mbIsInJunkYard)
                    {
                        NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                        lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_JUNK_YARD);
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_GAS_STATION_DRIVE_THRU:
                {
                    NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                    lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_GAS_STATION);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_WORLD_REGION_CHANGE:
                {
                    const GsmIO::WorldRegionChangeAction* lpWorldRegionChangeAction =
                        reinterpret_cast<const GsmIO::WorldRegionChangeAction*>(lpAction);

                    NetIO::NetworkInChangeDistrictEvent lChangeDistrict;
                    lChangeDistrict.mNewWorldRegion = lpWorldRegionChangeAction->mNewWorldRegion;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lChangeDistrict, lChangeDistrict.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_RACE_CAR_REACHED_CHECKPOINT:
                {
                    const GsmIO::RaceCarReachedCheckpointAction* lpCheckpointAction =
                        reinterpret_cast<const GsmIO::RaceCarReachedCheckpointAction*>(lpAction);
                    CGS_ASSERT_AT(lpCheckpointAction, "lpCheckpointAction", CGS_ARTIST_FILE, 4880);

                    if (lpCheckpointAction->mbIsLocalPlayer)
                    {
                        NetIO::NetworkInLocalPlayerReachesCheckpoint lCheckpointEvent;
                        lCheckpointEvent.miCheckpointIndex = lpCheckpointAction->miCheckPointIndex;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lCheckpointEvent, lCheckpointEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_SHOWTIME_UPDATE:
                {
                    const GsmIO::ShowtimeUpdateAction* lpShowtimeUpdateAction =
                        reinterpret_cast<const GsmIO::ShowtimeUpdateAction*>(lpAction);
                    CGS_ASSERT_AT(lpShowtimeUpdateAction, "lpShowtimeUpdateAction", CGS_ARTIST_FILE, 3976);

                    NetIO::NetworkInShowtimeUpdateEvent lNetworkShowtimeUpdateEvent;
                    lNetworkShowtimeUpdateEvent.mPlayerID       = lpShowtimeUpdateAction->mNetworkPlayerID;
                    lNetworkShowtimeUpdateEvent.miShowtimeScore = lpShowtimeUpdateAction->miShowtimeScore;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkShowtimeUpdateEvent,
                                                                     lNetworkShowtimeUpdateEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SHOWTIME_MODE_SWITCH:
                {
                    const GsmIO::ShowtimeModeSwitchAction* lpShowtimeModeSwitchAction =
                        reinterpret_cast<const GsmIO::ShowtimeModeSwitchAction*>(lpAction);
                    CGS_ASSERT_AT(lpShowtimeModeSwitchAction, "lpShowtimeModeSwitchAction", CGS_ARTIST_FILE, 3992);

                    NetIO::NetworkInShowtimeSwitchEvent lNetworkShowtimeModeSwitchEvent;
                    lNetworkShowtimeModeSwitchEvent.mPlayerID             = lpShowtimeModeSwitchAction->mNetworkPlayerID;
                    lNetworkShowtimeModeSwitchEvent.miFinalShowtimeScore = lpShowtimeModeSwitchAction->miFinalShowtimeScore;
                    lNetworkShowtimeModeSwitchEvent.mbEnteringShowtime   = lpShowtimeModeSwitchAction->mbEnteringShowtime;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkShowtimeModeSwitchEvent,
                                                                     lNetworkShowtimeModeSwitchEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE:
                {
                    const GsmIO::FreeburnChallengeAction* lpChallengeAction =
                        reinterpret_cast<const GsmIO::FreeburnChallengeAction*>(lpAction);
                    CGS_ASSERT(lpChallengeAction, "lpChallengeAction");

                    if (!lpChallengeAction->mbAbortingToStartNewChallenge &&
                        lpChallengeAction->meEventType != NetIO::E_CHALLENGE_EVENT_RESULTS_FINISHED)
                    {
                        // mPlayerID is left unset: the network side stamps it.
                        NetIO::NetworkInFreeburnChallengeEvent lChallengeEvent;
                        lChallengeEvent.mChallengeID                   = lpChallengeAction->mChallengeID;
                        lChallengeEvent.meEventType                    =
                            static_cast<NetIO::EChallengeEventType>(lpChallengeAction->meEventType);
                        lChallengeEvent.meChallengeStatus              = lpChallengeAction->meChallengeStatus;
                        lChallengeEvent.miActionIndex                  = lpChallengeAction->miActionIndex;
                        lChallengeEvent.miNumberOfCompletedChallenges  = lpChallengeAction->miNumChallengesComplete;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lChallengeEvent, lChallengeEvent.GetEventType());
                    }

                    if (lpChallengeAction->meEventType == NetIO::E_CHALLENGE_EVENT_TRIGGERED)
                    {
                        NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                        lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_NETWORK_CHALLENGE_STARTED);
                        lTelemeteryEvent.mEventData.AddParameter(lpChallengeAction->mChallengeID);
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    }
                    else if (lpChallengeAction->meEventType == NetIO::E_CHALLENGE_EVENT_ENDED)
                    {
                        NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                        lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_NETWORK_CHALLENGE_FINISHED);
                        lTelemeteryEvent.mEventData.AddParameter(lpChallengeAction->mChallengeID);
                        lTelemeteryEvent.mEventData.AddParameter(static_cast<s32>(lpChallengeAction->meChallengeStatus));
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS:
                {
                    const GsmIO::FburnChallengeStatusAction* lpFburnChallengeStatusAction =
                        reinterpret_cast<const GsmIO::FburnChallengeStatusAction*>(lpAction);
                    CGS_ASSERT(lpFburnChallengeStatusAction, "lpFburnChallengeStatusAction");

                    NetIO::NetworkInFburnChallengeStatusEvent lFburnChallengeStatusEvent;
                    lFburnChallengeStatusEvent.mCompletedChallenges = lpFburnChallengeStatusAction->mCompletedChallenges;
                    lFburnChallengeStatusEvent.mPlayerID            = lpFburnChallengeStatusAction->mPlayerID;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lFburnChallengeStatusEvent,
                                                                     lFburnChallengeStatusEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE:
                {
                    const GsmIO::FburnChallengeSuccessUpdateAction* lpFburnChallengeSuccessUpdateAction =
                        reinterpret_cast<const GsmIO::FburnChallengeSuccessUpdateAction*>(lpAction);
                    CGS_ASSERT(lpFburnChallengeSuccessUpdateAction, "lpFburnChallengeSuccessUpdateAction");

                    NetIO::NetworkInFburnSuccessUpdateEvent lFburnChallengeSuccessUpdateEvent;
                    lFburnChallengeSuccessUpdateEvent.mChallengeSuccessUpdate =
                        lpFburnChallengeSuccessUpdateAction->mChallengeSuccessUpdate;
                    lFburnChallengeSuccessUpdateEvent.miActionIndex = lpFburnChallengeSuccessUpdateAction->miActionIndex;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lFburnChallengeSuccessUpdateEvent,
                                                                     lFburnChallengeSuccessUpdateEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS:
                {
                    const GsmIO::FburnChallengeSuccessAction* lpFburnChallengeSuccessAction =
                        reinterpret_cast<const GsmIO::FburnChallengeSuccessAction*>(lpAction);
                    CGS_ASSERT(lpFburnChallengeSuccessAction, "lpFburnChallengeSuccessAction");

                    NetIO::NetworkInFburnChallengeSuccessEvent lFburnChallengeSuccessEvent;
                    std::memcpy(lFburnChallengeSuccessEvent.mafActionScores,
                                lpFburnChallengeSuccessAction->mafActionScores,
                                sizeof(lFburnChallengeSuccessEvent.mafActionScores));
                    std::memcpy(lFburnChallengeSuccessEvent.mabSuccessfulActions,
                                lpFburnChallengeSuccessAction->mabSuccessfulActions,
                                sizeof(lFburnChallengeSuccessEvent.mabSuccessfulActions));
                    std::memcpy(lFburnChallengeSuccessEvent.mabAccumulationThisFrame,
                                lpFburnChallengeSuccessAction->mabAccumulationThisFrame,
                                sizeof(lFburnChallengeSuccessEvent.mabAccumulationThisFrame));
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lFburnChallengeSuccessEvent,
                                                                     lFburnChallengeSuccessEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ACTIVE_FREEBURN_CHALLENGE:
                {
                    const GsmIO::ActiveFburnChallengeAction* lpActiveFburnChallengeAction =
                        reinterpret_cast<const GsmIO::ActiveFburnChallengeAction*>(lpAction);
                    CGS_ASSERT(lpActiveFburnChallengeAction, "lpActiveFburnChallengeAction");

                    NetIO::NetworkInActiveFburnChallengeEvent lActiveFburnChallengeEvent;
                    lActiveFburnChallengeEvent.mChallengeID             = lpActiveFburnChallengeAction->mChallengeID;
                    lActiveFburnChallengeEvent.mPlayerToSendToID        = lpActiveFburnChallengeAction->mPlayerToSendToID;
                    lActiveFburnChallengeEvent.miNumPlayersInChallenge  = lpActiveFburnChallengeAction->miNumPlayersInChallenge;
                    std::memcpy(lActiveFburnChallengeEvent.maePlayersInChallengeARCI,
                                lpActiveFburnChallengeAction->maePlayersInChallengeARCI,
                                sizeof(lActiveFburnChallengeEvent.maePlayersInChallengeARCI));
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lActiveFburnChallengeEvent,
                                                                     lActiveFburnChallengeEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SWITCH_BURNING_HOME_RUN_RUNNER:
                {
                    const GsmIO::SwitchBurningHomeRunRunnerAction* lpSwitchAction =
                        reinterpret_cast<const GsmIO::SwitchBurningHomeRunRunnerAction*>(lpAction);
                    CGS_ASSERT(lpSwitchAction, "lpSwitchAction");

                    NetIO::NetworkInSwitchBurningHomeRunRunner lNetworkSwitchEvent;
                    lNetworkSwitchEvent.mNewRunnerID = lpSwitchAction->mNewRunnerPlayerID;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkSwitchEvent, lNetworkSwitchEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_BURNOUT_SKILLZ:
                {
                    const GsmIO::BurnoutSkillzAction* lpGameStateSkillzAction =
                        reinterpret_cast<const GsmIO::BurnoutSkillzAction*>(lpAction);
                    CGS_ASSERT(lpGameStateSkillzAction, "lpGameStateSkillzAction");

                    NetIO::NetworkInBurnoutSkillzEvent lNetworkSkillzEvent;
                    lNetworkSkillzEvent.mPlayerID      = lpGameStateSkillzAction->mNetworkPlayerID;
                    lNetworkSkillzEvent.mNewSkillzData = lpGameStateSkillzAction->mBurnoutSkillzData;

                    const s32 liActionFlags = lpGameStateSkillzAction->miActionFlags;
                    if ((liActionFlags & GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_SINGLE_PLAYER) ==
                        GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_SINGLE_PLAYER)
                    {
                        lNetworkSkillzEvent.meEventType =
                            NetIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_A_SPECIFIC_PLAYER;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkSkillzEvent, lNetworkSkillzEvent.GetEventType());
                    }
                    else if ((liActionFlags & GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_ALL_PLAYERS) ==
                             GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_ALL_PLAYERS)
                    {
                        lNetworkSkillzEvent.meEventType =
                            NetIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_ALL_PLAYERS;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkSkillzEvent, lNetworkSkillzEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_GAME_STATS_RESPONSE:
                {
                    // The action's payload is the GameStats record itself (see the enumerator's
                    // note in BrnGameActions.h).
                    typedef GsmIO::GameStats GS;
                    const GS* lpGameStats = reinterpret_cast<const GS*>(lpAction);

                    NetIO::NetworkInOfflineProgression lNetworkProgressionEvent;
                    BrnNetwork::ServerGeneratedTypes::OfflineProgressionT& lrProgression =
                        lNetworkProgressionEvent.mOfflineProgression;
                    lrProgression.miTotalTime          = lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_TIME_PLAYED);
                    lrProgression.miTotalDistance      = lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE);
                    lrProgression.miTakedowns          = lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_TAKEDOWNS);
                    lrProgression.mi16Jumps            = static_cast<s16>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_JUMPS));
                    lrProgression.mi16Smashes          = static_cast<s16>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_SMASHES));
                    lrProgression.mi16Stunts           = static_cast<s16>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_STUNTS));
                    lrProgression.mi8CarsCollected     = static_cast<s8>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_CARS_COLLECTED));
                    lrProgression.mi8TimeRoadRulesWon  = static_cast<s8>(lpGameStats->GetRoadsRuledCount(BrnStreetData::E_SCORE_TYPE_TIME));
                    lrProgression.mi8CrashRoadRulesWon = static_cast<s8>(lpGameStats->GetRoadsRuledCount(BrnStreetData::E_SCORE_TYPE_CRASH));
                    lrProgression.mi8AchievementsEarnt = static_cast<s8>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_ACHIEVEMENTS));
                    CgsIDConvertToString(lpGameStats->GetValue(GS::E_ID_VALUE_TYPE_FAVOURITE_CAR), lrProgression.macFavouriteCar);
                    CgsIDConvertToString(lpGameStats->GetValue(GS::E_ID_VALUE_TYPE_FORGOTTEN_CAR), lrProgression.macForgottenCar);
                    CgsIDConvertToString(lpGameStats->GetValue(GS::E_ID_VALUE_TYPE_NEMESIS), lrProgression.macNemesis);
                    lNetworkProgressionEvent.miFreeburnChallengeSuccessCount =
                        lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_FREEBURN_CHALLENGES_COMPLETE);

                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkProgressionEvent,
                                                                     lNetworkProgressionEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAYBACK_MUGSHOT:
                {
                    const GsmIO::PaybackMugshotAction* lpPaybackMugshotAction =
                        reinterpret_cast<const GsmIO::PaybackMugshotAction*>(lpAction);
                    CGS_ASSERT(lpPaybackMugshotAction, "lpPaybackMugshotAction");

                    NetIO::NetworkInPaybackMugshotEvent lPaybackMugshotEvent;
                    lPaybackMugshotEvent.meTakedownAggressorIndex         = lpPaybackMugshotAction->meTakedownAggressorIndex;
                    lPaybackMugshotEvent.meTakedownVictimIndex            = lpPaybackMugshotAction->meTakedownVictimIndex;
                    lPaybackMugshotEvent.meMugshotResponse                = lpPaybackMugshotAction->meMugshotResponse;
                    lPaybackMugshotEvent.meMugshotType                    = lpPaybackMugshotAction->meImageType;
                    lPaybackMugshotEvent.mRoadRuleBeatenRoadID            = lpPaybackMugshotAction->mRoadRuleBeatenRoadID;
                    lPaybackMugshotEvent.mbIsTakedownAggressorLocalPlayer = lpPaybackMugshotAction->mbIsTakedownAggressorLocalPlayer;
                    lPaybackMugshotEvent.mbMugshotRequiresBroadcast       = lpPaybackMugshotAction->mbMugshotRequiresBroadcast;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lPaybackMugshotEvent, lPaybackMugshotEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ABORT_MUGSHOT_CAPTURE:
                {
                    // An empty signal event; its single byte is never written.
                    NetIO::NetworkInAbortMugshotCaptureEvent lAbortCaptureEvent;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lAbortCaptureEvent, lAbortCaptureEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAYBACK_ACTIVATED:
                {
                    const GsmIO::PaybackActivatedAction* lpPaybackAction =
                        reinterpret_cast<const GsmIO::PaybackActivatedAction*>(lpAction);
                    CGS_ASSERT(lpPaybackAction, "lpPaybackAction");

                    NetIO::NetworkInPaybackIntialised lNetworkPaybackEvent;
                    lNetworkPaybackEvent.mePaybackAggressorIndex = lpPaybackAction->mePaybackAggressorIndex;
                    lNetworkPaybackEvent.mePaybackVictimIndex    = lpPaybackAction->mePaybackVictimIndex;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNetworkPaybackEvent, lNetworkPaybackEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAYBACK_SUCCEEDED:
                {
                    CGS_ASSERT(lpAction, "lpAction");
                    const GsmIO::PaybackSucceededAction* lpPaybackAction =
                        reinterpret_cast<const GsmIO::PaybackSucceededAction*>(lpAction);

                    NetIO::NetworkInPaybackSucceeded lPaybackSucceededEvent;
                    lPaybackSucceededEvent.mePaybackAggressorIndex = lpPaybackAction->mePaybackAggressorIndex;
                    lPaybackSucceededEvent.mePaybackVictimIndex    = lpPaybackAction->mePaybackVictimIndex;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lPaybackSucceededEvent, lPaybackSucceededEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_UPDATE_RICH_PRESENCE:
                {
                    const GsmIO::UpdateRichPresence* lpRichPresenceAction =
                        reinterpret_cast<const GsmIO::UpdateRichPresence*>(lpAction);

                    NetIO::NetworkInUpdateRichPresence lEvent;
                    CgsCore::StrCpy(lEvent.macRichPresenceString,
                                    static_cast<u32>(sizeof(lEvent.macRichPresenceString)),
                                    lpRichPresenceAction->macNewPresenceData);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lEvent, lEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SEND_TELEMETRY:
                {
                    const GsmIO::TelemetryAction* lpTeleAction =
                        reinterpret_cast<const GsmIO::TelemetryAction*>(lpAction);

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    lTeleEvent.mEventData = lpTeleAction->mEventData;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ROAD_RULES_CHALLENGE_SCORES:
                {
                    const GsmIO::RoadRulesChallengeScoresAction* lpRoadRulesScoresAction =
                        reinterpret_cast<const GsmIO::RoadRulesChallengeScoresAction*>(lpAction);

                    const u32 luTimestamp = lpRoadRulesScoresAction->GetTimeStampOfLastRoadRulesDownload();
                    NetIO::NetworkInRoadRulesDataEvent lRoadRulesScoresEvent;
                    lRoadRulesScoresEvent.Construct(lpRoadRulesScoresAction->GetRoadRulesID(),
                                                    luTimestamp,
                                                    lpRoadRulesScoresAction->GetStreetManager());
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lRoadRulesScoresEvent, lRoadRulesScoresEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ROAD_RULES_PERSONAL_BEST:
                {
                    const GsmIO::RoadRulesPersonalBestAction* lpRoadRulesPersonalBestAction =
                        reinterpret_cast<const GsmIO::RoadRulesPersonalBestAction*>(lpAction);

                    NetIO::NetworkInRoadRulesPBEvent lRoadRulesPersonalBestEvent;
                    lRoadRulesPersonalBestEvent.mPersonalBestScore.Copy(&lpRoadRulesPersonalBestAction->mPersonalBestScore);
                    lRoadRulesPersonalBestEvent.mChallengeIndex      = lpRoadRulesPersonalBestAction->mChallengeIndex;
                    lRoadRulesPersonalBestEvent.mbLobbyPersonalBest  = lpRoadRulesPersonalBestAction->mbLobbyPersonalBest;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lRoadRulesPersonalBestEvent,
                                                                     lRoadRulesPersonalBestEvent.GetEventType());

                    // One telemetry record per score type the new personal best carries.
                    for (BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                         leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                         leScoreType++)
                    {
                        if (!lRoadRulesPersonalBestEvent.mPersonalBestScore.ContainsData(leScoreType))
                        {
                            continue;
                        }

                        NetIO::NetworkInTelemetryEvent lTelemetryEvent;
                        switch (leScoreType)
                        {
                            case BrnStreetData::E_SCORE_TYPE_TIME:
                                lTelemetryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_ROAD_RULES_NEW_PB_TIME);
                                break;
                            case BrnStreetData::E_SCORE_TYPE_CRASH:
                                lTelemetryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_ROAD_RULES_NEW_PB_CRASH);
                                break;
                            default:
                                CGS_FAIL("Telemetry can't report this unknown road rules type");
                                break;
                        }
                        lTelemetryEvent.mEventData.AddParameter(static_cast<s32>(lpRoadRulesPersonalBestAction->mChallengeIndex));
                        lTelemetryEvent.mEventData.AddParameter(
                            lRoadRulesPersonalBestEvent.mPersonalBestScore.GetScore(leScoreType));
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(lTelemetryEvent, lTelemetryEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_ROAD_RULES_OVERWRITE_SERVER_RECORD:
                {
                    // An empty signal event; its single byte is never written.
                    NetIO::NetworkInRoadRulesOverwriteServerRecord lOverwriteServerRoadRulesEvent;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lOverwriteServerRoadRulesEvent,
                                                                     lOverwriteServerRoadRulesEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_REQUEST_GAMERCARD:
                {
                    const GsmIO::RequestGamerCardAction* lpGamerCardAction =
                        reinterpret_cast<const GsmIO::RequestGamerCardAction*>(lpAction);
                    CGS_ASSERT(lpGamerCardAction, "lpGamerCardAction");

                    NetIO::NetworkInShowGamerCard lShowGamerCardEvent;
                    lShowGamerCardEvent.mPlayerName = lpGamerCardAction->mPlayerName;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lShowGamerCardEvent, lShowGamerCardEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_DXT_DECODE_IMAGE:
                {
                    const GsmIO::DXTDecodeImageAction* lpDxtDecodeImageAction =
                        reinterpret_cast<const GsmIO::DXTDecodeImageAction*>(lpAction);
                    CGS_ASSERT(lpDxtDecodeImageAction, "lpDxtDecodeImageAction");

                    NetIO::NetworkInDxtDecodeImageEvent lDxtDecodeImageEvent;
                    lDxtDecodeImageEvent.mpTextureToDecode = lpDxtDecodeImageAction->mpTextureToDecode;
                    lDxtDecodeImageEvent.mPlayerName       = lpDxtDecodeImageAction->mPlayerName;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lDxtDecodeImageEvent, lDxtDecodeImageEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_EVENT_SCORE_TO_UPLOAD:
                {
                    const GsmIO::ModeScoreLeaderboardAction* lpModeScoreLeaderboardAction =
                        reinterpret_cast<const GsmIO::ModeScoreLeaderboardAction*>(lpAction);
                    CGS_ASSERT_AT(lpModeScoreLeaderboardAction, "lpModeScoreLeaderboardAction", CGS_ARTIST_FILE, 4939);

                    NetIO::NetworkInScoreLeaderboardEvent lScoreLeaderboardEvent;
                    lScoreLeaderboardEvent.mEventID       = lpModeScoreLeaderboardAction->mEventID;
                    lScoreLeaderboardEvent.meGameModeType = static_cast<s32>(lpModeScoreLeaderboardAction->meGameModeType);
                    lScoreLeaderboardEvent.miScore        = lpModeScoreLeaderboardAction->miScore;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lScoreLeaderboardEvent,
                                                                     lScoreLeaderboardEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_NON_UPLOADED_MODE_SCORES:
                {
                    const GsmIO::NonUploadedModeScoresAction* lpNonUploadedModeScoresAction =
                        reinterpret_cast<const GsmIO::NonUploadedModeScoresAction*>(lpAction);
                    CGS_ASSERT_AT(lpNonUploadedModeScoresAction, "lpNonUploadedModeScoresAction", CGS_ARTIST_FILE, 4959);

                    NetIO::NetworkInNonUploadedScoresEvent lNonUploadedScoresEvent;
                    lNonUploadedScoresEvent.mpNonUploadedScores = lpNonUploadedModeScoresAction->mpNonUploadedScores;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lNonUploadedScoresEvent,
                                                                     lNonUploadedScoresEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_STUNT_SCORE_UPDATED:
                {
                    // No assert on this arm.
                    const GsmIO::StuntScoreUpdatedAction* lpStuntScoreAction =
                        reinterpret_cast<const GsmIO::StuntScoreUpdatedAction*>(lpAction);

                    NetIO::NetworkInStuntScoreUpdatedEvent lStuntScoreUpdatedEvent;
                    lStuntScoreUpdatedEvent.miStuntScore = lpStuntScoreAction->miStuntScore;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lStuntScoreUpdatedEvent,
                                                                     lStuntScoreUpdatedEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_STUNT_MULTIPLIER:
                {
                    const GsmIO::StuntMultiplierAction* lpStuntMultiplierAction =
                        reinterpret_cast<const GsmIO::StuntMultiplierAction*>(lpAction);
                    CGS_ASSERT(lpStuntMultiplierAction, "lpStuntMultiplierAction");

                    // One 8-byte copy on the console.
                    NetIO::NetworkInStuntMultiplierEvent lStuntMultiplierEvent;
                    lStuntMultiplierEvent.muStuntTypes    = lpStuntMultiplierAction->muStuntTypes;
                    lStuntMultiplierEvent.mu16FlatSpins   = lpStuntMultiplierAction->mu16FlatSpins;
                    lStuntMultiplierEvent.mu16BarrelRolls = lpStuntMultiplierAction->mu16BarrelRolls;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lStuntMultiplierEvent,
                                                                     lStuntMultiplierEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SHOW_GAMERCARD_FOR_XUID:
                {
                    const GsmIO::ShowGamerCardForXuidAction* lpGamerCardAction =
                        reinterpret_cast<const GsmIO::ShowGamerCardForXuidAction*>(lpAction);
                    CGS_ASSERT(lpGamerCardAction, "lpGamerCardAction");

                    NetIO::NetworkInShowGamerCardForXuidEvent lShowGamerCardEvent;
                    lShowGamerCardEvent.mu64Xuid = lpGamerCardAction->mu64Xuid;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(lShowGamerCardEvent, lShowGamerCardEvent.GetEventType());
                    break;
                }

                default:
                    break;
            }

            liActionType = lpInQueue->GetNextEvent(lpAction, &lpAction, &liActionSize);
        }

        // The output accessor hands the takedown queue out as the forward-declared
        // TakedownEventOutputQueueType; the payload behind it is the committed
        // EventQueue<BrnGameState::TakedownEvent, 8> that AppendTakedownQueue takes (the same
        // cross-home cast BridgeGameStateToWorld carries).
        lpNetworkInput->AppendTakedownQueue(
            reinterpret_cast<const NetIO::PostSimulationInputBuffer::TakedownEventQueue*>(
                lpGameStateOutput->GetTakedownEventOutputQueue()));
        lpNetworkInput->AppendGameStateToNetworkInterface(lpGameStateOutput->GetGameStateToNetworkInterface());
    }
}

#include "GameSource/Game/GameBridgeGameStateToX.h"                // PushGuiEvent<T>
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"               // the posted GUI records
#include "GameSource/GameState/BrnGameActions.h"                    // action ids + DriveThruJunkYardAction
#include "SharedClasses/Trigger/BrnTriggerBase.h"                   // TriggerRegion::E_TYPE_LANDMARK
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsID
#include <cstring>                                                  // memcpy

namespace BrnGame
{
namespace
{
    // The drive-thru "discovered" action (104, 12 bytes). Its producer,
    // DriveThruManager::Update, writes the three words as the drive-thru type, the number of
    // drive-thrus of that type, and how many of them the profile has discovered.
    struct DriveThruDiscoveredActionMirror
    {
        s32 meDriveThruType;                   // +0x00  BrnTrigger::GenericRegion::Type
        s32 miTotalDriveThrusOfThisType;       // +0x04
        s32 miNumDriveThrusOfThisTypeFound;    // +0x08
    };
    CGS_STATIC_ASSERT((sizeof(DriveThruDiscoveredActionMirror) == 12), "action 104 is posted with size 12");

    // The player-trigger action (109, 24 bytes), one per trigger region the player car is in,
    // every frame. Posted by TriggerQueryManager::PreWorldUpdate's fan-out.
    struct PlayerTriggerActionMirror
    {
        CgsID mId;                             // +0x00  TriggerRegion::GetId()
        s32   miRegionType;                    // +0x08  TriggerRegion::Type
        s32   miGenericType;                   // +0x0C  GenericRegion::Type (generic regions only)
        s32   miRegionIndex;                   // +0x10  the region's index in the trigger data
        u8    mbFirstFrame;                    // +0x14
        u8    mauPad[3];                       // +0x15
    };
    CGS_STATIC_ASSERT((sizeof(PlayerTriggerActionMirror) == 24), "action 109 is posted with size 24");

    // id 315 size 1 -- AddGuiEvent<CgsGui::GuiEvent<315>>. The arm posts a stack byte it never
    // writes; the consumer (HudMessageAnalyzer case 315 -> "EvryDrveThru") reads only the id.
    struct AllDriveThrusFoundWire315
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 315; }
    };
    CGS_STATIC_ASSERT((sizeof(AllDriveThrusFoundWire315) == 1), "id 315 size 1");

    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEnteredJunkyard) == 1),              "id 79 size 1");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventEnterLandmarkArea) == 2),       "id 165 size 2");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventHideDriveThru) == sizeof(CgsID)), "id 201 size 8");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventMiniMapSwitch) == 1),           "id 205 size 1");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventJumpStarted) == sizeof(CgsID)), "id 216 size 8");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventDriveThruDiscovered) == 12),    "id 314 size 12");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventSuperJumpFailed) == 1),         "id 549 size 1");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventCantPaintCar) == 1),            "id 551 size 1");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventReturnDistrict) == 8),          "id 196 size 8");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventAllJunctionsDiscoveredOfType) == 4), "id 313 size 4");

    // id 312 size 1 -- AddGuiEvent<CgsGui::GuiEvent<312>>. The arm posts a stack byte it never
    // writes; the consumer (HudMessageAnalyzer case 312 -> "EvryEventJnc") reads only the id.
    struct AllEventsDiscoveredWire312
    {
        u8 mu8Unused;                          // +0x00  never written by the arm
        s32 GetEventType() const { return 312; }
    };
    CGS_STATIC_ASSERT((sizeof(AllEventsDiscoveredWire312) == 1), "id 312 size 1");
    CGS_STATIC_ASSERT((sizeof(BrnGui::GuiEventMustFixCarFirst) == 1),         "id 552 size 1");

}   // anonymous namespace

bool TranslateFreeRoamGameActionToGuiEvent(
    s32 liActionType,
    const CgsModule::Event* lpAction,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    switch (liActionType)
    {
    // ---- 46  the drive-thru CLOSE (8 bytes) ---------------------------------------------------
    // The closed drive-thru's CgsID (the record's doubleword) straight into GuiEventHideDriveThru
    // (id 201, size 8). GuiCache's case-201 arm hides that drive-thru's map row.
    case 46:
    {
        const CgsID lDriveThruId = *reinterpret_cast<const CgsID*>(lpAction);

        BrnGui::GuiEventHideDriveThru lEvent;
        std::memcpy(lEvent.maData, &lDriveThruId, sizeof(lDriveThruId));
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 57  E_ACTION_SHOW_JUMP_NAME (8 bytes) ------------------------------------------------
    // Null assert on the record, then its doubleword (the jump's stunt-element key) into
    // GuiEventJumpStarted (id 216, size 8).
    case BrnGameState::GameStateModuleIO::E_ACTION_SHOW_JUMP_NAME:
    {
        CGS_ASSERT_AT(lpAction != 0,
            "lpJumpNameAction != NULL", CGS_ARTIST_FILE, 1813);
        const CgsID lJumpKey = *reinterpret_cast<const CgsID*>(lpAction);

        BrnGui::GuiEventJumpStarted lEvent;
        std::memcpy(lEvent.maData, &lJumpKey, sizeof(lJumpKey));
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 99  E_ACTION_DRIVE_THRU_JUNK_YARD (1 byte) -------------------------------------------
    // The in-junkyard byte straight into GuiEnteredJunkyard (id 79, size 1).
    case BrnGameState::GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD:
    {
        const BrnGameState::GameStateModuleIO::DriveThruJunkYardAction* lpJunkYard =
            reinterpret_cast<const BrnGameState::GameStateModuleIO::DriveThruJunkYardAction*>(lpAction);

        BrnGui::GuiEnteredJunkyard lEvent;
        lEvent.maData[0] = lpJunkYard->mbIsInJunkYard ? 1u : 0u;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 104  the drive-thru DISCOVERED (12 bytes) --------------------------------------------
    // The three words straight across into GuiEventDriveThruDiscovered (id 314, size 12).
    case 104:
    {
        const DriveThruDiscoveredActionMirror* lpDiscovered =
            reinterpret_cast<const DriveThruDiscoveredActionMirror*>(lpAction);

        BrnGui::GuiEventDriveThruDiscovered lEvent;
        lEvent.meDriveThruType = lpDiscovered->meDriveThruType;
        lEvent.miNumTotal      = lpDiscovered->miTotalDriveThrusOfThisType;
        lEvent.miNumDiscovered = lpDiscovered->miNumDriveThrusOfThisTypeFound;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 105  ALL drive-thrus found (1 byte, unread) ------------------------------------------
    // No load off the action: the bare CgsGui::GuiEvent<315> tag.
    case 105:
    {
        AllDriveThrusFoundWire315 lEvent;
        lEvent.mu8Unused = 0;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 109  the PLAYER TRIGGER (24 bytes) ---------------------------------------------------
    // Null assert on the record; only a LANDMARK region (type word at +0x08 == 0) posts, and it
    // posts the region index (+0x10) as the halfword of GuiEventEnterLandmarkArea (id 165,
    // size 2). Every other region type is consumed with no post.
    case 109:
    {
        CGS_ASSERT_AT(lpAction != 0,
            "lpTriggerAction != NULL", CGS_ARTIST_FILE, 1931);
        const PlayerTriggerActionMirror* lpTrigger =
            reinterpret_cast<const PlayerTriggerActionMirror*>(lpAction);

        if (lpTrigger->miRegionType == BrnTrigger::TriggerRegion::E_TYPE_LANDMARK)
        {
            const u16 lu16LandmarkIndex = static_cast<u16>(lpTrigger->miRegionIndex);
            BrnGui::GuiEventEnterLandmarkArea lEvent;
            std::memcpy(lEvent.maData, &lu16LandmarkIndex, sizeof(lu16LandmarkIndex));
            PushGuiEvent(lEvent, lpGuiInput);


        }
        return true;
    }

    // ---- 190  the SAT-NAV switch (1 byte) -----------------------------------------------------
    // The action's byte straight into GuiEventMiniMapSwitch (id 205, size 1).
    case 190:
    {
        BrnGui::GuiEventMiniMapSwitch lEvent;
        lEvent.maData[0] = *reinterpret_cast<const u8*>(lpAction);
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 263 / 264 / 265  the drive-thru refusals and the failed super jump (1 byte) ----------
    // No load off the action and no store into the frame: each arm posts a stack byte it never
    // writes, and each consumer (HudMessageAnalyzer 551 "CantPaintCar", 552 "NeedToFixCar",
    // 549 "JumpFailed") reads only the id.
    case 263:
    {
        BrnGui::GuiEventCantPaintCar lEvent;
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }
    case 264:
    {
        BrnGui::GuiEventMustFixCarFirst lEvent;
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }
    case 265:
    {
        BrnGui::GuiEventSuperJumpFailed lEvent;
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 186  the REGION-FROM-POSITION answer (8 bytes) ---------------------------------------
    // The WorldRegion {county, district} straight across into GuiEventReturnDistrict (id 196).
    case 186:
    {
        BrnGui::GuiEventReturnDistrict lEvent;
        std::memcpy(lEvent.maData, lpAction, sizeof(lEvent.maData));
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 202  ALL events discovered (1 byte, unread) ------------------------------------------
    // No load off the action: the bare CgsGui::GuiEvent<312> tag.
    case BrnGameState::GameStateModuleIO::E_ACTION_ALL_EVENTS_DISCOVERED:
    {
        AllEventsDiscoveredWire312 lEvent;
        lEvent.mu8Unused = 0;
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    // ---- 203  ALL events of a type discovered (4 bytes) ---------------------------------------
    // Null assert on the record, then its word (the runtime mode type) into
    // GuiEventAllJunctionsDiscoveredOfType (id 313).
    case 203:
    {
        CGS_ASSERT_AT(lpAction != 0,
            "lpAllEventsDiscoveredOfTypeAction != NULL", CGS_ARTIST_FILE, 1719);
        const s32 liGameModeType = *reinterpret_cast<const s32*>(lpAction);

        BrnGui::GuiEventAllJunctionsDiscoveredOfType lEvent;
        std::memcpy(lEvent.maData, &liGameModeType, sizeof(liGameModeType));
        PushGuiEvent(lEvent, lpGuiInput);


        return true;
    }

    default:
        return false;
    }
}
} // namespace BrnGame

#undef CGS_ARTIST_FILE
