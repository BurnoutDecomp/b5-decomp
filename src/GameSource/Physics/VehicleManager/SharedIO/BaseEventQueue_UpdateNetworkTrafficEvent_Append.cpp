#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::UpdateNetworkTrafficEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::UpdateNetworkTrafficEvent>::Append  @ 0x823C3838
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Append body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// VehicleInputInterface::Append and VehicleInputInterface::operator=. XMemCpy copies
// 80*src.miLength bytes onto the tail at an 80-byte stride (the *5 then *16 index math on both the
// count and the dest offset), cross-confirmed by the sibling AddEvent @0x827C2950.
// sizeof(UpdateNetworkTrafficEvent) == VolumeInstanceId(8) + [8 pad] + Matrix44Affine(64) == 80.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::UpdateNetworkTrafficEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::UpdateNetworkTrafficEvent>&);
