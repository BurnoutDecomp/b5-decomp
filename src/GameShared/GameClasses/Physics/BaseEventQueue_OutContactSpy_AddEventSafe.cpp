#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutContactSpy (112-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutContactSpy>::AddEventSafe
//   @ X360 0x828A1B78 (dossier id "class:CgsPhysics::PhysicsSimulationIO::OutContactSpy>").
//
// The generic BaseEventQueue<T>::AddEventSafe body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne cr6 skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `cmpw`/`bge cr6` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 112-byte element to mpEvents[miLength] at a 112-byte stride
//     (`mulli r11,r11,0x70` == miLength*112; `add r10,r11,r10` adds mpEvents base; ctr=0xE==14
//     `ld`/`std` 64-bit block moves == 14*8 == 112 bytes), bumps miLength (`stw r11,8(r31)`)
//     and returns 1.
//
// The 112-byte (0x70) stride is X360-ATTESTED off this function (and its sibling
// BaseEventQueue<OutContactSpy>::AddEvent @ 0x825E44C8, which uses the identical
// `mulli ...,0x70` + ctr=14 ld/std copy), so sizeof(OutContactSpy) == 112 (the committed
// OutContactSpy payload in CgsPhysicsSimulationIO_Events.h has been grown from the placeholder
// 16-byte span to this attested 112-byte span; the Construct @ 0x828A68B8 stays store-for-store
// faithful since 112 is still 16-aligned). Called from
// CgsPhysics::PhysicsSimulationModule::AddContactSpiesToOutputQueue.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutContactSpy>::AddEventSafe(
    const CgsPhysics::PhysicsSimulationIO::OutContactSpy&);
