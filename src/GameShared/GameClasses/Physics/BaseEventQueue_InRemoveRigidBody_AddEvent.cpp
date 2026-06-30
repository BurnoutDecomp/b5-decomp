#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody (16-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>::AddEvent
//   @ X360 0x825E3ED8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>", funcs: 2;
//   this TU covers AddEvent only -- the sibling Append @ 0x825A3988 is a separate instantiation).
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL  (CgsBaseEventQueue.h, `lwz r11,0(r29)`; bne skips) -- tripwire only;
//   * miLength < miMaxLength ("Reached Max length", `lwz r11,8(r29)` vs `lwz r10,4(r29)`,
//     blt skips) -- tripwire only; the copy below always runs;
//   * store at a 16-byte stride: `lwz r11,8(r29)` (miLength), `slwi r11,r11,4` (miLength*16),
//     `add r11,r11,r10` (+mpEvents @0(r29)), then two 8-byte block stores
//     (`ld/std r10` from a2 @0/@8 == 16 bytes) -- i.e. sizeof(InRemoveRigidBody) == 16, a
//     genuinely X360-ATTESTED 16-byte stride (not just sized-to-alignment);
//   * bumps miLength (`lwz r11,8(r29)`/`addi r11,r11,1`/`stw r11,8(r29)`); returns 1 (`li r3,1`).
//
// Member offsets read from the asm: mpEvents @+0, miMaxLength @+4, miLength @+8 (consistent with
// the generic BaseEventQueue<T> layout in CgsBaseEventQueue.h).
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody&);
