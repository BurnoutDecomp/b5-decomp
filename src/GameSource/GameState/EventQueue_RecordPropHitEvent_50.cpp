#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/BrnGameEvents.h"

// Explicit instantiation of EventQueue<RecordPropHitEvent,50>::Construct.
template void CgsModule::EventQueue<BrnGameState::GameStateModuleIO::RecordPropHitEvent, 50>::Construct();
