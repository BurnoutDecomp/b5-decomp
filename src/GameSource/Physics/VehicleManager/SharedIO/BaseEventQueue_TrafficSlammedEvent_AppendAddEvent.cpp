#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::TrafficSlammedEvent (20-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent>::AddEvent @ 0x825E4AB8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent>::Append   @ 0x827A7478
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 20 bytes: AddEvent's element copy is the
// five-_DWORD `do { *v12++ = *v11++; } while(--v13)` loop (count 5); Append's XMemCpy copies
// 20*count bytes. sizeof(TrafficSlammedEvent) == 2*EntityId(4) + eCrashTrafficType(4) +
// 2*f32(4) == 20, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent>::AddEvent(const BrnPhysics::Vehicle::TrafficSlammedEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent>&);
