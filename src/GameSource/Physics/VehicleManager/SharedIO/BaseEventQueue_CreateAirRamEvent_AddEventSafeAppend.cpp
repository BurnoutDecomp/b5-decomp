#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::AddEventSafe/Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                // BrnPhysics::Vehicle::CreateAirRamEvent (64-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateAirRamEvent>::AddEventSafe @ 0x822C7FB8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateAirRamEvent>::Append       @ 0x827A78E8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 64 bytes:
//   AddEventSafe (@0x822C7FB8): asserts mpEvents!=NULL, returns false WITHOUT appending when
//     miLength >= miMaxLength, else copies 8 qwords (64 bytes: slwi r10,r11,6 == miLength*64 dest),
//     bumps miLength, returns true.
//   Append (@0x827A78E8): asserts mpEvents!=NULL, no-overflow ("Base event queue overflow"), then
//     XMemCpy's 64*srcLen bytes onto the tail (slwi-by-6 count/dest) and advances miLength.
// sizeof(CreateAirRamEvent) == 64 (DWARF BrnVehicleEvents.h:586, pinned in the embed check).
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateAirRamEvent>::AddEventSafe(
    const BrnPhysics::Vehicle::CreateAirRamEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateAirRamEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateAirRamEvent>&);
