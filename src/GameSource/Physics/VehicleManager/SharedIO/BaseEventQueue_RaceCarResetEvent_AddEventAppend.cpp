#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::RaceCarResetEvent (32-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarResetEvent>::AddEvent @ 0x825E4D70
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarResetEvent>::Append   @ 0x827A7568
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 32 bytes: AddEvent's element assignment is the
// four-QWORD copy (four ld/std at +0/+8/+0x10/+0x18) into slot (count<<5 + *mpEvents), and
// Append's XMemCpy copies 32*count bytes. sizeof(RaceCarResetEvent) == EActiveRaceCarIndex(4)
// + bool(1) + pad + Vector3(16 @ offset 16) == 32, matching.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarResetEvent>::AddEvent(const BrnPhysics::Vehicle::RaceCarResetEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarResetEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarResetEvent>&);
