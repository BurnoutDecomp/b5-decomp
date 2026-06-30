#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InSetDriveSpy, 1>::Construct
//   @ X360 0x828A6618 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InSetDriveSpy,1>").
//
// Fixed-capacity (1) event-queue instantiation: the derived EventQueue<T,1>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes,
// padded to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (1, the asm's `li r11,1; stw r11,4(r31)`)
// and clears the live count (`li r10,0; stw r10,8(r31)`). The lpEventBuffer != NULL tripwire
// (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct (always true:
// &maEvents[0] == this + 0x10).
//
// This queue is one of PhysicsSimulationIO::InputBuffer's queues (called by InputBuffer::Construct).
// Thin explicit instantiation only (the shared generic bodies live inline in CgsEventQueue.h /
// CgsBaseEventQueue.h); just Construct is in this TU's ledger. No AddEvent/Append/memcpy for
// InSetDriveSpy is in scope to pin its element stride, so the payload is sized only to the 16-byte
// alignment class the asm proves here (`addi r30, r31, 0x10`); see CgsPhysicsSimulationIO_Events.h.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InSetDriveSpy, 1>::Construct();
