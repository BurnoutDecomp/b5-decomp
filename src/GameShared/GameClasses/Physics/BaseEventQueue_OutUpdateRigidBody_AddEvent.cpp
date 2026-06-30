#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody (192-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody>::AddEvent
//   @ X360 0x828A66F8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody>", 1 func).
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin
// explicit instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL  (CgsBaseEventQueue.h, mpEvents at +0, `lwz r11,0(r29)`; bne skips)
//     -- non-gating tripwire;
//   * miLength(+8) < miMaxLength(+4) (`lwz r11,8(r29)` vs `lwz r10,4(r29)`, blt skips)
//     -- non-gating tripwire; the copy below always runs;
//   * element copy at a 192-byte stride: `slwi r9,r11,1; add r11,r11,r9; slwi r11,r11,6`
//     == (miLength*3)<<6 == miLength*192, added to mpEvents (`add r11,r11,r10`). The compiler
//     split the element copy-assign into a leading 8-byte `ld/std` (element bytes 0..7) plus a
//     delegated rw::physics::RigidBody::operator=(dest+0x10, src+0x10) for the remainder; net
//     effect is a full 192-byte element copy at stride 192 -- exactly the generic
//     `mpEvents[miLength] = lEvent;`.
//   * bumps miLength (`stw r8,8(r29)`, r8 = miLength+1, computed at 0x828A6814 addi r8,r11,1);
//     returns 1 (`li r3,1`).
//
// Member offsets read off the asm: mpEvents +0, miMaxLength +4, miLength +8 -- matches the
// committed CgsModule::BaseEventQueue<T> layout. The 192-byte stride matches
// sizeof(OutUpdateRigidBody) == 192 (X360-attested off this very function via the *192 index
// math; see CgsPhysicsSimulationIO_Events.h). Distinct from the input-side InUpdateRigidBody --
// do NOT conflate. Called by CgsPhysics::PhysicsSimulationModule::AddActiveBodiesToOutputQueue
// and CgsPhysics::PhysicsSimulationModule::ActivateAndFreezeAsNeeded.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody&);
