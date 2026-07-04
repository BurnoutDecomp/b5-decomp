#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::PhysicalTrafficState (816-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>::GetEvent(s32)  @ 0x8227BE58
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnEffectsGlassManager::UpdateVehicleEffectPositions, BrnSound TrafficSkid::FindPhysicalTrafficState /
// ::UpdateParams, TrafficInAir::UpdatePhysicsData and CrashModule::GenerateOwnedTrafficUpdates to
// read the published physical-traffic-state queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275), then returns &mpEvents[liIndex] (result = 816*a2 + mpEvents). The
// 816-byte (0x330) multiplier (mulli r11,r29,0x330) is sizeof(PhysicalTrafficState) == 816. DWARF
// confirms CgsModule::BaseEventQueue<PhysicalTrafficState>::GetEvent(int) (CgsBaseEventQueue.h
// dwarf line 4093+). The Hex-Rays `int` return is the ABI-returned PhysicalTrafficState& pointer.
template BrnPhysics::Vehicle::PhysicalTrafficState&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>::GetEvent(s32);
