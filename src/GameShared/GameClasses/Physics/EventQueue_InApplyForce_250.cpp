#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InApplyForce, 250>::Construct
//   @ X360 0x828A6068 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InApplyForce,250>").
//
// Fixed-capacity (250) event-queue instantiation: the derived EventQueue<T,250>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes, padded
// to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (250 == 0xFA, the asm's `li r11,0xFA;
// stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer
// != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct
// (always true: &maEvents[0] == this + 0x10).
//
// This capacity-250 apply-force queue lives in PhysicsSimulationIO::InputBuffer (X360 caller
// CgsPhysics::PhysicsSimulationIO::InputBuffer::Construct). Thin explicit instantiation only
// (the shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just Construct
// is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InApplyForce, 250>::Construct();
