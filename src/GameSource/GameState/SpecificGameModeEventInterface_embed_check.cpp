// Compile-only embedder check for BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface.
// Not part of the shipping build: it embeds the interface by value (forcing the Array<Event,175>
// layout to be instantiated) and exercises AddEvent / Event::GetLandmark by name, the same way a
// GameStateModuleIO buffer would. Mirrors how the X360 GameStateModuleIO buffers own one of these
// per active game mode.
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameStateTypes.h"  // BrnGameState::LandmarkIndex

namespace BrnGameState
{
namespace GameStateModuleIO
{

// Stand-in for the owning module buffer: embeds the event interface by value.
struct SpecificGameModeEventInterfaceEmbedderCheck
{
    SpecificGameModeEventInterface mEventInterface;
};

LandmarkIndex EmbedderCheck_Exercise(SpecificGameModeEventInterfaceEmbedderCheck& lrEmbedder,
                                     s32 liEventID, u32 luTriggerId,
                                     LandmarkIndex* lpaLandmarks, s32 liNumLandmarks)
{
    SpecificGameModeEventInterface::Event* lpEvent =
        lrEmbedder.mEventInterface.AddEvent(liEventID, luTriggerId, lpaLandmarks, liNumLandmarks);

    return lpEvent->GetLandmark(0);
}

}
}
