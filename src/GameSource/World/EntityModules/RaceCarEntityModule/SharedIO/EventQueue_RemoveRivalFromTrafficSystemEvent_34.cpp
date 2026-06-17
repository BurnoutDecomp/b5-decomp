#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"

// CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::RemoveRivalFromTrafficSystemEvent, 34>::Construct
//   @ 0x822E3520. Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (34) queue of
// RemoveRivalFromTrafficSystemEvent (sizeof == 1: a single s8 miRivalIndex). Construct points
// the base queue at its inline maEvents buffer, sets KI_LENGTH(34) and clears the live count.
// The generic EventQueue<T,N>::Construct / BaseEventQueue<T>::Construct bodies are already
// committed inline in CgsEventQueue.h / CgsBaseEventQueue.h -- this TU is the explicit
// instantiation ONLY (do NOT re-define the generic).
template void CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::RemoveRivalFromTrafficSystemEvent, 34>::Construct();
