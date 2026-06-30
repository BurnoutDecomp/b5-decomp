#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent() (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::PhysicalTrafficState (816-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>::AddEvent()  @ 0x825E5010
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The no-arg "reserve next slot" AddEvent body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// VehicleOutputInterface::AddTrafficState to grab the next free PhysicalTrafficState slot.
//
// The X360 body:
//   asserts mpEvents != NULL (CgsBaseEventQueue.h:360) and GetLength() < GetMaxLength() (:361),
//   then  v2 = miLength;  result = 816*v2 + mpEvents;  miLength = v2 + 1;  return result;
// i.e. reserves &mpEvents[miLength] and post-increments the live count. The 816-byte multiplier
// (`mulli r10, miLength, 0x330`) is sizeof(PhysicalTrafficState) == 816, matching the element
// stride asserted in BrnVehicleEvents.h / the PhysicalTrafficState Construct+Append siblings.
template BrnPhysics::Vehicle::PhysicalTrafficState&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>::AddEvent();
