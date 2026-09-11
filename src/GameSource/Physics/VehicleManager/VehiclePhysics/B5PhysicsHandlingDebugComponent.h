#pragma once

#include "types.hpp"
// FIXED 2026-08-03: this was `#include "DebugSystem/Core/CgsDebugComponent.h"`, which resolves
// against NO -I directory in either the per-TU gate or build_game_exe.bat -- i.e. this header had
// never been compiled by anything. The real path is below.
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"   // CgsGeometric::Triangle4::AOSTriangle

// BrnPhysics::Vehicle::DebugComponent - the in-game handling/grip-curve debug menu for the
// vehicle physics. Derives from the real CgsDev::DebugComponent. The full component (the
// PrimitiveVehicleDebugRender2D / GearStats / GripCurveDebugGraph / GripCurveDebugWindow
// helper classes and the render/update/window machinery declared in the DWARF) is owned by
// its own dev-UI pass. Incremental: this TU implements the per-car two-phase Construct, the
// gear-stat reset, the leaf path getter, the last-wall-triangle setter and the per-frame
// sampling tick. Each is declared here BY NAME so its body has a real .cpp home.

namespace BrnPhysics
{
namespace Vehicle
{
    class RaceCarPhysics;

    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Two-phase init, one per car, run from VehicleManager::PrepareData. Records the car this
        // component samples, seats the two grip-curve windows, clears the gear stats and every
        // draw toggle, and zeroes the per-wheel and last-wall-triangle scratch. Bodied in this TU.
        void Construct(RaceCarPhysics* lpRaceCarPhysics);

        // Clear all six gear-stat rows (six floats plus the row's closed flag). Construct inlines
        // this; UpdateAndRenderGearStats is what closes a row again.
        void ResetGearStats();

        // @0x825B4D60 (public, non-virtual per DWARF B5PhysicsHandlingDebugComponent.h:442): stash
        // the collision triangle of the last wall the car scraped, for the DrawLastWallTriangle
        // debug overlay. Copies the whole 80-byte AOSTriangle into mLastWallTriangle @+0x350.
        void SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle);

        // The per-frame tick VehicleManager::UpdateVehiclePhysics calls once per live car with
        // the sim timestep. Samples only: gear-stats time accumulate, the four wheels'
        // road-contact mirror, and the speed mirror. Bodied in this TU.
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
