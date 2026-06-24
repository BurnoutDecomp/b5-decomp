#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InAddRigidBody (192-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody>::{AddEvent,Append}
//   AddEvent @ X360 0x825A3000, Append @ X360 0x825A3898
//   (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddRigidBody>", funcs: 2).
//
// The generic AddEvent / Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations. Both X360 bodies match the generic store-for-store.
//
// AddEvent (@0x825A3000) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL  (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r29)` vs
//     `lwz r10,4(r29)`, blt skips) -- tripwire only; the copy below always runs;
//   * memcpy(mpEvents + miLength, &lEvent, 192) at a 192-byte stride: `li r5,0xC0` (Size==192),
//     `slwi r9,r11,1; add r11,r11,r9` (miLength*3), `slwi r11,r11,6` (*64 == miLength*192);
//     bumps miLength (`stw r11,8(r29)`); returns 1.
//
// Append (@0x825A3898) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", ble skips);
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy at the same 192-byte stride (`slwi r9,r29,1; add r9,r29,r9; slwi r5,r9,6` ==
//     count*192; matching dest offset); bumps miLength.
//
// The 192-byte stride matches sizeof(InAddRigidBody) == 192 (X360-attested off this very pair, see
// CgsPhysicsSimulationIO_Events.h). This is the AddEvent/Append pair of the EventQueue<InAddRigidBody,N>
// family whose Construct instantiations live in EventQueue_InAddRigidBody_{1,50,200}.cpp. AddEvent has
// many callers (PrepareWorldRigidBody, PhysicalTrafficManager::SendCreateRemoveTrafficEvents,
// PhysicalBodyPart/PhysicalWheel::AddToSim, VehicleManager::ProcessCreateEvents,
// PropManager::AddPropToSim, ...); Append from InputBuffer::AppendAddRigidBodyQueue and
// BrnPhysics::PhysicsModule::Update.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::InAddRigidBody&);

template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody>::Append(
    const CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody>&);
