#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

// BrnPhysics::Vehicle::PhysicalTrafficState copy constructor  @ 0x82284EE8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body is a pure bitwise copy of
// the whole 816-byte object: memcpy of the 448-byte WheelLite[4] head, VMX (lvx128/
// stvx128) 16-byte block copies across the matrices/vectors region [448..800), then the
// scalar tail (mEntityID u32 @800, mfSpeed f32 @804, three bools @808/809/810, mfSteering
// f32 @812). PhysicalTrafficState is trivially copyable, so `= default` reproduces the
// X360 memberwise copy exactly; kept out-of-line so this ledger func has a definition site.
namespace BrnPhysics
{
namespace Vehicle
{
    PhysicalTrafficState::PhysicalTrafficState( const PhysicalTrafficState& ) = default;
}
}
