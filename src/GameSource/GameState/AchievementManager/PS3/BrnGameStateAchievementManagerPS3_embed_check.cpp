// Compile-only embedder check for the eight gameplay-event hooks bodied in
// BrnGameStateAchievementManagerPS3.cpp. Not part of the shipping build: it defines a
// minimal CONCRETE subclass of AchievementManagerBase (supplying the two protected pure
// virtuals AchievementEarnt / IsAchievementEarnt) so the abstract base can be
// instantiated, then exercises each of the eight hooks by name. This forces the
// compiler to instantiate the hook bodies (which dispatch through those two virtuals).
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

namespace BrnGameState
{

// Concrete stand-in: supplies the two pure virtuals so the base is instantiable here.
class AchievementManagerEmbedderCheck : public AchievementManagerBase
{
protected:
    void AchievementEarnt(EAchievement leAchievement) override { (void)leAchievement; }
    bool IsAchievementEarnt(EAchievement leAchievement) override { (void)leAchievement; return false; }
};

void EmbedderCheck_ExerciseAchievementPS3Hooks(
    AchievementManagerEmbedderCheck& lrManager,
    u32 luCompletedChallengeCount,
    s32 liNumberOfNetworkPlayers,
    f32 lfNewSkillzTotal,
    u32 luMugshotCount,
    s32 liMugshotCount,
    s32 liGameModeType,
    bool lbFlag,
    s32 liChainA,
    s32 liChainB,
    s32 liRivalCount)
{
    lrManager.OnCaughtFever();
    lrManager.OnFreeburnChallengeBlockComplete();
    lrManager.OnFreeburnChallengeComplete(luCompletedChallengeCount);
    lrManager.OnFreeburnSkillzTotalChange(liNumberOfNetworkPlayers, lfNewSkillzTotal);
    lrManager.OnMugshotAdded(luMugshotCount);
    lrManager.OnMugshotSent(liMugshotCount);
    lrManager.OnOnlineRaceComplete(liGameModeType, lbFlag, liChainA, liChainB);
    lrManager.OnRivalAdded(liRivalCount);
}

}
