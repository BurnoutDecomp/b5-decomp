#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"

// CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddDrive, 1>::Construct
//   @ X360 0x828A6458 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddDrive,1>").
//
// Fixed-capacity (1) event-queue instantiation: the derived EventQueue<T,1>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes,
// padded to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30, r31, 0x10`), stores the max length (1, the asm's `li r11,1; stw r11,4(r31)`)
// and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer != NULL tripwire
// (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct (always true:
// &maEvents[0] == this + 0x10).
//
// This queue is PhysicsSimulationIO::InputBuffer's InAddDriveQueue (X360
// InputBuffer::Construct @ 0x828A71B8 calls it at this+203472). Thin explicit
// instantiation only (the shared generic bodies live in CgsEventQueue.h /
// CgsBaseEventQueue.h); just Construct is in this TU's ledger.
template void CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddDrive, 1>::Construct();
