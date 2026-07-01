#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::AddEvent / ::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::CreatePhysicalTrafficEvent (144-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreatePhysicalTrafficEvent>::AddEvent @ 0x82719C30
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreatePhysicalTrafficEvent>::Append   @ 0x823C34A8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// AddEvent asserts mpEvents!=NULL and GetLength()<GetMaxLength() ("Reached Max length"), reserves
// &mpEvents[miLength] (144*miLength + mpEvents), copy-constructs the passed event and increments
// miLength. Append asserts + XMemCpy(144*miLength+mpEvents, src.mpEvents, 144*src.miLength).
// The 144-byte (0x90) element stride is attested by the index math (slwi-by-3;add;slwi-by-4 => x144)
// == sizeof(CreatePhysicalTrafficEvent) (mCgsID @136, alignas(16) => 144).
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreatePhysicalTrafficEvent>::AddEvent(
    const BrnPhysics::Vehicle::CreatePhysicalTrafficEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreatePhysicalTrafficEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreatePhysicalTrafficEvent>&);
