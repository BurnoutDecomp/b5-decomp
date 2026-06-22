// Tiny embed/ODR check for the Vehicle-events group: includes the grown homes and the
// GripCurveDebugGraph home, and statically asserts the recovered element/struct sizes that
// the X360 event-queue copy strides depend on. Compile-only; not linked into the game.

#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h"

namespace
{
    using namespace BrnPhysics::Vehicle;

    // Event-queue element strides observed in the X360 AddEvent/Append copies.
    static_assert(sizeof(SetRaceCarCollisionEvent)    == 8,  "SetRaceCarCollisionEvent stride must be 8 bytes (2-_DWORD copy)");
    static_assert(sizeof(SetRaceCarCullingGroupEvent) == 8,  "SetRaceCarCullingGroupEvent stride must be 8 bytes (2-_DWORD copy)");
    static_assert(sizeof(TrafficSlammedEvent)         == 20, "TrafficSlammedEvent stride must be 20 bytes (5-_DWORD copy)");

    // GripCurveDebugGraph: the offsets GetOrigin/GetPointOnGraph read (+0x10/+0x20/+0x30).
    static_assert(sizeof(rw::math::vpu::Vector2) == 16, "rw vpu Vector2 must be a 16-byte SIMD register");
}
