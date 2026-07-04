#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::GetEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::CreateRaceCarEvent (160-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>::GetEvent(s32)  @ 0x825BB7F0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked element accessor body is already inline
// in CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// BrnPhysics::Vehicle::VehicleManager::ProcessCreateEvents to walk the create-race-car request queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275), then returns &mpEvents[liIndex]. The element index math
// (slwi r11,r30,2; add r11,r30,r11; slwi r11,r11,5 == ((idx*4)+idx)*32 == idx*160) gives a
// 160-byte (0xA0) stride == sizeof(CreateRaceCarEvent). DWARF confirms
// CgsModule::BaseEventQueue<CreateRaceCarEvent>::GetEvent(int) (CgsBaseEventQueue.h dwarf line
// 6307/6345). The Hex-Rays `int` return is the ABI-returned CreateRaceCarEvent& pointer.
template BrnPhysics::Vehicle::CreateRaceCarEvent&
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>::GetEvent(s32);
