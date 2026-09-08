#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

namespace BrnGameState
{
namespace
{
    // X360 SKU ids used by ARTIST 0x8235ACF8.
    const EAchievement E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_TIME =
        static_cast<EAchievement>(30);
    const EAchievement E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_CRASH =
        static_cast<EAchievement>(31);
}

// X360 0x8235ACF8. Completing either complete road-rule set awards its
// corresponding achievement once; any other score type is an authored assert.
void AchievementManagerBase::OnSetAllRoadRules(BrnStreetData::ScoreType leScoreType)
{
    if (leScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_TIME))
            AchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_TIME);
    }
    else if (leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_CRASH))
            AchievementEarnt(E_X360_ACHIEVEMENT_SET_ALL_ROAD_RULE_CRASH);
    }
    else
    {
        CGS_ASSERT(false, "dodgy road rule type!");
    }
}
}
