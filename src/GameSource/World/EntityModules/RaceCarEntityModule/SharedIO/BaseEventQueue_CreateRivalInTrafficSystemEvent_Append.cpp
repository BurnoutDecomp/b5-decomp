#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"

// CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::CreateRivalInTrafficSystemEvent>::Append
//   @ 0x827A7AB8  (stride 48 == sizeof(CreateRivalInTrafficSystemEvent): 2x Vector3 (16+16,
//   SIMD-aligned) + EDistrict(4) + EGlobalRaceCarIndex(4) + s8(1) -> 48). The X360 build emits
// a per-instantiation out-of-line body for the generic BaseEventQueue<T>::Append (already
// committed inline in CgsBaseEventQueue.h). Emit ONLY the explicit instantiation.
template bool
CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::CreateRivalInTrafficSystemEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::RaceCarEntityModuleIO::CreateRivalInTrafficSystemEvent>&);
