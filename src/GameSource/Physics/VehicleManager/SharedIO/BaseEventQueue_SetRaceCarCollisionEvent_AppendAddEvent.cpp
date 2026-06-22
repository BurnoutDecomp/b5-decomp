#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::SetRaceCarCollisionEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetRaceCarCollisionEvent>::AddEvent @ 0x822C7D28
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetRaceCarCollisionEvent>::Append   @ 0x823C3208
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 8 bytes: AddEvent's element assignment is the
// two-_DWORD copy (*v12 = *a2; v12[1] = a2[1]); Append's XMemCpy copies 8*count bytes.
// sizeof(SetRaceCarCollisionEvent) == EntityId(4) + bool(1) padded to 8 == 8, matching.
// (Append's Hex-Rays signature carries trailing double a3..a6 -- the ABI-scratch FPRs the
// generic memcpy never touches; the recovered generic body is unaffected.)
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetRaceCarCollisionEvent>::AddEvent(const BrnPhysics::Vehicle::SetRaceCarCollisionEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetRaceCarCollisionEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::SetRaceCarCollisionEvent>&);
