#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody, 200>::Construct
//   @ X360 0x825A7B98 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody,200>").
//
// Fixed-capacity (200) event-queue instantiation: the derived EventQueue<T,200>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes, padded to
// the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (200, the asm's `li r11,0xC8; stw r11,4(r31)`)
// and clears the live count (`li r10,0; stw r10,8(r31)`). The lpEventBuffer != NULL tripwire
// (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct (always true:
// &maEvents[0] == this + 0x10).
//
// This is the 200-capacity member of the three InRemoveRigidBody queue capacities (1 / 50 / 200)
// wired into the PhysicsSimulationIO input/output buffers; called from
// PhysicsSimulationIO::InputBuffer::Construct and BrnPhysics::Props::PropOutputInterface::Construct.
// The element InRemoveRigidBody is sized to its X360-attested 16-byte stride (matching
// BaseEventQueue<InRemoveRigidBody>::AddEvent @0x825E3ED8 `slwi r11,r11,4` and Append @0x825A3988);
// its struct home is CgsPhysicsSimulationIO_Events.h. Thin explicit instantiation only (the shared
// generic bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just Construct is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody, 200>::Construct();
