#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InRemoveJoint (8-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint>::{AddEvent,Append}
//   AddEvent @ X360 0x825E4208, Append @ X360 0x825A3B58
//   (dossier id "class:CgsPhysics::PhysicsSimulationIO::InRemoveJoint>", funcs: 2).
//
// The generic AddEvent / Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations. Both X360 bodies match the generic store-for-store.
//
// AddEvent (@0x825E4208) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL  (CgsBaseEventQueue.h:312, `lwz r11,0(r28)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r28)` vs
//     `lwz r10,4(r28)`, blt skips) -- tripwire only; the store below always runs;
//   * stores the 8-byte element to mpEvents[miLength] as a SINGLE 64-bit store: `ld r10,0(r26)`
//     (the event value), `slwi r11,r11,3` (miLength*8), `stdx r10,r11,r9` (== *(mpEvents +
//     miLength*8)); bumps miLength (`addi r11,r11,1; stw r11,8(r28)`); returns 1. The 8-byte stride
//     == sizeof(InRemoveJoint) (the X360-attested element size for this type).
//
// Append (@0x825A3B58) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", lSource.miLength +
//     this->miLength vs this->miMaxLength, ble skips) -- NON-gating, the copy always runs;
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + miLength, lSource.mpEvents, sizeof(T)*lSource.miLength) at the same 8-byte
//     stride (`slwi r5,r29,3` == lSource.miLength*8 == count; `slwi r11,r11,3` == this->miLength*8 ==
//     dest offset; `add r3,r11,r10` == mpEvents + miLength*8), modelled as std::memcpy; bumps
//     this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`); returns 1.
//
// The 8-byte stride matches sizeof(InRemoveJoint) == 8 (X360-attested, see
// CgsPhysicsSimulationIO_Events.h). AddEvent is called from
// BrnPhysics::Vehicle::ArticulatedJointPool::SendCreateRemoveJointEvents; Append from
// InputBuffer::AppendRemoveJointQueue and BrnPhysics::PhysicsModule::Update.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint>::AddEvent(
    const CgsPhysics::PhysicsSimulationIO::InRemoveJoint&);

template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint>::Append(
    const CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveJoint>&);
