// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.cpp
//
// The BrnGame::BrnGameModule game-state->X bridge family. Reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX. Only the verified functions of this TU are homed here;
// the two top-level bridge entry points (BridgeGameStateToGui 0x823EE880 /
// BridgeGameStateToNetwork 0x823E2398) remain DEFERRED (blocked: their pseudocode is not
// store-for-store faithful -- ToGui's IDA output is flagged "local variable allocation has
// failed" over ~160 locals with un-homed GUI event payloads + VariableEventQueue Append;
// ToNetwork is a ~240-case switch over un-homed network-action payloads) and are homed by
// their owning batches once those event layouts land.
//
//   BridgeGameStateToController     0x823C0AE8  [reconstructed]
//   ConvertTrainingTypeToStringId   0x823AA3B8  [MOVED 2026-08-16 ->
//                                    GameBridgeGameStateToX_TrainingStringIds.cpp; see the
//                                    MOVED-OUT block below for the four compile errors that
//                                    keep THIS file out of the build]
//   TranslateTakedownsToGuiEvents   0x823E1C38  [reconstructed, DOES NOT COMPILE -- see below]
//
// FLAG (by-name, un-homed): the game-state input bind/unbind REQUEST-queue accessors
// (BrnGameState::GetGameStateInput*RequestQueue; X360 sub_823B9CD8, +0x4C for the second
// queue) and the PostWorldInputBuffer write-side PostBindRequest/PostUnbindRequest are
// reached by name (declared in GameBridgeGameStateToX.h / CgsInputModuleIO.h).
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/Game/GameBridgeControllerToX.h"                // CgsGui::GuiModule + AddGuiEvent<T> (established placeholder home)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"  // CgsInput::InputIO::PostWorldInputBuffer / BaseInputEvent
#include "SharedClasses/Progression/BrnTrainingTypes.h"            // BrnProgression::ETrainingType

namespace BrnGame
{
    // =========================================================================
    // BridgeGameStateToController  (X360 0x823C0AE8)
    // Merge the game-state output's input-bind / input-unbind REQUEST queues into the input
    // module's post-world input buffer: PostBindRequest per queued bind, PostUnbindRequest per
    // queued unbind. When either queue produced any work, stamp miInputModuleState (@ +10094136)
    // with the bind (3) / unbind (6) sentinel. Called by DoUpdate_InputPostWorld.
    //
    // FLAG: the game-state input bind/unbind request-queue accessors (X360 sub_823B9CD8; the
    // second queue sits at +0x4C from the first) are un-homed and reached by name with the
    // committed CgsInput bind/unbind request-queue types.
    // =========================================================================
    void BrnGameModule::BridgeGameStateToController(
        BrnGameState::GameStateModule* lpGameStateOutput,
        CgsInput::InputIO::PostWorldInputBuffer* lpPostWorldInput)
    {
        const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue* lpBindQueue =
            BrnGameState::GetGameStateInputBindRequestQueue(lpGameStateOutput);
        const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue* lpUnbindQueue =
            BrnGameState::GetGameStateInputUnbindRequestQueue(lpGameStateOutput);

        CGS_ASSERT(lpBindQueue   != 0, "lpGameStateInputBindRequestQueue");
        CGS_ASSERT(lpUnbindQueue != 0, "lpGameStateInputUnbindRequestQueue");

        // Bind requests: element i carries {word0, word1} (asm ld 8 bytes then split r4/r5).
        const s32 liNumBind = lpBindQueue->GetLength();   // *(queue+8)
        for (s32 i = 0; i < liNumBind; ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpBindQueue->GetEvent(i);
            const s32* lpWords = reinterpret_cast<const s32*>(&lrEvent);
            lpPostWorldInput->PostBindRequest(lpWords[0], lpWords[1]);
        }
        if (liNumBind > 0)
            miInputModuleState = 3;

        // Unbind requests: element j carries a single request word (asm *v12 -> PostUnbindRequest).
        const s32 liNumUnbind = lpUnbindQueue->GetLength();   // *(queue+8)
        for (s32 j = 0; j < liNumUnbind; ++j)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpUnbindQueue->GetEvent(j);
            const s32* lpWords = reinterpret_cast<const s32*>(&lrEvent);
            lpPostWorldInput->PostUnbindRequest(lpWords[0]);
        }
        if (liNumUnbind > 0)
            miInputModuleState = 6;
    }

    // ------------------------------------------------------------------------
    // MOVED OUT (2026-08-16, tutorial-ticker leg):
    //   ConvertTrainingTypeToStringId @0x823AA3B8 + its two rodata string-ID tables now
    //   live in the per-function sibling TU
    //   GameSource/Game/GameBridgeGameStateToX_TrainingStringIds.cpp.
    //   MOVED, not copied -- folding it back in later is a delete, not a duplicate-symbol hunt
    //   (the same _Prepare.cpp / _SetupParRivals.cpp precedent the build script documents).
    //
    // ⛔ WHY THE SPLIT: **THIS TU DOES NOT COMPILE**, and did not before this leg either --
    // MEASURED with a control (HEAD's own copy of this file, compiled with the canonical
    // build flags, produces the SAME four errors, so none of them is new work):
    //   1. C2011 'BrnGui::GuiTakedownEvent': struct type redefinition. The placeholder in
    //      GameBridgeGameStateToX.h collides with the real 40-byte record in
    //      BrnGuiEventTypeDefs.h:1056. ⚠️ That header's own comment claims "the include
    //      graphs do not meet" -- THEY DO. This TU includes BrnGameModule.hpp, which reaches
    //      BrnGuiEventTypeDefs.h. The claim is stale and the compiler falsifies it.
    //   2. The same fork exists for GuiSoftTakedownEvent (the bridge header's field-by-field
    //      placeholder vs BrnGuiDemangledEventTypes.h:264's opaque GuiEvent<364>+u8[20]);
    //      the compiler only reports #1 because it stops at the first definition clash.
    //   3. C2065 'mpCgsGuiModule': undeclared identifier. BrnGameModule no longer has that
    //      member -- it holds mGuiModule (a BrnGui::GuiModule BY VALUE, hpp:620). This TU is
    //      stale against a rename.
    //   4. The two C2027s are consequences of 1+3.
    // All four are inside TranslateTakedownsToGuiEvents / its header block. Repairing them is
    // a takedown-HUD job (two placeholder records to reconcile against two different canonical
    // types, one of them itself opaque) with no way to verify from here -- so it is NAMED, not
    // guessed at. ConvertTrainingTypeToStringId shares none of that surface, so it is split out
    // and mounted on its own.
    // ------------------------------------------------------------------------

    // =========================================================================
    // TranslateTakedownsToGuiEvents  (X360 0x823E1C38)
    // For each record in the game-state output's TakedownEvent output queue, synthesise a takedown
    // GUI event and push it through the CgsGui GUI module (this + 7252512). A record is a HARD
    // takedown (BrnGui::GuiTakedownEvent) when its race-car index equals the runner index, or when
    // the soft-takedown-display flag bit is clear; otherwise it is a SOFT takedown
    // (BrnGui::GuiSoftTakedownEvent) which carries only the leading fields + two status bytes.
    //
    // FLAG: the takedown queue element accessor (BrnGameState::GetTakedownEventOutputRecord), the
    // GUI event payload layouts, and the soft-display flag word are un-homed placeholders reached
    // by name (see GameBridgeGameStateToX.h / BrnGameModule.hpp) -- the parity contract encoded here
    // is the record-by-record hard/soft classification + store-for-store field copy.
    // =========================================================================
    void BrnGameModule::TranslateTakedownsToGuiEvents(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const void* lpTakedownQueue,
        s32 liRunnerActiveRaceCarIndex)
    {
        // X360 `li r,1; extldi r,r,64,33` -> 64-bit mask 0x0000000200000000 (bit 33).
        static const u64 KU_TAKEDOWN_SOFT_DISPLAY_MASK = 0x0000000200000000ull;

        CgsGui::GuiModule* lpGui = mpCgsGuiModule;

        const s32 liCount = BrnGameState::GetTakedownEventOutputCount(lpTakedownQueue);  // *(queue+8)
        for (s32 i = 0; i < liCount; ++i)
        {
            const BrnGameState::TakedownEventOutputRecord* lpRecord =
                BrnGameState::GetTakedownEventOutputRecord(lpTakedownQueue, i);

            // Hard when the record's race-car index matches the runner, or the soft-display bit is clear.
            bool lbHard;
            if (lpRecord->miRaceCarIndex == liRunnerActiveRaceCarIndex)
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
                lEvent.mu64Field00   = lpRecord->mu64Field08;   // +0x00 <- src +0x08
                lEvent.mu64Field08   = lpRecord->mu64Field10;   // +0x08 <- src +0x10
                lEvent.miField10     = lpRecord->miField00;     // +0x10 <- src +0x00
                lEvent.miRaceCarIndex= lpRecord->miRaceCarIndex;// +0x14 <- src +0x04
                lEvent.miField18     = lpRecord->miField18;     // +0x18 <- src +0x18
                lEvent.miField1C     = lpRecord->miField20;     // +0x1C <- src +0x20
                lEvent.miField20     = lpRecord->miField1C;     // +0x20 <- src +0x1C
                lEvent.mbStatus24    = lpRecord->mbField24;     // +0x24 <- src +0x24
                lEvent.mbStatus25    = lpRecord->mbField26;     // +0x25 <- src +0x26
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiInput);
            }
            else
            {
                BrnGui::GuiSoftTakedownEvent lEvent;
                lEvent.mu64Field00   = lpRecord->mu64Field08;   // +0x00 <- src +0x08
                lEvent.mu64Field08   = lpRecord->mu64Field10;   // +0x08 <- src +0x10
                lEvent.miField10     = lpRecord->miField00;     // +0x10 <- src +0x00
                lEvent.miRaceCarIndex= lpRecord->miRaceCarIndex;// +0x14 <- src +0x04
                lEvent.miField18     = lpRecord->miField18;     // +0x18 <- src +0x18
                lEvent.mbStatus1C    = lpRecord->mbField24;     // +0x1C byte0 <- src +0x24
                lEvent.mbStatus1D    = lpRecord->mbField26;     // +0x1D byte1 <- src +0x26
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiInput);
            }
        }
    }
} // namespace BrnGame
