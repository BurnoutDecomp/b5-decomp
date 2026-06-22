#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::VehicleAddedForCollisionEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::AddEvent @ 0x822AC8F0
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::Append   @ 0x823C33C8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 16 bytes: AddEvent's element assignment is the
// two-_QWORD copy (ld/std @0 + ld/std @8); Append's XMemCpy copies 16*count bytes (slwi r,r,4).
// sizeof(VehicleAddedForCollisionEvent) == VolumeInstanceId(u64, 8-byte aligned)(8) + bool(1)
// padded to 16, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::AddEvent(const BrnPhysics::Vehicle::VehicleAddedForCollisionEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>&);
