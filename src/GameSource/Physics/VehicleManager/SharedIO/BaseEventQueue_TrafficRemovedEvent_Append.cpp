#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::TrafficRemovedEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>::Append  @ 0x827A7808
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Append body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// VehicleManagerOutputInterface and CrashModule::PostPhysicsUpdate. XMemCpy copies 8*count bytes
// onto the tail at an 8-byte stride (slwi-by-3 on both the count and the dest offset),
// cross-confirmed by the sibling AddEvent @0x825BC130. sizeof(TrafficRemovedEvent) == 8.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>&);
