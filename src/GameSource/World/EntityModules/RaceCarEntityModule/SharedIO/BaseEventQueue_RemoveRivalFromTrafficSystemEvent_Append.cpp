#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"

// CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::RemoveRivalFromTrafficSystemEvent>::Append
//   @ 0x827A7BA8  (stride 1 == sizeof(RemoveRivalFromTrafficSystemEvent): a single s8 miRivalIndex).
// The X360 build emits a per-instantiation out-of-line body for the generic
// BaseEventQueue<T>::Append (already committed inline in CgsBaseEventQueue.h). Emit ONLY the
// explicit instantiation.
template bool
CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::RemoveRivalFromTrafficSystemEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::RemoveRivalFromTrafficSystemEvent>&);
