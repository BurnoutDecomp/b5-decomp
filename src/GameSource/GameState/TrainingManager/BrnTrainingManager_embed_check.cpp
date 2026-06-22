// Compile-only embedder check for BrnGameState::TrainingManager. Not part of the shipping build:
// it embeds the manager by value (the same way the GameStateModule owns one) so the full member
// layout instantiates, and exercises the public entry points by name -- the same calls the
// GameStateModule makes into the manager. Mirrors how the X360 GameStateModule embeds the
// TrainingManager by value.
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"

namespace BrnGameState
{

// Stand-in for the owning module: embeds the manager by value.
struct TrainingManagerEmbedderCheck
{
    TrainingManager mTrainingManager;
};

void TrainingManager_EmbedderCheck_Exercise(
    TrainingManagerEmbedderCheck& lrEmbedder,
    BrnProgression::ProgressionManager* lpProgressionManager,
    GameStateModule* lpGameStateModule,
    GameStateModuleIO::GameActionQueue* lpGameActionQueue,
    BrnProgression::ETrainingType leTrainingType)
{
    TrainingManager& lrManager = lrEmbedder.mTrainingManager;

    lrManager.Construct(lpProgressionManager, lpGameStateModule);
    lrManager.RequestTraining(leTrainingType);
    lrManager.OnTogglePictureParadise(true);
    lrManager.OnVoiceoverFinished();
    lrManager.OnEnableTrainingTips(true);
    lrManager.ForceUnpause(lpGameActionQueue);
    lrManager.TriggerAnyFollowOnTrainingTips(leTrainingType, lpGameActionQueue);
    lrManager.DEBUG_ClearTrainingFlags();
}

}
