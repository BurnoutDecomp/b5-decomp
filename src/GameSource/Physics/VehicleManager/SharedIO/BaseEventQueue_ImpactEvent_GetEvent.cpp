#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::ImpactEvent (48-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>::GetEvent(s32)  @ 0x8254D8B8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already
// inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// NetworkAggressiveDrivingManager::ProcessInputQueue and VehicleManager::UpdateVehicleImpacts.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275), then returns &mpEvents[liIndex] (`result = 48*a2 + mpEvents`). The
// 48-byte multiplier (`liIndex*3<<4`) is sizeof(ImpactEvent) == 48, matching the stride asserted
// in the ImpactEvent Construct/AddEvent/Append sibling. The Hex-Rays `int` return is the
// ABI-returned T& pointer; the DWARF gives the real `ImpactEvent&`.
template BrnPhysics::Vehicle::ImpactEvent&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>::GetEvent(s32);
