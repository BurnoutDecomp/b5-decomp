#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/BrnGameEvents.h"

// Explicit instantiations of EventQueue<HitOverheadSignEvent,100>::Construct and
// BaseEventQueue<HitOverheadSignEvent>::AddEvent (template bodies inline in the committed headers).
template void CgsModule::EventQueue<BrnGameState::GameStateModuleIO::HitOverheadSignEvent, 100>::Construct();
template bool CgsModule::BaseEventQueue<BrnGameState::GameStateModuleIO::HitOverheadSignEvent>::AddEvent(const BrnGameState::GameStateModuleIO::HitOverheadSignEvent&);
