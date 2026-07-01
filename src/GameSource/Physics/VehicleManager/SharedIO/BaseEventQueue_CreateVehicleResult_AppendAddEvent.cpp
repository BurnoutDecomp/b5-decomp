#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // CgsModule::BaseEventQueue<T>::AddEvent / ::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::CreateVehicleResult (16-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateVehicleResult>::AddEvent @ 0x825E4EC8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateVehicleResult>::Append   @ 0x827A7648
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// Element stride is 16 bytes, attested two ways: AddEvent slwi r11,r11,4 (16*miLength index) + a
// two-QWORD element copy (ld/std @0, ld/std @8 == 16 bytes); Append slwi-by-4 count and dest
// (XMemCpy(mpEvents + 16*miLength, src.mpEvents, 16*src.miLength)). sizeof(CreateVehicleResult) ==
// VolumeInstanceId(u64, 8-byte aligned)@0 + bool mbSuccess@8, padded to 16.
// AddEvent called from VehicleManager::ProcessCreateEvents; Append from VehicleManagerOutputInterface.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateVehicleResult>::AddEvent(
    const BrnPhysics::Vehicle::CreateVehicleResult&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateVehicleResult>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateVehicleResult>&);
