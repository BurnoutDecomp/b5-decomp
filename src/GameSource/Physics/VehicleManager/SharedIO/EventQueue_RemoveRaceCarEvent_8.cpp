#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent, 8>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent, 8>::Construct();

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent>::AddEvent @ 0x822C78E8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent>::Append   @ 0x823C2F68
// The generic BaseEventQueue<T>::AddEvent / ::Append bodies are already inline in
// CgsBaseEventQueue.h; these are the thin explicit instantiations. The X360 element stride
// is 8 bytes: AddEvent's element assignment is a single 8-byte store (ld/stdx) into slot
// (count<<3 + *mpEvents); Append's XMemCpy copies 8*count bytes.
// sizeof(RemoveRaceCarEvent) == VolumeInstanceId(8) == 8, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent>::AddEvent(const BrnPhysics::Vehicle::RemoveRaceCarEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveRaceCarEvent>&);
