#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.h"

// BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent - the one leaf ledger function homed
// by the Vehicle-physics group (GetName @0x827DBA50). The rest of the component's render API is
// owned by a separate dev-UI pass.

namespace BrnPhysics
{
namespace Vehicle
{
    // @0x827DBA50  BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent::GetName
    //   lis r11,aPhysicalTraffi@ha ; addi r3,r11,aPhysicalTraffi@l "Physical traffic" ; blr
    const char* PhysicalTrafficManagerDebugComponent::GetName() const
    {
        return "Physical traffic";
    }
}
}
