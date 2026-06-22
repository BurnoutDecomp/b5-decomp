// Compile-only embedder check for BrnGameState::BurnoutSkillzManager. Not part of the shipping
// build: it embeds the manager by value (the same way OnlineFreeBurnLobbyMode owns one) so the
// full member layout instantiates, and exercises the public entry points by name -- the same calls
// the lobby mode's Construct / PreWorldUpdate / ProcessNewRoadScore / OnEnterRoad / OnModeEnd make.
#include "GameSource/GameState/ModeManager/GameModes/BrnBurnoutSkillzManager.h"

namespace BrnGameState
{

// Stand-in for the owning lobby mode: embeds the manager by value.
struct BurnoutSkillzManagerEmbedderCheck
{
    BurnoutSkillzManager mBurnoutSkillzManager;
};

void EmbedderCheck_ExerciseBurnoutSkillz(
    BurnoutSkillzManagerEmbedderCheck& lrEmbedder,
    ModeManager* lpModeManager,
    const GameStateModuleIO::PreWorldInputBuffer* lpInput,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
    const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInput,
    GameStateModuleIO::OutputBuffer* lpOutput,
    StreetManager* lpStreetManager,
    MugshotManager* lpMugshotManager,
    BrnStreetData::ChallengePlayerScoreEntry lScore,
    BrnStreetData::ScoreType leScoreType,
    BrnNetwork::Road::ChallengeIndex liChallengeIndex,
    EActiveRaceCarIndex leActiveRaceCarIndex)
{
    BurnoutSkillzManager& lrManager = lrEmbedder.mBurnoutSkillzManager;

    lrManager.Construct(lpModeManager);
    lrManager.SetStreetManager(lpStreetManager);
    lrManager.SetMugshotManager(lpMugshotManager);
    lrManager.OnEnterRoad(liChallengeIndex);
    lrManager.PreWorldUpdate(lpInput, lpActiveCarInterface, lpOutput, false);
    lrManager.UpdatePostWorld(lpPostWorldInput);
    lrManager.SendUpdatePlayerSkillsEvent(leActiveRaceCarIndex, true);
    lrManager.ProcessNewRoadScore(lpOutput, lScore, leScoreType, liChallengeIndex, leActiveRaceCarIndex);
    lrManager.BufferNewRoadScore(lScore, leScoreType, liChallengeIndex);
    lrManager.OnModeEnd(false);
}

}
