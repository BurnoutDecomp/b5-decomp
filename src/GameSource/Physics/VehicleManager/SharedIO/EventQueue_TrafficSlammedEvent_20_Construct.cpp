#include "GameShared/GameClasses/Module/CgsEventQueue.h"                     // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"      // BrnPhysics::Vehicle::TrafficSlammedEvent (20-byte element)

// CgsModule::EventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent, 20>::Construct @ 0x822E30C0
//   (ledger id: class:BrnPhysics::Vehicle::TrafficSlammedEvent,20>)
//
// The manager output interface's mSlammedTrafficEventQueue (DWARF BrnVehicleOutputInterface.h:177,
// TrafficSlammedEventQueue = EventQueue<TrafficSlammedEvent,20>). Construct() points the base
// queue at the inline maEvents[20] storage (addi r30,this,0xC; stw r30,0(this)), sets capacity 20
// (li 0x14; stw ,4) and clears the live count (li 0; stw ,8). The generic EventQueue<T,N>::Construct
// body is already inline in CgsEventQueue.h; this TU is the thin explicit instantiation. mpEvents
// landing at this+0xC (the natural 12-byte header end, no pad) confirms a 4-byte-aligned element
// array: TrafficSlammedEvent is five 4-byte fields, matching sizeof(TrafficSlammedEvent) == 20.
template void
CgsModule::EventQueue<BrnPhysics::Vehicle::TrafficSlammedEvent, 20>::Construct();
