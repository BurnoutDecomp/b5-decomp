#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"        // BrnPhysics::Deformation::RemoveDeformationModelEvent

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::RemoveDeformationModelEvent>::AddEvent
// The generic BaseEventQueue<T>::AddEvent body is inline in CgsBaseEventQueue.h; this is the thin
// explicit instantiation. The console body appends UNCONDITIONALLY (both asserts are non-gating
// tripwires):
//   - assert mpEvents != NULL (CgsBaseEventQueue.h)
//   - the overflow tripwire (miLength >= miMaxLength) streams "CgsModule::BaseEventQueue<class
//     BrnPhysics::Deformation::RemoveDeformationModelEvent>::AddEvent\nReached Max length
//     <miMaxLength>\n" into the assert buffer and fires it
//   - copies the element (one 64-bit RigidBodyId, the whole console record) into
//     mpEvents[miLength], then ++miLength, returns true.
// The element is indexed by the host sizeof of the record, the same T the owning
// EventQueue<RemoveDeformationModelEvent, 20> buffer is declared with. Callers:
// PhysicalTrafficManager::PhysicallyUncrashTrafficCar / SendCreateRemoveTrafficEvents and
// VehicleManager::ProcessRemoveEvents.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::RemoveDeformationModelEvent>::AddEvent(
    const BrnPhysics::Deformation::RemoveDeformationModelEvent&);
