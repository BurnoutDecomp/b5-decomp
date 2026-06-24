#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody, 50>::Construct
//   @ X360 0x825A7C78 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddRigidBody,50>").
//
// Fixed-capacity (50) event-queue instantiation: the derived EventQueue<T,50>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes, padded
// to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (50 == 0x32, the asm's `li r11,0x32;
// stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer
// != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct
// (always true: &maEvents[0] == this + 0x10).
//
// This capacity-50 rigid-body queue lives in the OutputBuffers (X360 callers
// BrnPhysics::PhysicsModuleIO::OutputBuffer::Construct and
// BrnPhysics::Vehicle::VehicleManagerOutputBuffer::Construct). Thin explicit instantiation
// only (the shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just
// Construct is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody, 50>::Construct();
