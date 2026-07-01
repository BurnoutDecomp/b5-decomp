#include "GameShared/GameClasses/Module/CgsEventQueue.h"                 // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::CreateSpinEvent (48-byte element)

// CgsModule::EventQueue<BrnPhysics::Vehicle::CreateSpinEvent, 10>::Construct @ 0x822E3440
//   (ledger id: class:BrnPhysics::Vehicle::CreateSpinEvent,10>)
//
// The mSpinQueue member of VehicleEffectsInputInterface. Thin explicit instantiation the X360
// emitted out-of-line for the CreateSpinEvent/10 specialisation; the generic
// EventQueue<T,N>::Construct body is already inline in CgsEventQueue.h.
//
// X360 store-for-store (asm at 0x822E3440): addi r30,this,0x10 (lpEventBuffer = &maEvents),
// stw r30,0(this) (mpEvents), li 0xA/stw ,4(this) (miMaxLength = 10), li 0/stw ,8(this)
// (miLength = 0). Element stride 48 (DWARF BrnVehicleEvents.h:620, pinned in
// VehicleEvents_embed_check.cpp).
template void
CgsModule::EventQueue<BrnPhysics::Vehicle::CreateSpinEvent, 10>::Construct();
