#include "GameSource/GameState/ModeManager/BrnModeManager.h"

namespace BrnGameState
{
// ARTIST 0x823406E0. The debug base teardown is an empty ICF-folded leaf.
void ModeManager::Destruct()
{
    mScoringSystemDebugComponent.mpScoringSystem = 0;
    mScoringSystemDebugComponent.mbShowChainableStunts = false;
    mModeManagerDebugComponent.mpModeManager = 0;
    mChallengeManager.Destruct();
    mHUDMessageLogic.Destruct();
    for (s32 i = 0; i < KI_GAME_MODE_SLOTS; ++i)
    {
        if (mapGameModes[i] != 0)
            mapGameModes[i]->Destruct();
    }
    mbDistanceToFinishLineTransmitted = false;
    mbModeIntroStarted = false;
    mbFinishedOnlineEvent = false;
    mbInstantIntroSplash = false;
    mbStuntChallengeActive = false;
}
}
