#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::TrafficCrashedEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent>::AddEvent @ 0x825BBFE8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent>::Append   @ 0x827A7398
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 16 bytes: AddEvent's element assignment is the
// two-_QWORD copy (ld/std @0 + ld/std @8); Append's XMemCpy copies 16*count bytes (slwi r,r,4).
// sizeof(TrafficCrashedEvent) == VolumeInstanceId(u64, 8-byte aligned)(8) + EntityId(4)
// padded to 16, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent>::AddEvent(const BrnPhysics::Vehicle::TrafficCrashedEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent>&);
