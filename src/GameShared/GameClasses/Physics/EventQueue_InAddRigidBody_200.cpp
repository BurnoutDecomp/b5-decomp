#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody, 200>::Construct
//   @ X360 0x825A7AB8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddRigidBody,200>").
//
// Fixed-capacity (200) event-queue instantiation: the derived EventQueue<T,200>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes, padded
// to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (200 == 0xC8, the asm's `li r11,0xC8;
// stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer
// != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct
// (always true: &maEvents[0] == this + 0x10).
//
// This capacity-200 rigid-body queue lives in PhysicsSimulationIO::InputBuffer (X360 callers
// BrnPhysics::Props::PropOutputInterface::Construct and
// CgsPhysics::PhysicsSimulationIO::InputBuffer::Construct). Thin explicit instantiation only
// (the shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just Construct
// is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody, 200>::Construct();
