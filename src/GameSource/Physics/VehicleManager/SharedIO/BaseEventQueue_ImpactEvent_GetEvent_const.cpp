#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent(s32) const (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::ImpactEvent (48-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>::GetEvent(s32) const  @ 0x825BB5E8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked const element accessor body is already
// inline in CgsBaseEventQueue.h (const T& GetEvent(s32) const, decl :270); this is the thin explicit
// instantiation. This is the CONST sibling of the non-const ImpactEvent GetEvent @0x8254D8B8.
// Called by BrnPhysics::Vehicle::PhysicalTrafficManager::UpdateTrafficPhysicsPostSimulation.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the CONST-overload assert-line triple. Then returns &mpEvents[liIndex].
// The element index math (slwi r11,r30,1; add r11,r30,r11 == liIndex*3; slwi r11,r11,4 == *16 ==
// liIndex*48) gives a 48-byte (0x30) stride == sizeof(ImpactEvent).
template const BrnPhysics::Vehicle::ImpactEvent&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>::GetEvent(s32) const;
