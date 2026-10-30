#include "GameSource/GameState/ModeManager/BrnModeManager.h"

namespace BrnGameState
{
    // ARTIST823274F8: these are ChallengeManager's three adjacent owners.
    bool ModeManager::Release()
    {
        mChallengeManager.mpFreeburnChallengeList = 0;
        mChallengeManager.mpRoadRulesManager = 0;
        mChallengeManager.mpTriggerQueryManager = 0;
        return mScoringSystem.Release();
    }
}
