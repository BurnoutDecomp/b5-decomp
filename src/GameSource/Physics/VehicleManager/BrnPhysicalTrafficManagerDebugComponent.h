#pragma once

#include "types.hpp"
#include "DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)

// BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent - the in-game debug menu/overlay for
// the physical traffic manager (air-ram / driver-control / joint / catchup / reset-on-water
// visualisations). Derives from the real CgsDev::DebugComponent. The full component (the many
// RenderWorld/DrawXxx render helpers declared in the DecFIGS DWARF) is owned by its own dev-UI
// pass. Incremental: this TU implements ONLY the leaf name getter (GetName @0x827DBA50). It is
// declared here BY NAME so the body has a real .cpp home.

namespace BrnPhysics
{
namespace Vehicle
{
    class PhysicalTrafficManagerDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        // @0x827DBA50: the debug-menu display name for this component.
        //   asm: lis r11,aPhysicalTraffi@ha ; addi r3,r11,aPhysicalTraffi@l "Physical traffic" ; blr
        const char* GetName() const override;
    };
}
}
