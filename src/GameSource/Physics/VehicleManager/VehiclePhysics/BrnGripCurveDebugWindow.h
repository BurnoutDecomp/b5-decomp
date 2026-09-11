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
// LAYOUT: complete. The two graphs sit at +0x30 / +0x70, the axle pair the window plots at
// +0xB0 / +0xB4 and the visible flag at +0xB8, closing the console's 0xC0-byte object behind a
// width-identical Window base. The host image is 0xD0, because the two wheel pointers widen
// 4 -> 8 and the graphs' 16-byte alignment rounds the tail up; the handling debug component embeds
// two of these BY VALUE and its own layout gate pins that 0xD0 (and therefore this member set)
// seat by seat.
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
        // Two-phase init, run once per window by the handling debug component's Construct. Seats
        // both graphs, clears the wheel pair the window plots and starts hidden. The window's own
        // Window base is deliberately NOT touched -- Prepare is what fills that in.
        void Construct();

        // @0x825B4B00 / @0x825B4B60: add/remove this window to/from the debug-UI stack + toggle mbVisible.
        void Show();
        void Hide();

        // @0x825B4BC0 / @0x825B4C90: the X (slip-ratio) / Y (grip-coefficient) axis range for the
        // graph, sized to fit both tyres' curves with 20% headroom, ceil'd to a whole number.
        f32  GetSlipRatioRange();
        f32  GetCoefficientRange();

    private:
        GripCurveDebugGraph mLongGripCurveGraph;   // +0x30
        GripCurveDebugGraph mLatGripCurveGraph;    // +0x70
        Wheel*              mpLeftWheel;           // +0xB0  the axle pair this window plots;
        Wheel*              mpRightWheel;          // +0xB4  Prepare fills them in
        bool                mbVisible;             // +0xB8
    };
}
}
