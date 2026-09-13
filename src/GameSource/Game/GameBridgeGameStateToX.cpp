// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.cpp
//
// The BrnGame::BrnGameModule game-state->X bridge family. Reconstructed store-for-store
// from the console build. Only the verified functions of this TU are homed here; the two
// top-level bridge entry points (BridgeGameStateToGui / BridgeGameStateToNetwork) remain
// DEFERRED (their pseudocode is not store-for-store faithful -- ToGui's IDA output is flagged
// "local variable allocation has failed" over ~160 locals with un-homed GUI event payloads +
// VariableEventQueue Append; ToNetwork is a ~240-case switch over un-homed network-action
// payloads) and are homed by their owning batches once those event layouts land.
//
//   BridgeGameStateToController     [MOVED -> GameBridgeGameStateToX_Controller.cpp; that
//                                    sibling is NOT mounted -- see its banner for the four
//                                    un-homed symbols it reaches by name]
//   ConvertTrainingTypeToStringId   [MOVED -> GameBridgeGameStateToX_TrainingStringIds.cpp]
//   TranslateTakedownsToGuiEvents   [reconstructed, BOTH arms]
//   MapStuntEnumsFromGameplayToGui  [MOVED -> GameBridgeGameStateToX_StuntGuiEvents.cpp]
//   TranslateGameActionsToGuiEvents [PARTIAL (cases 58/59/60), same sibling TU]
// ============================================================================

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

namespace BrnGame
{
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
