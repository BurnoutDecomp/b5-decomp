#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append/AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::CreateArticulatedTrafficEvent (272-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateArticulatedTrafficEvent>::AddEvent @ 0x82719D70
// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateArticulatedTrafficEvent>::Append   @ 0x823C3598
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent / ::Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
// The X360 element stride is 272 bytes (0x110): AddEvent computes the slot as 0x110*miLength +
// mpEvents (mulli 0x110) then does a CreateArticulatedTrafficEvent::operator= fill; Append's
// XMemCpy copies 0x110*count bytes. sizeof(element) == 0x110 on the X360 spine.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateArticulatedTrafficEvent>::AddEvent(const BrnPhysics::Vehicle::CreateArticulatedTrafficEvent&);

template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateArticulatedTrafficEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateArticulatedTrafficEvent>&);
