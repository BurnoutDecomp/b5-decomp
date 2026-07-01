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

    // Wave-7 additions: event-queue element strides observed in the X360 Construct/AddEvent/Append copies.
    static_assert(sizeof(CreateAirRamEvent)           == 64, "CreateAirRamEvent stride must be 64 bytes (8-QWORD copy / slwi-by-6)");
    static_assert(sizeof(CreateSpinEvent)             == 48, "CreateSpinEvent stride must be 48 bytes (48*count XMemCpy)");
    static_assert(sizeof(CreateVehicleResult)         == 16, "CreateVehicleResult stride must be 16 bytes (2-QWORD copy / slwi-by-4)");
    static_assert(sizeof(CreateWorldEvent)            == 80, "CreateWorldEvent stride must be 80 bytes (VolumeInstanceId + Matrix44Affine, alignas16)");
    static_assert(sizeof(RaceCarResetEvent)           == 32, "RaceCarResetEvent stride must be 32 bytes (4-QWORD copy / slwi-by-5)");

    // GripCurveDebugGraph: the offsets GetOrigin/GetPointOnGraph read (+0x10/+0x20/+0x30).
    static_assert(sizeof(rw::math::vpu::Vector2) == 16, "rw vpu Vector2 must be a 16-byte SIMD register");
}
