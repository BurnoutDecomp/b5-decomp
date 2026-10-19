// GameState-to-X bridges. ARTIST 0x823EE880 is recovered below with its direct
// output builders in EventStatusGuiEvents/EventStartsGuiEvents. The network
// wrapper remains deferred; the action translator has its separately owned TU.

#include "GameSource/Game/BrnGameModule.hpp"
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
        CGS_ASSERT(lpGuiInput != 0, "lpGuiInput");
        CGS_ASSERT(lpGameStateOutput != 0, "lpGameStateOutput");
        const BrnGameState::GameStateModuleIO::ScoringOutputInterface* lpScoringOutputInterface =
            lpGameStateOutput->GetScoringOutputInterface();
        const BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface* lpOnlScoringOutputInterface =
            lpGameStateOutput->GetOnlineScoringOutputInterface();
        CGS_ASSERT(lpScoringOutputInterface != 0, "lpScoringOutputInterface");
        CGS_ASSERT(lpOnlScoringOutputInterface != 0, "lpOnlScoringOutputInterface");
        CGS_ASSERT(lpGuiInput->GetGuiEvents() != 0, "lpGuiInput->GetGuiEvents()");
        CGS_ASSERT(lpGameStateOutput->GetGuiEventQueue() != 0, "lpGameStateOutput->GetGuiEventQueue()");

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

        // [gui-iface] PC witness (BRN_MODEMGR_DIAG -- the switch the [tailing] producer witness
        // already uses): prove the leg RUNS and what it posted. The first 24 non-empty calls.
        // [FLAG PC witness]
        {
            static const bool sbDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
            static s32 siPrinted = 0;
            const s32 liPosted = liDTNewLength + liDTTriggeredLength + liDTEndedLength
                               + liOvertakeLength + liFinishRaceLength + liTookLeadLength
                               + liTookLastLength + liOnTailQueueLength;
            if (sbDiag && liPosted > 0 && siPrinted < 24 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siPrinted;
                *CgsDev::Log::gpDebugPrint
                    << "[gui-iface] posted 177x" << liDTNewLength << " 179x" << liDTTriggeredLength
                    << " 181x" << liDTEndedLength << " 371x" << liOvertakeLength
                    << " 372x" << liFinishRaceLength << " 484x" << liTookLeadLength
                    << " 485x" << liTookLastLength << " 486x" << liOnTailQueueLength
                    << " [FLAG PC witness]\n";
            }
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
        // The console builds the mask with li 1 / extldi 64,33 -- i.e. 1 << 33.
        static const u64 KU_TAKEDOWN_SOFT_DISPLAY_MASK = 0x0000000200000000ull;

        const s32 liCount = lpTakedownQueue->GetLength();

        // [td-gui] PC witness (BRN_TD_DIAG): prove the translator RUNS, and with what queue depth.
        // A takedown that reaches ProcessTakedownEvent but produces no GUI event is either a leg
        // that never runs or a queue drained before this point; only the count separates them.
        // [FLAG PC witness]
        {
            static const bool sbTdDiag = (std::getenv("BRN_TD_DIAG") != 0);
            static s32 siCalls = 0;
            ++siCalls;
            // First three calls, every non-empty call, and a heartbeat every 600 calls -- the
            // heartbeat is the term that separates "runs every frame and reads zero" from "stopped
            // being called after boot", which a first-N-only witness cannot distinguish.
            if (sbTdDiag && CgsDev::Log::gpDebugPrint != 0
                && (liCount > 0 || siCalls <= 3 || (siCalls % 600) == 0))
            {
                *CgsDev::Log::gpDebugPrint << "[td-gui] translator called on queue "
                                           << static_cast<s32>(reinterpret_cast<u64>(lpTakedownQueue) & 0xFFFFFFFFu)
                                           << " queued=" << liCount
                                           << " call=" << siCalls
                                           << " player=" << liPlayerActiveRaceCarIndex
                                           << " [FLAG PC witness]\n";
            }
        }

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
                lbHard = ((mu64TakedownDisplayFlags & KU_TAKEDOWN_SOFT_DISPLAY_MASK) == 0);
            }

            // [td-gui] PC witness (BRN_TD_DIAG): the type actually crossing into the GUI module,
            // and which arm carried it. [FLAG PC witness]
            {
                static const bool sbTdDiag = (std::getenv("BRN_TD_DIAG") != 0);
                if (sbTdDiag && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "[td-gui] " << (lbHard ? "hard" : "soft")
                                               << " type=" << static_cast<s32>(lrRecord.meType)
                                               << " aggressor=" << static_cast<s32>(lrRecord.meAggressorIndex)
                                               << " victim=" << static_cast<s32>(lrRecord.meVictimIndex)
                                               << " chain=" << lrRecord.miTakedownChainCount
                                               << " multiple=" << lrRecord.miMultipleTakedownCount
                                               << " [FLAG PC witness]\n";
                }
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
