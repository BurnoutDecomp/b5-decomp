#pragma once

// ============================================================================
// BrnPhysics::Vehicle::GripCurveDebugWindow
//   GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugWindow.h
//   (DWARF home B5PhysicsHandlingDebugComponent.h)
//
// A debug window that plots a wheel's longitudinal + lateral tyre grip curves. Derives from the
// debug-UI Window so it can be added to / removed from the debug-UI window stack. Reconstructed
// from BURNOUT_X360_ARTIST.XEX (Show @0x825B4B00, Hide @0x825B4B60, GetSlipRatioRange @0x825B4BC0,
// GetCoefficientRange @0x825B4C90).
//
// LAYOUT: the two GripCurveDebugGraph members sit at the X360 +0x30 / +0x70 and the visible flag
// at +0xB8. Only the member set the wave-7 functions touch is modelled (the two graphs + mbVisible);
// they are declared in console order after the Window base. Exact console byte offsets are NOT
// pinned (host pointers are 8 bytes; semantic parity by named member, per project rule) -- the
// range getters reach each graph's grip curve via GripCurveDebugGraph::GetGripCurve(), and Show/Hide
// toggle mbVisible; no consumer embeds this window by value at a pinned offset.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"          // CgsDev::DebugUI::Window
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h"   // GripCurveDebugGraph

namespace BrnPhysics
{
namespace Vehicle
{
    class GripCurveDebugWindow : public CgsDev::DebugUI::Window
    {
    public:
        // @0x825B4B00 / @0x825B4B60: add/remove this window to/from the debug-UI stack + toggle mbVisible.
        void Show();
        void Hide();

        // @0x825B4BC0 / @0x825B4C90: the X (slip-ratio) / Y (grip-coefficient) axis range for the
        // graph, sized to fit both tyres' curves with 20% headroom, ceil'd to a whole number.
        f32  GetSlipRatioRange();
        f32  GetCoefficientRange();

    private:
        GripCurveDebugGraph mLongGripCurveGraph;   // X360 +0x30
        GripCurveDebugGraph mLatGripCurveGraph;    // X360 +0x70
        bool                mbVisible;             // X360 +0xB8
    };
}
}
