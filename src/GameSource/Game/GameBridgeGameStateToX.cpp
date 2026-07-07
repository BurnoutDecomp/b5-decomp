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
//   ConvertTrainingTypeToStringId   0x823AA3B8  [reconstructed]
//   TranslateTakedownsToGuiEvents   0x823E1C38  [reconstructed]
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
    // Two file-scope string-ID lookup tables (X360 rodata off_82CDBF40 /
    // dword_82FAE290). Both are const char*[] with a 4-byte element stride
    // (asm: slwi rX, rX, 2 ; lwzx). Contents are UNRECOVERABLE from this TU's
    // dossier -- only the first "specific" entry ("TRAINING_LEAVES_JUNKYARD")
    // is attested -- so they are referenced by their DWARF names and defined
    // elsewhere in the full translation unit rather than fabricated here.
    //   KAC_SPECIFIC_TRAINING_TEXT : const char*[77]   (@0x82CDBF40)
    //   KAC_GENERAL_TRAINING_TEXT  : const char*[128]  (@0x82FAE290)
    extern const char* const KAC_SPECIFIC_TRAINING_TEXT[77];
    extern const char* const KAC_GENERAL_TRAINING_TEXT[128];

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

        CGS_ASSERT(liType >= 0 && liType < BrnProgression::E_TRAINING_TYPE_COUNT,
                   "leTrainingType >= 0 && leTrainingType < BrnProgression::E_TRAINING_TYPE_COUNT");

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
            CGS_ASSERT(liIndex < 128, "liIndex < ciNumGeneralTrainingStringIDs");
            return "ERROR - UNKNOWN TRAINING TYPE";
        }
        return KAC_GENERAL_TRAINING_TEXT[liIndex];
    }

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
