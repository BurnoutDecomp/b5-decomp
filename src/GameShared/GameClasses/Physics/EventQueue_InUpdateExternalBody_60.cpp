#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>::Construct
//   @ X360 0x825A8370 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody,60>").
//
// Fixed-capacity (60) event-queue instantiation: the derived EventQueue<T,60>::Construct points the
// BaseEventQueue<T> base at its inline maEvents buffer (the 12-byte base pads to the 16-byte element
// alignment -> the buffer lives at +0x10, exactly the asm's `addi r30, r31, 0x10`), stores the max
// length (60 == 0x3C, the asm's `li r11,0x3C; stw r11,4(r31)`) and clears the live count
// (`li r10,0; stw r10,8(r31)`). The lpEventBuffer != NULL tripwire (CgsBaseEventQueue.h:160) is
// reproduced by BaseEventQueue<T>::Construct (always true: &maEvents[0] == this + 0x10).
//
// Called from BrnPhysics::PhysicsModule::BridgeUpdatedVehiclesToSimulation. Thin explicit
// instantiation only (the shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h);
// just Construct is in this TU's ledger (n_funcs == 1, all bodied here).
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>::Construct();
