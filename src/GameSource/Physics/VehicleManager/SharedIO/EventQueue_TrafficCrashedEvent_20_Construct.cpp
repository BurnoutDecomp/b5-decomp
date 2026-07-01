#include "GameShared/GameClasses/Module/CgsEventQueue.h"                     // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"      // BrnPhysics::Vehicle::TrafficCrashedEvent (16-byte element)

// CgsModule::EventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent, 20>::Construct @ 0x822E3050
//   (ledger id: class:BrnPhysics::Vehicle::TrafficCrashedEvent,20>)
//
// The manager output interface's mCrashedTrafficEventQueue (DWARF BrnVehicleOutputInterface.h:176,
// TrafficCrashedEventQueue = EventQueue<TrafficCrashedEvent,20>). The 20 events live inline in the
// derived EventQueue's maEvents[20] buffer; Construct() points the base queue at that inline
// storage (addi r30,this,0x10; stw r30,0(this)), sets capacity 20 (li 0x14; stw ,4) and clears the
// live count (li 0; stw ,8). The generic EventQueue<T,N>::Construct body is already inline in
// CgsEventQueue.h; this TU is the thin explicit instantiation. mpEvents landing at this+0x10
// confirms an 8-byte-aligned element array (TrafficCrashedEvent carries a VolumeInstanceId u64),
// matching sizeof(TrafficCrashedEvent) == 16.
template void
CgsModule::EventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent, 20>::Construct();
