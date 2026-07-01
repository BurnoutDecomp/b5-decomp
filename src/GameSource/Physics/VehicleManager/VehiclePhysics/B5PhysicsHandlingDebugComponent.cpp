#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Vehicle::DebugComponent - the leaf ledger functions homed by the Vehicle-physics
// group (GetPath @0x825DB0D0, SetLastWallTriangle @0x825B4D60). The rest of the component's API is
// owned by a separate dev-UI pass.

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

    // @0x825B4D60  BrnPhysics::Vehicle::DebugComponent::SetLastWallTriangle
    //   Record the collision triangle of the last wall the car scraped (for the
    //   DrawLastWallTriangle debug overlay). The asm asserts lpTriangle != NULL then copies the
    //   whole 80-byte AOSTriangle (10 qwords, ld/std loop) into mLastWallTriangle @ this+0x350.
    //   sizeof(CgsGeometric::Triangle4::AOSTriangle) == 0x50 == 80, so the 10-qword copy is
    //   exactly the member assign.
    void DebugComponent::SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle)
    {
        CGS_ASSERT(lpTriangle != nullptr, "lpTriangle != NULL");

        mLastWallTriangle = *lpTriangle;   // 10-qword (80-byte) AOSTriangle copy into this+0x350
    }
}
}
