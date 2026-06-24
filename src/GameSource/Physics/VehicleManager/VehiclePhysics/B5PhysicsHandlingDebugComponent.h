#pragma once

#include "types.hpp"
#include "DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)

// BrnPhysics::Vehicle::DebugComponent - the in-game handling/grip-curve debug menu for the
// vehicle physics. Derives from the real CgsDev::DebugComponent. The full component (the
// PrimitiveVehicleDebugRender2D / GearStats / GripCurveDebugGraph / GripCurveDebugWindow
// helper classes and the render/update/window machinery declared in the DWARF) is owned by
// its own dev-UI pass. Incremental: this TU implements ONLY the leaf path getter
// (GetPath @0x825DB0D0). It is declared here BY NAME so the body has a real .cpp home.

namespace BrnPhysics
{
namespace Vehicle
{
    class DebugComponent : public CgsDev::DebugComponent
    {
    protected:
        // @0x825DB0D0: the debug-menu path under which this component is grouped.
        //   asm: lis r11,aPhysics@ha ; addi r3,r11,aPhysics@l "Physics" ; blr
        const char* GetPath() const override;
    };
}
}
