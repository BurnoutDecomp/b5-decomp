#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"

// CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct  @ 0x822E29C0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct();

// CgsModule::BaseEventQueue<BrnGameState::TakedownEvent> methods (bodies inline in CgsBaseEventQueue.h).
template bool CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::AddEvent(const BrnGameState::TakedownEvent&);
template bool CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::Append(const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>&);
template BrnGameState::TakedownEvent& CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>::GetEvent(s32);
