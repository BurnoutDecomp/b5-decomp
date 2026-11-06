#include "GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.h"

using BrnGameState::GameStateModuleIO::CarScoreData;
using BrnReplays::DirectorBridgeSerialiser;
using BrnReplays::DirectorBridgeSerialiserStaticLayout;

CGS_STATIC_ASSERT((sizeof(CarScoreData) == 0x128), "score stream width");
CGS_STATIC_ASSERT((sizeof(bool) == 1), "combo warning stream width");
CGS_STATIC_ASSERT((offsetof(DirectorBridgeSerialiserStaticLayout, mPlayerScoreData) == 0x5DD0), "score stream offset");
CGS_STATIC_ASSERT((offsetof(DirectorBridgeSerialiserStaticLayout, mbPlayerComboWarningActive) == 0x5EF8), "combo warning stream offset");
CGS_STATIC_ASSERT((offsetof(DirectorBridgeSerialiserStaticLayout, maBlock5EFC) == 0x5EFC), "next stream offset");
CGS_STATIC_ASSERT((sizeof(DirectorBridgeSerialiserStaticLayout) == 0x5F00), "full snapshot extent");

extern "C" const CarScoreData* R167ReadPlayerScoreData(DirectorBridgeSerialiser* lpSerialiser)
{
    const CarScoreData& (DirectorBridgeSerialiser::*lpRead)() = &DirectorBridgeSerialiser::ReadPlayerScoreData;
    return &(lpSerialiser->*lpRead)();
}
