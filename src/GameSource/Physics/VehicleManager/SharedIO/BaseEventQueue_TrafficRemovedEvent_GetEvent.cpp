#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::TrafficRemovedEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>::GetEvent(s32)  @ 0x827B4D48
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// CrashModule::WillTrafficVehicleBeRecycledNextFrame and CrashModule::ClearUpRecycledTraffic to
// walk the removed-traffic queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:292), liIndex < GetLength() (:294)
// and liIndex >= 0 (:295) -- the non-const GetEvent(int) overload (DWARF :290) -- then returns
// &mpEvents[liIndex] (`result = 8*a2 + mpEvents`, `slwi r11, liIndex, 3`). The 8-byte stride is
// sizeof(TrafficRemovedEvent) == EntityId(4) + ETrafficType(4) == 8. The Hex-Rays `int` return is
// the ABI-returned T& pointer; the DWARF gives the real `TrafficRemovedEvent&`.
template BrnPhysics::Vehicle::TrafficRemovedEvent&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::TrafficRemovedEvent>::GetEvent(s32);
