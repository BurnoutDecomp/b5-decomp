#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutJointSpy (48-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutJointSpy>::AddEvent
//   @ X360 0x828A1C30 (dossier id "class:CgsPhysics::PhysicsSimulationIO::OutJointSpy>").
//
// The generic BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store -- it appends
// UNCONDITIONALLY (the two asserts are non-gating tripwires, not a bounds gate):
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:312 "mpEvents != NULL" tripwire, `lwz r11,0(r29)`;
//     bne skips the assert);
//   * asserts miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length" tripwire,
//     `lwz r11,8(r29)` (miLength) vs `lwz r10,4(r29)` (miMaxLength), `blt` skips the assert) -- this is
//     a tripwire only; the copy below runs regardless;
//   * copies the 48-byte element to mpEvents[miLength] at a 48-byte stride: `slwi r7,r11,1`
//     (miLength*2), `add r11,r11,r7` (miLength*3), `slwi r11,r11,4` (*16 == miLength*48),
//     `add r11,r11,r8` (== mpEvents + miLength*48); ctr == 6 ld/std 64-bit block moves == 48 bytes;
//     bumps miLength (`addi r11,r11,1; stw r11,8(r29)`) and returns 1.
// The 48-byte stride matches sizeof(OutJointSpy) == 48 (the X360-attested stride for this event type,
// see CgsPhysicsSimulationIO_Events.h). This is the AddEvent member of the same EventQueue<OutJointSpy,64>
// instance whose Construct lives in EventQueue_OutJointSpy_64.cpp; called from
// PhysicsSimulationModule::AddJointSpiesToOutputQueue.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutJointSpy>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::OutJointSpy&);
