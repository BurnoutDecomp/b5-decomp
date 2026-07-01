#pragma once

// BrnPhysics::Vehicle::GripCurveDebugGraph -- a debug-only 2D graph that plots a tyre
// grip curve (grip coefficient vs slip ratio) into screen space for the handling debug
// component. Reconstructed from BURNOUT_X360_ARTIST.XEX (GetOrigin @0x825E8D00,
// GetPointOnGraph @0x825E8DB8) and the DecFIGS DWARF hints
// (B5PhysicsHandlingDebugComponent.cpp / BrnVehicleConstants.h:1837 forward-decl).
//
// LAYOUT NOTE (HONEST PLACEHOLDER): this TU (the "Vehicle-events" group) owns only the two
// pure-geometry accessors GetOrigin / GetPointOnGraph. Those two touch exactly three
// 16-byte-aligned rw::math::vpu::Vector2 members, at object offsets +0x10, +0x20 and +0x30.
// The remaining members -- the grip-curve pointer set by Prepare(), and the screen member at
// +0x00 zeroed by Construct() -- are NOT exercised by this TU and are left as flagged
// placeholders so the recovered offsets stay exact. They must be filled in by the TU that
// owns Construct/Prepare/Render (do NOT redefine the named members below; GROW this header).

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector2 (16-byte, 4 named lanes)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"   // BrnPhysics::Vehicle::Wheel::TireGripCurve

namespace BrnPhysics
{
namespace Vehicle
{
    class GripCurveDebugGraph
    {
    public:
        // Screen-space point of the graph origin (bottom-left of the axis box): the top-left
        // corner translated down by the axis box height. X360 GetOrigin @0x825E8D00 reads the
        // top-left at +0x10 and the box dimensions' height lane (Dy) at +0x30, returning
        // {topLeft.x, topLeft.y + height}. (The console emits this purely in VMX -- splat /
        // sub / add / rlimi-insert lanes; the recovered math is reproduced scalar over the
        // named Vector2 lanes, since the rw vpu SIMD ops have no PC implementation.)
        // Out-of-line in the .cpp so this ledger func (real X360 address) has a definition site.
        rw::math::vpu::Vector2 GetOrigin() const;

        // Map a grip-curve sample (slip ratio along X, grip coefficient along Y) into screen
        // space. X360 GetPointOnGraph @0x825E8DB8 normalises the inputs by the axis ranges
        // (slip / mAxisRanges.x at +0x20, grip / mAxisRanges.y at +0x24), scales by the axis
        // box dimensions at +0x30, and offsets from GetOrigin. The Y screen axis points down,
        // so grip grows upward (negative screen-Y): the console negates the Y term with a
        // vxor sign-mask. (Recovered scalar over the named Vector2 lanes; the rw vpu SIMD ops
        // have no PC implementation.) Out-of-line in the .cpp (ledger func definition site).
        rw::math::vpu::Vector2 GetPointOnGraph(f32 lfSlipRatio, f32 lfGripCoefficient) const;

        // @+0x00 (DWARF B5PhysicsHandlingDebugComponent.h:221): the grip curve this graph plots,
        // set by Prepare(). Read by GripCurveDebugWindow::GetCoefficientRange/GetSlipRatioRange.
        const Wheel::TireGripCurve* GetGripCurve() const { return mpGripCurve; }

    private:
        // +0x00 (DWARF B5PhysicsHandlingDebugComponent.h:178-181): the grip-curve source pointer set
        // by Prepare(const Wheel::TireGripCurve*). On the X360 this is a 4-byte pointer occupying the
        // first word of the +0x00..+0x10 slot; padded here so mAxisTopLeft stays at +0x10.
        const Wheel::TireGripCurve* mpGripCurve;
        u8                          mPad0x00[16 - sizeof(const Wheel::TireGripCurve*)];

        rw::math::vpu::Vector2 mAxisTopLeft;     // +0x10: top-left screen corner of the axis box
        rw::math::vpu::Vector2 mAxisRanges;      // +0x20: .x = slip-ratio range, .y = grip range
        rw::math::vpu::Vector2 mAxisDimensions;  // +0x30: .x = box width (px), .y = box height (px)

        // +0x40 onward: any further Render/window state. Not touched by this TU. FLAGGED PLACEHOLDER
        // -- the Render owner must add (GROW) these without disturbing the offsets above.
    };
}
}
