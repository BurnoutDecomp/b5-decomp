// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_Controller.cpp
//
// SIBLING SPLIT of GameSource/Game/GameBridgeGameStateToX.cpp (the same MOVED-not-copied
// split as _TrainingStringIds.cpp / _Sound.cpp): BrnGameModule::BridgeGameStateToController
// lives here so its owning TU can be mounted without it.
//
// ⛔ NOT MOUNTED, and the reason is four symbols with no body anywhere in b5-decomp/src that
// this body reaches BY NAME:
//     BrnGameState::GetGameStateInputBindRequestQueue    (GameBridgeGameStateToX.h)
//     BrnGameState::GetGameStateInputUnbindRequestQueue  (        "               )
//     CgsInput::InputIO::PostWorldInputBuffer::PostBindRequest    (CgsInputModuleIO.h)
//     CgsInput::InputIO::PostWorldInputBuffer::PostUnbindRequest  (CgsInputModuleIO.h)
// Mounting this file before those four are homed is an unresolved-externals link failure for
// the whole build. Home them in the game-state IO TU / the input-module IO TU, then add the
// mount line next to its _Sound.cpp sibling.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
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
    // FLAG: the game-state input bind/unbind request-queue accessors (the console has a single accessor; the
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
        const s32 liNumBind = lpBindQueue->GetLength();
        for (s32 i = 0; i < liNumBind; ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpBindQueue->GetEvent(i);
            const s32* lpWords = reinterpret_cast<const s32*>(&lrEvent);
            lpPostWorldInput->PostBindRequest(lpWords[0], lpWords[1]);
        }
        if (liNumBind > 0)
            miInputModuleState = 3;

        // Unbind requests: element j carries a single request word (asm *v12 -> PostUnbindRequest).
        const s32 liNumUnbind = lpUnbindQueue->GetLength();
        for (s32 j = 0; j < liNumUnbind; ++j)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpUnbindQueue->GetEvent(j);
            const s32* lpWords = reinterpret_cast<const s32*>(&lrEvent);
            lpPostWorldInput->PostUnbindRequest(lpWords[0]);
        }
        if (liNumUnbind > 0)
            miInputModuleState = 6;
    }

} // namespace BrnGame
