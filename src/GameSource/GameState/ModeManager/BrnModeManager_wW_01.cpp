// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_wW_01.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// ModeManager::HandleWorldStunt: StuntManager::ProcessStuntElement reports every completed
// collectible stunt element here, and the mode manager hands it straight to its embedded
// freeburn-challenge manager (+28160), whose billboard skill is the only consumer.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

namespace BrnGameState
{

void ModeManager::HandleWorldStunt(StuntElementType leStuntType, CgsID lStuntID)
{
    mChallengeManager.HandleWorldStunt(leStuntType, lStuntID);
}

}
