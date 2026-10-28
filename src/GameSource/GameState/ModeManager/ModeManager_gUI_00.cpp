// b5-decomp/src/GameSource/GameState/ModeManager/ModeManager_gUI_00.cpp
//
// ModeManager support bodies (owning header BrnModeManager.h).
//
// This mounted partfile retains the two GetScoringSystem accessors and the extracted
// PreWorldUpdateClocksBringUp helper. The partial ConstructInterModeStateBringUp route was
// removed when GameStateModule::Construct switched to the full ModeManager::Construct body in
// BrnModeManager_Lifecycle.cpp.
//
// ModeManager::Prepare is separately selected by GameStateModule::Prepare. The full
// ModeManager::PreWorldUpdate body is selected through the current GameState EMM pre-world path.
// No current caller was found for PreWorldUpdateClocksBringUp in this source tree; retain it for
// separate cleanup and do not add it alongside the full pre-world route.
//
// GetScoringSystem is this repository's de-inlining of the console's ScoringSystem member adjust.
// Keep these accessors while mode/scoring bodies call them.
// ============================================================================================
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem (embedded by value)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"      // [tut-ticker] GameMode::IsOnline (the clock leg)
#include "GameSource/GameState/BrnGameStateModule.h"                     // [tut-ticker] GameStateModule accessors (the clock leg)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // [tut-ticker] TrainingManager::IsInPictureParadise
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // [tut-ticker] CarSelectManager::GetJunkyardId
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // NetworkRoundManager (mpNetworkRoundManager)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

namespace BrnGameState
{

ScoringSystem* ModeManager::GetScoringSystem()
{
    return &mScoringSystem;
}

const ScoringSystem* ModeManager::GetScoringSystem() const
{
    return &mScoringSystem;
}

}
