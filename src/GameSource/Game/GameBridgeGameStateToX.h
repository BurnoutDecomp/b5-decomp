#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.h
//
// Public declarations for the GameState->X bridge TU (DWARF home
// GameSource/Game/GameBridgeGameStateToX.cpp). Only the surface needed by the
// reconstructed function(s) in this batch is declared here; other bridge entry
// points are added by their owning batches.
// ============================================================================

#include "SharedClasses/Progression/BrnTrainingTypes.h"   // BrnProgression::ETrainingType
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO::PostWorldInputBuffer

namespace BrnGameState { class GameStateModule; }

namespace BrnGameState
{
    // Un-homed game-state accessors reached by name from
    // BrnGameModule::BridgeGameStateToController (X360 0x823C0AE8: sub_823B9CD8(a3) returns the
    // bind-request queue; the unbind-request queue sits at +0x4C from it). Their canonical home
    // is the game-state IO; declared here so the bridge TU resolves them until that TU lands.
    const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue*
        GetGameStateInputBindRequestQueue(GameStateModule* lpGameStateOutput);
    const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue*
        GetGameStateInputUnbindRequestQueue(GameStateModule* lpGameStateOutput);
}

namespace BrnGame
{
    // @0x823AA3B8 -- map a training-tip enum to its GUI string-ID.
    // Returns "ERROR - UNKNOWN TRAINING TYPE" for the unused/gap indices.
    const char* ConvertTrainingTypeToStringId(BrnProgression::ETrainingType leTrainingType);

} // namespace BrnGame
