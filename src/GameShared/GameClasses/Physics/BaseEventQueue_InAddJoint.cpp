#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InAddJoint (192-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint>::{AddEvent,Append}
//   AddEvent @ X360 0x825E40C0, Append @ X360 0x825A3A68
//   (dossier id "class:CgsPhysics::PhysicsSimulationIO::InAddJoint>", funcs: 2).
//
// The generic AddEvent / Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations. Both X360 bodies match the generic store-for-store.
//
// AddEvent (@0x825E40C0) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL  (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r29)` vs
//     `lwz r10,4(r29)`, blt skips) -- tripwire only; the copy below always runs;
//   * memcpy(mpEvents + miLength, &lEvent, 192) at a 192-byte stride: `li r5,0xC0` (Size==192),
//     `slwi r9,r11,1; add r11,r11,r9` (miLength*3), `slwi r11,r11,6` (*64 == miLength*192),
//     `add r3,r11,r10` (== mpEvents + miLength*192); bumps miLength (`stw r11,8(r29)`); returns 1.
//
// Append (@0x825A3A68) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", lSource.miLength +
//     this->miLength vs this->miMaxLength, ble skips);
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + miLength, lSource.mpEvents, sizeof(T)*lSource.miLength) at the same
//     192-byte stride (`slwi/add` == miLength*192 for both count and dest offset); bumps miLength.
//
// The 192-byte stride matches sizeof(InAddJoint) == 192 (X360-attested, see
// CgsPhysicsSimulationIO_Events.h). This is the AddEvent/Append pair of the EventQueue<InAddJoint,N>
// family whose Construct instantiations live in EventQueue_InAddJoint_{10}.cpp (+ capacity-36 in the
// input buffer). AddEvent is called from BrnPhysics::Vehicle::VehicleOutputRequestInterface::AddJoint;
// Append from InputBuffer::AppendAddJointQueue and BrnPhysics::PhysicsModule::Update.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::InAddJoint&);

template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint>::Append(
    const CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint>&);
