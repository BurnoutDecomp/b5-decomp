// Embedder gate for the ModeManager TU: proves BrnGameStateModule.h (which embeds ModeManager BY
// VALUE as mModeManager) still compiles against the additively-grown ModeManager layout, and that a
// caller can reach the bodied methods by name. Compile-only; not part of the shipped tree.
#include "GameSource/GameState/BrnGameStateModule.h"

namespace BrnGameState
{
// Exercise the embedded ModeManager surface the way the real callers do (through GetModeManager()).
bool ModeManager_EmbedGate_Probe(GameStateModule* lpModule)
{
    ModeManager* lpModeManager = lpModule->GetModeManager();
    if (lpModeManager == nullptr)
    {
        return false;
    }
    const bool lbOnline   = lpModeManager->IsOnlineGameMode();
    const bool lbPostEvent = lpModeManager->IsInPostEvent();
    const u32  luRemaining = lpModeManager->CountCheckpointsRemaining(E_GLOBAL_RACE_CAR_INDEX_0);
    lpModeManager->ResetCheckpointDataForNextLap(E_GLOBAL_RACE_CAR_INDEX_0);
    lpModeManager->MarkCarHittingCheckpoint(0, E_GLOBAL_RACE_CAR_INDEX_0);
    lpModeManager->SetupStuntChallenge();
    lpModeManager->EndStuntChallenge();
    lpModeManager->TellGuiToShowOnlineFinalStandings();
    return lbOnline || lbPostEvent || (luRemaining > 0);
}
}
