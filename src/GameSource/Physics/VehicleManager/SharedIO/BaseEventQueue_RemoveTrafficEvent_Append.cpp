#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // BrnPhysics::Vehicle::RemoveTrafficEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveTrafficEvent>::Append @ 0x823C3758
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// element stride is 8 bytes: Append's XMemCpy copies 8*count bytes (slwi-by-3) into mpEvents +
// 8*miLength. sizeof(RemoveTrafficEvent) == VolumeInstanceId(u64) == 8. Distinct from the
// TrafficCrashedEvent Append and the RemoveTrafficEvent/25 Construct (this 0x823C3758 Append is a
// separate ledger func).
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveTrafficEvent>::Append(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RemoveTrafficEvent>&);
