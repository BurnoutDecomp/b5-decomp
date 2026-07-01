#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::AddEvent / ::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::CreateRaceCarEvent (160-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>::AddEvent @ 0x822C77A8
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>::Append   @ 0x823C2E78
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// AddEvent asserts mpEvents!=NULL and GetLength()<GetMaxLength() ("Reached Max length"), reserves
// &mpEvents[miLength] (160*miLength + mpEvents), assigns via CreateRaceCarEvent::operator= and
// increments miLength. Append asserts + XMemCpy(160*miLength+mpEvents, src.mpEvents, 160*src.miLength).
// The 160-byte (0xA0) element stride is attested by the index math (slwi-by-2;add;slwi-by-5 => x160)
// == sizeof(CreateRaceCarEvent) (miCarStrengthStat @0x98, alignas(16) => 160).
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>::AddEvent(
    const BrnPhysics::Vehicle::CreateRaceCarEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateRaceCarEvent>&);
