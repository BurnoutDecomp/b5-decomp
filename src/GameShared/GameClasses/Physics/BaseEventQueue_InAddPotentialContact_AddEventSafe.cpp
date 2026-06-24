#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InAddPotentialContact (80-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact>::AddEventSafe
//   @ X360 0x8259D308 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddPotentialContact>").
//
// The generic BaseEventQueue<T>::AddEventSafe body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:331 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength) vs `lwz r10,4(r31)` (miMaxLength),
//     `bge` => return 0 WITHOUT appending when miLength >= miMaxLength;
//   * otherwise copies the 80-byte element to mpEvents[miLength] at an 80-byte stride
//     (`slwi r9,r11,2; add r11,r11,r9; slwi r11,r11,4` == miLength*80; ctr=10 std/ld 64-bit
//     block moves == 80 bytes), bumps miLength (`stw r11,8(r31)`) and returns 1.
// The 80-byte stride matches sizeof(InAddPotentialContact) == 80 (the X360-attested stride
// for this event type, see CgsPhysicsSimulationIO_Events.h). This is the AddEventSafe member
// of the same EventQueue<InAddPotentialContact,1024> instance whose Construct lives in
// EventQueue_InAddPotentialContact_1024.cpp; called from the Bridge*ContactsToSimulation paths.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddPotentialContact>::AddEventSafe(
    const CgsPhysics::PhysicsSimulationIO::InAddPotentialContact&);
