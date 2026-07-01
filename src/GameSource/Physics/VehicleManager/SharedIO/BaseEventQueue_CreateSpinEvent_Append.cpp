#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::CreateSpinEvent (48-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateSpinEvent>::Append @ 0x827A79C8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// element stride is 48 bytes (0x30): Append asserts mpEvents!=NULL, no-overflow and the source
// owns a buffer, then XMemCpy's 48*srcLen bytes onto the tail (count = (srcLen*3)<<4 == 48*srcLen;
// dest = *mpEvents + 48*miLength) and advances miLength. sizeof(CreateSpinEvent) == 48.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateSpinEvent>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::CreateSpinEvent>&);
