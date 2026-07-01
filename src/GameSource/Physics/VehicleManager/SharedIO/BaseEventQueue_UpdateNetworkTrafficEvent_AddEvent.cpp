#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::UpdateNetworkTrafficEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::UpdateNetworkTrafficEvent>::AddEvent  @ 0x827C2950
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic AddEvent body is already inline in
// CgsBaseEventQueue.h; this is the thin explicit instantiation. Called by
// CrashModule::HandleNetworkCrashingTraffic. The X360 stores at an 80-byte stride (slwi r8,r10,2 +
// add (miLength*5), slwi r10,r10,4 (*16 => miLength*80)); the element body is one std (the 8-byte
// VolumeInstanceId head) + four 16-byte VMX block stores (the 64-byte Matrix44Affine), so
// sizeof(UpdateNetworkTrafficEvent) == VolumeInstanceId(8) + [8 pad] + Matrix44Affine(64) == 80.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::UpdateNetworkTrafficEvent>::AddEvent(
    const BrnPhysics::Vehicle::UpdateNetworkTrafficEvent&);
