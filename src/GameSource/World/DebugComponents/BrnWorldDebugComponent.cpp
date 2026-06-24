#include "GameSource/World/DebugComponents/BrnWorldDebugComponent.h"

// BrnWorld::WorldDebugComponent - the one leaf ledger function homed by the Vehicle-physics group
// (GetName @0x827DD1F0). The rest of the component's API is owned by a separate dev-UI pass.

namespace BrnWorld
{
    // @0x827DD1F0  BrnWorld::WorldDebugComponent::GetName
    //   lis r11,aWorldModule@ha ; addi r3,r11,aWorldModule@l "World Module" ; blr
    const char* WorldDebugComponent::GetName() const
    {
        return "World Module";
    }
}
