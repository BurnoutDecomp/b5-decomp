#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateJointFrames, 36>::Construct
//   @ X360 0x828A6308 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InUpdateJointFrames,36>").
//
// Fixed-capacity (36) event-queue instantiation: the derived EventQueue<T,36>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes, padded
// to the 16-byte element alignment -> the buffer lives at +0x10, matching the asm's
// `addi r30, r31, 0x10`), stores the max length (36 == 0x24, the asm's `li r11,0x24;
// stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer != NULL
// tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct (always
// true: &maEvents[0] == this + 0x10).
//
// Only Construct is in scope for this TU (no Append/AddEvent in scope to pin the event stride,
// and the InputBuffer::Construct offset map @ 0x828A71B8 that would pin it is not in scope
// either), so InUpdateJointFrames is sized only to the 16-byte alignment class the asm proves;
// see CgsPhysicsSimulationIO_Events.h. Thin explicit instantiation only (the shared generic
// bodies live in CgsEventQueue.h / CgsBaseEventQueue.h).
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateJointFrames, 36>::Construct();
