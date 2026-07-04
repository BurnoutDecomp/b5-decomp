#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::VehicleAddedForCollisionEvent (16-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::GetEvent(s32)  @ 0x822AC848
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnWorld::ActiveRaceCar::SendAddedRemovedNetworkCarForCollisionEvents to walk the
// added/removed-network-car collision queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:292), liIndex < GetLength() (:294)
// and liIndex >= 0 (:295) -- the non-const GetEvent(int) overload (DWARF :290) -- then returns
// &mpEvents[liIndex] (`result = 16*a2 + mpEvents`, `slwi r11, liIndex, 4`). The 16-byte stride is
// sizeof(VehicleAddedForCollisionEvent) == VolumeInstanceId(8, 8-byte aligned) + bool, padded to
// 0x10. The Hex-Rays `int` return is the ABI-returned T& pointer; the DWARF (CgsBaseEventQueue.h:6746)
// gives the real `VehicleAddedForCollisionEvent&`.
template BrnPhysics::Vehicle::VehicleAddedForCollisionEvent&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::GetEvent(s32);

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>::GetEvent(s32)  @ 0x822ACA38
// Second COMDAT copy (caller BrnWorld::RaceCarEntityModule::UpdateNearMisses /
// BrnTraffic::TrafficEntityModule::HandleExternalResponses); same instantiation, covered by the
// single explicit-instantiation line above.
