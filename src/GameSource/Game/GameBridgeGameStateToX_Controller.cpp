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

        CGS_ASSERT(lpBindQueue   != 0, "lpGameStateInputBindRequestQueue");
        CGS_ASSERT(lpUnbindQueue != 0, "lpGameStateInputUnbindRequestQueue");

        // Bind requests: element i carries {word0, word1} (asm ld 8 bytes then split r4/r5).
        const s32 liNumBind = lpBindQueue->GetLength();
        for (s32 i = 0; i < liNumBind; ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpBindQueue->GetEvent(i);
            lpPostWorldInput->PostBindRequest(lrEvent.miPlayer, lrEvent.miPort);
        }
        if (liNumBind > 0)
            miInputModuleState = 3;

        // Unbind requests: element j carries a single request word (asm *v12 -> PostUnbindRequest).
        const s32 liNumUnbind = lpUnbindQueue->GetLength();
        for (s32 j = 0; j < liNumUnbind; ++j)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpUnbindQueue->GetEvent(j);
            lpPostWorldInput->PostUnbindRequest(lrEvent.miPlayer);
        }
        if (liNumUnbind > 0)
            miInputModuleState = 6;
    }

} // namespace BrnGame
