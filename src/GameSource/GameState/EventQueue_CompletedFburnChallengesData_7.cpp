#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"

// CgsModule::EventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData, 7>::Construct  @ 0x82373B50
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (7) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData, 7>::Construct();
