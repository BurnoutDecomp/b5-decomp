#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint, 10>::Construct
//   @ X360 0x825A7DC8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveJoint,10>").
//
// Fixed-capacity (10) event-queue instantiation: the derived EventQueue<T,10>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer. The EventQueue<T,N> is
// alignas(16), so its 12-byte BaseEventQueue base pads to +0x10 before maEvents, exactly the
// asm's `addi r30, r31, 0x10`. The body stores the max length (10 == 0xA, the asm's
// `li r11,0xA; stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`); the base
// pointer store is `stw r30,0(r31)` (mpEvents = &maEvents[0]). The lpEventBuffer != NULL
// tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct's
// CGS_ASSERT (always true: &maEvents[0] == this + 0x10).
//
// This capacity-10 remove-joint queue lives in the OutputBuffers (X360 callers
// BrnPhysics::PhysicsModuleIO::OutputBuffer::Construct and
// BrnPhysics::Vehicle::VehicleManagerOutputBuffer::Construct). The InRemoveJoint element is
// 8 bytes (empty Event base + u8[8] payload, no alignas), X360-attested via
// BaseEventQueue<InRemoveJoint>::AddEvent @0x825E4208 (stdx) and Append @0x825A3B58. Thin
// explicit instantiation only (the shared generic bodies live in CgsEventQueue.h /
// CgsBaseEventQueue.h); just Construct is in this TU's ledger. Mirrors sibling
// EventQueue_InAddJoint_10.cpp.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint, 10>::Construct();
