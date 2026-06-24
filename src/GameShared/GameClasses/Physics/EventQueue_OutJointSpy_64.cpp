#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutJointSpy, 64>::Construct
//   @ X360 0x828A6928 (dossier id "class:CgsPhysics::PhysicsSimulationIO::OutJointSpy,64>").
//
// Fixed-capacity (64) event-queue instantiation: the derived EventQueue<T,64>::Construct points
// the BaseEventQueue<T> base at its inline maEvents buffer (the 12-byte base pads to the 16-byte
// element alignment -> the buffer lives at +0x10, exactly the asm's `addi r30, r31, 0x10`), stores
// the max length (64 == 0x40, the asm's `li r11,0x40; stw r11,4(r31)`) and clears the live count
// (`li r10,0; stw r10,8(r31)`). The lpEventBuffer != NULL tripwire (CgsBaseEventQueue.h:160) is
// reproduced by BaseEventQueue<T>::Construct (always true: &maEvents[0] == this + 0x10; the asm's
// `cmplwi cr6,r30,0` / `bne` skips the assert).
//
// Called from PhysicsSimulationIO::OutputBuffer::Construct. Thin explicit instantiation only (the
// shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just Construct is in this
// TU's ledger (n_funcs == 1, all bodied here).
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutJointSpy, 64>::Construct();
