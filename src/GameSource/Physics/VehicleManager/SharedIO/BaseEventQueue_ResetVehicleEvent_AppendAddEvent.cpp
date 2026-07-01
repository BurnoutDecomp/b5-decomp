#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::ResetVehicleEvent (128-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ResetVehicleEvent>::AddEvent @ 0x822C7A20
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ResetVehicleEvent>::Append   @ 0x823C3048
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// The X360 element stride is 128 bytes: AddEvent's slwi r9,r11,7 (index*128) and its store map
// (u32@0, 6x lvx/stvx across [16..112), 3 bools@112..114, f32@116, u32@120) span the committed
// alignas(16) ResetVehicleEvent [0..124) padded to 128; Append's XMemCpy copies 128*count bytes.
// sizeof(ResetVehicleEvent) == 128, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ResetVehicleEvent>::AddEvent(const BrnPhysics::Vehicle::ResetVehicleEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ResetVehicleEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ResetVehicleEvent>&);
