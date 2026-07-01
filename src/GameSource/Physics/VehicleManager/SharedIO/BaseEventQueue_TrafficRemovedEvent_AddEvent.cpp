#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::TrafficRemovedEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>::AddEvent  @ 0x825BC130
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic AddEvent body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// VehicleManagerOutputInterface::AddTrafficRemovedEvent. The X360 body stores at an 8-byte stride
// (slwi r11,r11,3 == miLength*8, then two word stores == 8 bytes) so sizeof(TrafficRemovedEvent) ==
// EntityId(4) + ETrafficType(4) == 8.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>::AddEvent(
    const BrnPhysics::Vehicle::TrafficRemovedEvent&);
