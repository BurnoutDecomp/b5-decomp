#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"

// CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct  @ 0x822E29C0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct();
