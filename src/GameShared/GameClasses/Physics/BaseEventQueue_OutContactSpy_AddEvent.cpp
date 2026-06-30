#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutContactSpy (112-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutContactSpy>::AddEvent
//   @ X360 0x825E44C8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::OutContactSpy>").
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin
// explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL ("mpEvents != NULL" tripwire, `lwz r11,0(r29)` (mpEvents @ +0);
//     bne skips the assert);
//   * asserts miLength < miMaxLength ("Reached Max length" tripwire, `lwz r11,8(r29)`
//     (miLength @ +8) vs `lwz r10,4(r29)` (miMaxLength @ +4), blt skips) -- a non-gating
//     tripwire only; the copy below always runs (AddEvent's unconditional-append semantics,
//     unlike the bounds-gated AddEventSafe @ 0x828A1B78 which has a `bge => li r3,0` early-out);
//   * copies the element to mpEvents[miLength] at a 112-byte (0x70) stride
//     (`mulli r10,r10,0x70` == miLength*112, `add r10,r10,r8` with r8 = mpEvents base;
//     14 doubleword ld/std block moves == 112 bytes, `li r9,0xE` / `mtctr r9`),
//     bumps miLength (`stw r11,8(r29)`) and returns 1.
//
// The 112-byte stride is X360-attested directly off this function's asm (the `mulli ...,0x70`
// literal and the 14x8-byte copy loop), and independently off the sibling AddEventSafe
// @ 0x828A1B78 in the same dossier (same 0x70 mulli + 14-doubleword copy). OutContactSpy is now
// sized to 112 bytes (see CgsPhysicsSimulationIO_Events.h), queued with capacity 800 in
// PhysicsSimulationIO::OutputBuffer (EventQueue<OutContactSpy,800>::Construct @ 0x828A68B8).
// AddEvent is called from BrnPhysics::Deformation::DeformationSensor::OutputContactSpy.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutContactSpy>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::OutContactSpy&);
