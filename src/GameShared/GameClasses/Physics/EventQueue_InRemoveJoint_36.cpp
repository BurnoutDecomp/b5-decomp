#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint, 36>::Construct
//   @ X360 0x828A6298 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveJoint,36>").
//
// Fixed-capacity (36) event-queue instantiation: the derived EventQueue<T,36>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (the 12-byte base pads to
// the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (36 == 0x24, the asm's `li r11,0x24;
// stw r11,4(r31)`) and clears the live count (`li r10,0; stw r10,8(r31)`). The lpEventBuffer
// != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct
// (always true: &maEvents[0] == this + 0x10).
//
// This capacity-36 remove-joint queue lives in PhysicsSimulationIO::InputBuffer (X360 caller
// CgsPhysics::PhysicsSimulationIO::InputBuffer::Construct). The InRemoveJoint element is sized
// to its X360-attested 8-byte stride (AddEvent @ 0x825E4208 stores each element with a single
// 64-bit `stdx`; Append @ 0x825A3B58 block-copies at the same 8-byte stride); its struct home
// is CgsPhysicsSimulationIO_Events.h. Thin explicit instantiation only (the shared generic
// bodies live in CgsEventQueue.h / CgsBaseEventQueue.h); just Construct is in this TU's ledger.
// Mirrors EventQueue_InSetJointSpy_36.cpp (0x828A63E8, adjacent, same capacity) and
// EventQueue_InRemoveRigidBody_50.cpp (0x825A7CE8), differing only in capacity + element type.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint, 36>::Construct();
