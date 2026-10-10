// BrnGameState::OnlineFlybyManager -- lifecycle (Construct / Prepare).
//
// The console inlines both into the owning GameStateModule (the manager sits at gsm+0x2D8E0):
// Construct right after the offline manager's in GameStateModule::Construct (the base Construct,
// then a doubleword zero on the "no rivalry" bit array), Prepare as GameStateModule::Prepare's
// stage 18 (the bit array zeroed, then FlybyData::Prepare on the manager's flyby payload).

#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"

namespace BrnGameState
{

void OnlineFlybyManager::Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem)
{
    FlybyManager::Construct(lpGameStateModule, lpScoringSystem);
    mAvailableNoRivalryMessages.Construct();
}

bool OnlineFlybyManager::Prepare()
{
    mAvailableNoRivalryMessages.UnSetAll();
    GetFlybyData()->Prepare();
    return true;
}

}
