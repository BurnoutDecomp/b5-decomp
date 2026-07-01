#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::SetTrafficCrashingEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetTrafficCrashingEvent>::AddEvent @ 0x82719EA8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetTrafficCrashingEvent>::Append   @ 0x823C3678
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// The X360 element stride is 8 bytes: AddEvent's slwi r11,r11,3 (index*8) with the two-word element
// copy (stw@0 EntityId, stw@4 bool mbCrashing padded to word); Append's XMemCpy copies 8*count
// bytes. sizeof(SetTrafficCrashingEvent) == EntityId(4) + bool(1) padded to 8 == 8, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetTrafficCrashingEvent>::AddEvent(const BrnPhysics::Vehicle::SetTrafficCrashingEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetTrafficCrashingEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetTrafficCrashingEvent>&);
