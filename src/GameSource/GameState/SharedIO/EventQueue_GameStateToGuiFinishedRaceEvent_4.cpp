#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiEvents.h"

// Explicit instantiations of the EventQueue<BrnGameState::GameStateToGuiFinishedRaceEvent, 4> methods reconstructed
// by the GameMode leaf batch (committed EventQueue_<Type>_<N>.cpp pattern). The template bodies
// live inline in CgsEventQueue.h / CgsBaseEventQueue.h; the element struct lives in
// BrnGameStateToGuiEvents.h. These lines just force the per-instantiation ledger TUs.
template void CgsModule::EventQueue<BrnGameState::GameStateToGuiFinishedRaceEvent, 4>::Construct();
template bool CgsModule::BaseEventQueue<BrnGameState::GameStateToGuiFinishedRaceEvent>::AddEvent(const BrnGameState::GameStateToGuiFinishedRaceEvent&);
