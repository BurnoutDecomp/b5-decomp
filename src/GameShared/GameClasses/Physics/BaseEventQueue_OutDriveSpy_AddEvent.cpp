#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutDriveSpy (64-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutDriveSpy>::AddEvent
//   @ X360 0x828A1D90 (dossier id "class:CgsPhysics::PhysicsSimulationIO::OutDriveSpy>").
//
// The generic BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store -- it appends
// UNCONDITIONALLY (the two asserts are non-gating tripwires, not a bounds gate):
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:312 "mpEvents != NULL" tripwire, `lwz r11,0(r29)`;
//     bne skips the assert);
//   * asserts miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length" tripwire,
//     `lwz r11,8(r29)` (miLength) vs `lwz r10,4(r29)` (miMaxLength), `blt` skips the assert) -- this is
//     a tripwire only; the copy below runs regardless;
//   * copies the 64-byte element to mpEvents[miLength] at a 64-byte stride (`slwi r10,r10,6`
//     == miLength*64, then `add r10,r10,r8` == + mpEvents; ctr = 8 ld/std 64-bit block moves
//     == 64 bytes), bumps miLength (`addi r11,r11,1; stw r11,8(r29)`) and returns 1.
// The 64-byte stride matches sizeof(OutDriveSpy) == 64 (the X360-attested stride for this event type,
// see CgsPhysicsSimulationIO_Events.h). This is the AddEvent member of the same EventQueue<OutDriveSpy,1>
// instance whose Construct lives in EventQueue_OutDriveSpy_1.cpp; called from
// PhysicsSimulationModule::AddDriveSpiesToOutputQueue.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutDriveSpy>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::OutDriveSpy&);
