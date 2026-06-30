#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody (192-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody>::AddEvent
//   @ X360 0x82614928 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody>").
//
// The generic AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL tripwire (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips);
//   * miLength < miMaxLength tripwire (CgsBaseEventQueue.h:313 "...Reached Max length", `lwz r11,8(r29)`
//     vs `lwz r10,4(r29)`, blt skips) -- non-gating; the copy below always runs;
//   * the element is copied at index miLength via the type's assignment: `std r10,0(r11)` stores the
//     first 8 bytes (the Event base), then `rw::physics::RigidBody::operator_(v13+0x10, a2+0x10)`
//     copies the remaining bytes through RigidBody's own assignment operator -- net effect is a full
//     192-byte element assignment;
//   * index math attests the element STRIDE: `slwi r9,r11,1; add r11,r11,r9; slwi r11,r11,6` ==
//     miLength*3*64 == miLength*192 (192-byte stride), confirmed by the Hex-Rays `v13 = (192*v11 + *a1)`;
//   * bumps miLength (`stw r8,8(r29)`); returns 1 (true) unconditionally.
//
// This 192-byte stride is X360-ATTESTED off THIS function (same convention as InAddRigidBody's
// AddEvent @0x825A3000 pins 192 for that type); it supersedes the prior "stride NOT recovered,
// sized to 16B alignment only" comment on InUpdateRigidBody in CgsPhysicsSimulationIO_Events.h,
// where macOpaquePayload is now 192 bytes. This is the AddEvent of the
// EventQueue<InUpdateRigidBody,200> family whose Construct instantiation lives at X360 0x828A5FF8
// (capacity 200, PhysicsSimulationIO::InputBuffer). Only caller in scope:
// BrnPhysics::Props::PropManager::ClampAcceleration.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::InUpdateRigidBody&);
