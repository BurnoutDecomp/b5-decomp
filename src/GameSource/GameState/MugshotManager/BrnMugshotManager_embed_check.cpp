// Compile-only embedder check for BrnGameState::MugshotManager. Not part of the shipping build:
// it embeds the manager by value (the same way the GameStateModule owns one) so the full member
// layout + offset static_asserts instantiate, and exercises the public lifecycle / event entry
// points by name -- the same calls BrnGameStateModule::Construct / PreWorldUpdate / ProcessGameEvents
// make into the manager. Mirrors how the X360 GameStateModule embeds the MugshotManager by value.
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"

namespace BrnGameState
{

// Stand-in for the owning module: embeds the manager by value.
struct MugshotManagerEmbedderCheck
{
    MugshotManager mMugshotManager;
};

void EmbedderCheck_Exercise(MugshotManagerEmbedderCheck& lrEmbedder,
                            GameStateModule* lpGameStateModule,
                            const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                            GameStateModuleIO::OutputBuffer* lpOutput,
                            const GameStateModuleIO::VehicleOutputInterface* lpVehicleOutput,
                            const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                            GameStateModuleIO::EGameModeType leGameModeType,
                            const OnlineImageReceivedEvent* lpImageReceivedEvent,
                            const OnlineImageCaptureAbortedEvent* lpAbortEvent,
                            ::EActiveRaceCarIndex leBeatenPlayer, CgsID lBeatenRoadID, s32 leScoreType)
{
    MugshotManager& lrManager = lrEmbedder.mMugshotManager;

    lrManager.Construct(lpGameStateModule);
    lrManager.Prepare();
    lrManager.OnRoundStart();
    lrManager.Update(lpInput, lpOutput, lpVehicleOutput, lpTakedownEventQueue, leGameModeType, false);
    lrManager.ProcessImageReceivedEvent(lpImageReceivedEvent);
    lrManager.ProcessBeatenRoadRuleEvent(lpOutput, leBeatenPlayer, lBeatenRoadID, leScoreType);
    lrManager.ProcessOnlineWin(lpOutput);
    lrManager.ProcessAbortCaptureEvent(lpAbortEvent);
    lrManager.OnRoundEnd(true);
    lrManager.Release();
    lrManager.Destruct();
}

}
