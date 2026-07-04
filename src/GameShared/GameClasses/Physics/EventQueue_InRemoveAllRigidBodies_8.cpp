#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveAllRigidBodies, 8>::Construct
//   @ X360 0x828A6148 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveAllRigidBodies,8>").
//
// Fixed-capacity (8) event-queue instantiation: the derived EventQueue<T,8>::Construct points
// the BaseEventQueue<T> base at its inline maEvents buffer, stores the max length (8, the asm's
// `li r11,8; stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The
// lpEventBuffer != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by
// BaseEventQueue<T>::Construct (always true: &maEvents[0] == this + 0x0C).
//
// NOTE the buffer offset: unlike the 16-byte-aligned sibling queues (whose element pads the
// 12-byte base to +0x10, the asm's `addi r30, r31, 0x10`), this element is a single uint8_t,
// so the base is NOT padded and maEvents starts at this + 0x0C -- exactly the asm's
// `addi r30, r31, 0xC`. The element InRemoveAllRigidBodies is fully DWARF-attested
// (CgsPhysicsSimulationModuleIO.h:154/156: { uint8_t mu8OwnerId; } over an empty Event base ==
// sizeof 1, align 1); its struct home is CgsPhysicsSimulationIO_Events.h.
//
// This capacity-8 remove-all queue is PhysicsSimulationIO::InputBuffer's
// mRemoveAllRigidBodiesQueue (X360 caller
// CgsPhysics::PhysicsSimulationIO::InputBuffer::Construct @ 0x828A71B8). Thin explicit
// instantiation only (the shared generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h);
// just Construct is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveAllRigidBodies, 8>::Construct();
