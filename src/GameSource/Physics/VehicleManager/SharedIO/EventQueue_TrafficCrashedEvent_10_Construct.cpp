#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent, 10>::Construct @ 0x822E3130
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (10) event-queue instantiation:
// points the base queue at its inline maEvents buffer (this+0x10), sets the max length (10) and
// clears the live count. The generic EventQueue<T,N>::Construct / BaseEventQueue<T>::Construct
// bodies are already inline in CgsEventQueue.h / CgsBaseEventQueue.h (incl. the
// lpEventBuffer != NULL assert @:160). Buffer offset 0x10 == BaseEventQueue header padded to 16
// by the 16-byte-aligned element. Element stride 16 (TrafficCrashedEvent = VolumeInstanceId(u64,
// 8-byte aligned) + EntityId(4) padded to 16), already committed in BrnVehicleEvents.h.
// The FineTrafficCrashedEventQueue member of VehicleManagerOutputInterface.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent, 10>::Construct();
