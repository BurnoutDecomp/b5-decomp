#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody (112-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody>::Append
//   @ X360 0x825A41D8 (dossier id "class:CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 112-byte (0x70) element stride -- `mulli r5,r29,0x70` (count == lSource.miLength * 112)
//     and `mulli r11,r11,0x70` (dest offset == this->miLength * 112), `add r3,r11,r10` == mpEvents
//     + miLength*112; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 112-byte stride matches sizeof(InUpdateExternalBody) == 112 (the X360-attested stride for this
// event type read directly off this very Append, see CgsPhysicsSimulationIO_Events.h). This is the
// Append member of the EventQueue<InUpdateExternalBody,N> family whose Construct instantiations live
// in EventQueue_InUpdateExternalBody_{60,200}.cpp; called from
// EventQueue<InUpdateExternalBody>::InputBuffer::AppendUpdateExternalBodyQueue.
template bool
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody>::Append(
    const CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody>&);
