#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::PhysicalTrafficState (816-byte element)

// CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>::Append @ 0x8236A090
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::Append body is
// already inline in CgsBaseEventQueue.h; this is the thin explicit instantiation. The X360
// element stride is 816 bytes (0x330) -- matched exactly:
//   Append: asserts mpEvents!=NULL (CgsBaseEventQueue.h:413), no-overflow (:414) and the source
//     owns a buffer (:486), then XMemCpy's 816 * srcLen bytes (`mulli r5, srcLen, 0x330`;
//     `mulli r11, miLength, 0x330`) onto the tail and advances miLength.
// The 816-byte stride matches sizeof(PhysicalTrafficState) == WheelLite[4](448) + Matrix44Affine
// (64) + Vector3Plus(16) + Vector3(16) + Matrix44Affine[4](256) + scalar tail(16) == 816,
// already a multiple of 16 (alignas(16)). The Hex-Rays double a3..a6 params are spurious FP
// register reads -- the asm uses only r3 (this) / r4 (source queue), matching Append's signature.
template bool
CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>::Append(
    const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState>&);
