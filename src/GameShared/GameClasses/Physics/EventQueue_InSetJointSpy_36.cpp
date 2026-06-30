#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InSetJointSpy, 36>::Construct
//   @ X360 0x828A63E8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InSetJointSpy,36>").
//
// Fixed-capacity (36) event-queue instantiation: the derived EventQueue<T,36>::Construct points the
// BaseEventQueue<T> base at its inline maEvents buffer (the 12-byte base pads to the 16-byte element
// alignment -> the buffer lives at +0x10, exactly the asm's `addi r30, r31, 0x10`), stores the max
// length (36 == 0x24, the asm's `li r11,0x24; stw r11,4(r31)`) and clears the live count
// (`li r10,0; stw r10,8(r31)`). The lpEventBuffer != NULL tripwire (CgsBaseEventQueue.h:160) is
// reproduced by BaseEventQueue<T>::Construct (always true: &maEvents[0] == this + 0x10).
//
// Called from PhysicsSimulationIO::InputBuffer::Construct. Thin explicit instantiation only (the
// shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just Construct is in this
// TU's ledger. Construct never touches the payload itself, so it does NOT attest the InSetJointSpy
// element stride -- the payload is sized only to the 16-byte alignment class (see
// CgsPhysicsSimulationIO_Events.h).
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InSetJointSpy, 36>::Construct();
