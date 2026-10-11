#pragma once

// BrnPhysics::Vehicle::GripCurveDebugGraph -- a debug-only 2D graph that plots a tyre
// grip curve (grip coefficient vs slip ratio) into screen space for the handling debug
// component. Reconstructed from BURNOUT_X360_ARTIST.XEX (GetOrigin @0x825E8D00,
// GetPointOnGraph @0x825E8DB8) and the DecFIGS DWARF hints
// (B5PhysicsHandlingDebugComponent.cpp / BrnVehicleConstants.h:1837 forward-decl).
//
// Complete graph state: one curve pointer and the location/range/size vectors.
// The original handling TU owns Render and the label/current-sample consumers;
// the geometry helpers remain in BrnGripCurveDebugGraph.cpp.

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector2 (16-byte, 4 named lanes)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"   // BrnPhysics::Vehicle::Wheel::TireGripCurve
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"

namespace CgsDev { struct Debug2DImmediateRender; }

namespace BrnPhysics
{
namespace Vehicle
{
    class GripCurveDebugGraph
    {
    public:
        // Two-phase init. The console has no out-of-line body for this: it is inlined whole into
        // GripCurveDebugWindow::Construct, once per graph, and folded back out here so the stores
        // land on the named members instead of on raw offsets inside the window. Clears the curve
        // pointer, the top-left corner and the axis ranges, and seats the axis box at its fixed
        // 300x225 pixel size. The box's unused z/w lanes are deliberately left alone -- the console
        // load-modify-stores only the two lanes it sets.
        void Construct();

        // B5PhysicsHandlingDebugComponent.h:173, inlined in GripCurveDebugWindow::Prepare on X360.
        // The caller stores this exact TireGripCurve pointer into the graph's first word.
        void Prepare(const Wheel::TireGripCurve* lpGripCurve) { mpGripCurve = lpGripCurve; }

        // B5PhysicsHandlingDebugComponent.h:200. GripCurveDebugWindow::Prepare writes the full
        // four-lane range vector into each graph; keep that typed storage update in its named owner.
        void SetRange(rw::math::vpu::Vector2 lRange) { mAxisRanges = lRange; }
        void SetLocation(rw::math::vpu::Vector2 lLocation) { mAxisTopLeft = lLocation; }
        rw::math::vpu::Vector2 GetTitleLocation() const
        {
            rw::math::vpu::Vector2 lLocation = mAxisTopLeft;
            lLocation.y -= 36.0f;
            return lLocation;
        }

        void Render(CgsDev::Debug2DImmediateRender* lpDisplay);
        void PlotCoefficientLabel(CgsDev::Debug2DImmediateRender* lpDisplay, f32 lfCoefficient,
                                   CgsDev::RGBA lColour);
        void PlotSlipRatioLabel(CgsDev::Debug2DImmediateRender* lpDisplay, f32 lfSlipRatio,
                                 CgsDev::RGBA lColour);
        void PlotCurrentValue(CgsDev::Debug2DImmediateRender* lpDisplay, f32 lfSlipRatio,
                              CgsDev::RGBA lColour);

        // Screen-space origin of the inset axes. X360 GetOrigin @0x825E8D00 returns
        // {topLeft.x + 36, topLeft.y + height - 36}. (The console emits this purely in VMX -- splat /
        // sub / add / rlimi-insert lanes; the recovered math is reproduced scalar over the
        // named Vector2 lanes, since the rw vpu SIMD ops have no PC implementation.)
        // Out-of-line in the .cpp so this ledger func (real X360 address) has a definition site.
        rw::math::vpu::Vector2 GetOrigin() const;

        // Map a grip-curve sample (slip ratio along X, grip coefficient along Y) into screen
        // space. X360 GetPointOnGraph @0x825E8DB8 normalises the inputs by the axis ranges
        // (slip / mAxisRanges.x at +0x20, grip / mAxisRanges.y at +0x24), scales by the axis
        // box dimensions minus the 36-unit label insets, and offsets from GetOrigin. The Y screen axis points down,
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

    };
}
}
