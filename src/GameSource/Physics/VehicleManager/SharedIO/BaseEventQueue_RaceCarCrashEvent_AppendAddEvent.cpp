#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // CgsModule::BaseEventQueue<T>::AddEvent / ::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::RaceCarCrashEvent (64-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>::AddEvent @ 0x825E4C18
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>::Append   @ 0x82369FB0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// Element stride is 64 bytes, attested two ways: AddEvent slwi r10,r10,6 (64*miLength index) + an
// eight-QWORD copy loop; Append slwi-by-6 count and dest (XMemCpy(mpEvents + 64*miLength,
// src.mpEvents, 64*src.miLength)). sizeof(RaceCarCrashEvent) == 64 per its committed alignas(16)
// layout in BrnVehicleEvents.h. AddEvent called from VehicleManagerOutputInterface::AddRaceCarCrashEvent;
// Append from GameStateModule / VehicleManagerOutputInterface.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>::AddEvent(
    const BrnPhysics::Vehicle::RaceCarCrashEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>&);
