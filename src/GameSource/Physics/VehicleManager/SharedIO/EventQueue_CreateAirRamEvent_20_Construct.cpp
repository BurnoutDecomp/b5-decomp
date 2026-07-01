#include "GameShared/GameClasses/Module/CgsEventQueue.h"                 // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::CreateAirRamEvent (64-byte element)

// CgsModule::EventQueue<BrnPhysics::Vehicle::CreateAirRamEvent, 20>::Construct @ 0x822E33D0
//   (ledger id: class:BrnPhysics::Vehicle::CreateAirRamEvent,20>)
//
// The mAirRamQueue member of VehicleEffectsInputInterface. Thin explicit instantiation the X360
// emitted out-of-line for the CreateAirRamEvent/20 specialisation; the generic
// EventQueue<T,N>::Construct body is already inline in CgsEventQueue.h.
//
// X360 store-for-store (asm at 0x822E33D0): addi r30,this,0x10 (lpEventBuffer = &maEvents),
// stw r30,0(this) (mpEvents), li 0x14/stw ,4(this) (miMaxLength = 20), li 0/stw ,8(this)
// (miLength = 0). mpEvents landing at this+0x10 confirms the {T* @0, s32 @4, s32 @8} header + 4
// pad, then CreateAirRamEvent[20] inline at +16. Element stride 64 (DWARF BrnVehicleEvents.h:586,
// pinned in VehicleEvents_embed_check.cpp).
template void
CgsModule::EventQueue<BrnPhysics::Vehicle::CreateAirRamEvent, 20>::Construct();
