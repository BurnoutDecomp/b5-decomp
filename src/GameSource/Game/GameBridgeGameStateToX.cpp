// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.cpp
//
// The BrnGame::BrnGameModule game-state->X bridge family. Reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX. Only the verified functions of this TU are homed here;
// the remaining bridge entry points (BridgeGameStateToGui 0x823EE880 /
// BridgeGameStateToNetwork 0x823E2398 / TranslateTakedownsToGuiEvents 0x823E1C38) are
// DEFERRED (blocked: not store-for-store faithful / un-homed GUI+network event payloads)
// and homed by their owning batches once the event layouts land.
//
//   BridgeGameStateToController   0x823C0AE8  [reconstructed]
//   ConvertTrainingTypeToStringId 0x823AA3B8  [reconstructed]
//
// FLAG (by-name, un-homed): the game-state input bind/unbind REQUEST-queue accessors
// (BrnGameState::GetGameStateInput*RequestQueue; X360 sub_823B9CD8, +0x4C for the second
// queue) and the PostWorldInputBuffer write-side PostBindRequest/PostUnbindRequest are
// reached by name (declared in GameBridgeGameStateToX.h / CgsInputModuleIO.h).
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"

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
} // namespace BrnGame
