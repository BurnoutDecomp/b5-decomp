#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"

// BrnPhysics::Vehicle::DebugComponent - the one leaf ledger function homed by the
// Vehicle-physics group (GetPath @0x825DB0D0). The rest of the component's API is owned by a
// separate dev-UI pass.

namespace BrnPhysics
{
namespace Vehicle
{
    // @0x825DB0D0  BrnPhysics::Vehicle::DebugComponent::GetPath
    //   lis r11,aPhysics@ha ; addi r3,r11,aPhysics@l "Physics" ; blr
    const char* DebugComponent::GetPath() const
    {
        return "Physics";
    }
}
}
