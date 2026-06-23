#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint, 10>::Construct
//   @ X360 0x825A7D58 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddJoint,10>").
//
// Fixed-capacity (10) event-queue instantiation: the derived EventQueue<T,10>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes,
// padded to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (10 == 0xA, the asm's `li r11,0xA;
// stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer
// != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct
// (always true: &maEvents[0] == this + 0x10).
//
// This capacity-10 joint queue lives in the OutputBuffers (X360 callers
// BrnPhysics::PhysicsModuleIO::OutputBuffer::Construct and
// BrnPhysics::Vehicle::VehicleManagerOutputBuffer::Construct). The same InAddJoint event
// type is also queued with capacity 36 in PhysicsSimulationIO::InputBuffer (a separate
// instantiation TU). Thin explicit instantiation only (the shared generic bodies live in
// CgsEventQueue.h / CgsBaseEventQueue.h); just Construct is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint, 10>::Construct();
