#pragma once

#include "types.hpp"
// ⚠️ FIXED 2026-08-03: this was `#include "DebugSystem/Core/CgsDebugComponent.h"`, which resolves
// against NO -I directory in either the per-TU gate or build_game_exe.bat -- i.e. this header had
// never been compiled by anything. The real path is below.
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"   // CgsGeometric::Triangle4::AOSTriangle

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
    public:
        // @0x825B4D60 (public, non-virtual per DWARF B5PhysicsHandlingDebugComponent.h:442): stash
        // the collision triangle of the last wall the car scraped, for the DrawLastWallTriangle
        // debug overlay. Copies the whole 80-byte AOSTriangle into mLastWallTriangle @+0x350.
        void SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle);

        // ⭐ ADDED 2026-08-06 (UpdateVehiclePhysics wave). The per-frame tick
        // VehicleManager::UpdateVehiclePhysics calls once per live car (bl @0x82645A78,
        // f1 = the sim timestep). Declaration-only here -- the body is a named FLAG trap
        // stub in BrnVehicleManagerLinkStubs.cpp until this component's own reconstruction
        // pass lands (this TU is not mounted).
        void Update(f32 lfTimeStep);

    protected:
        // @0x825DB0D0: the debug-menu path under which this component is grouped.
        //   asm: lis r11,aPhysics@ha ; addi r3,r11,aPhysics@l "Physics" ; blr
        const char* GetPath() const override;

    private:
        // @+0x350 (BY NAME; DWARF B5PhysicsHandlingDebugComponent.h:427). The last wall-collision
        // triangle recorded for the debug overlay. The ~0x350 bytes of preceding component state
        // (the render/window helpers owned by the dev-UI pass) are not laid out in this slice.
        CgsGeometric::Triangle4::AOSTriangle mLastWallTriangle;
    };
}
}
