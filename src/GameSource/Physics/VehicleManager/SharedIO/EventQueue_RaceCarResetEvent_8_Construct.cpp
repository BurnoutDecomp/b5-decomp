#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarResetEvent, 8>::Construct @ 0x822E3210
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue instantiation:
// points the base queue at its inline maEvents buffer (this+0x10), sets the max length (8) and
// clears the live count. The generic EventQueue<T,N>::Construct / BaseEventQueue<T>::Construct
// bodies are already inline in CgsEventQueue.h / CgsBaseEventQueue.h (incl. the
// lpEventBuffer != NULL assert @:160). Element stride 32 (sizeof(RaceCarResetEvent) ==
// EActiveRaceCarIndex(4) + bool(1) + Vector3(16 @+16) == 32), matching the
// EventQueue<RaceCarResetEvent,8> footprint 0x5B0..0x6C0 (0x110 == 0x10 + 8*32) in
// VehicleManagerOutputInterface.
template void CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarResetEvent, 8>::Construct();
