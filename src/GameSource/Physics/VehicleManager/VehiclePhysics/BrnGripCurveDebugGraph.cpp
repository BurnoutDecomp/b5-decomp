#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h"

// BrnPhysics::Vehicle::GripCurveDebugGraph -- the two pure-geometry accessors owned by the
// Vehicle-events group. Reconstructed from BURNOUT_X360_ARTIST.XEX. The console bodies are
// emitted in VMX (rw::math::vpu::Vector2 SIMD: splat / vsubfp / vaddfp / vmulfp / vxor
// sign-flip / vperm / vrlimi lane inserts), which have no PC implementation; the recovered
// math is reproduced here scalar over the named Vector2 lanes (semantic parity, members by
// name), matching the offsets +0x10/+0x20/+0x24/+0x30 the disassembly reads.
//
// The two-phase Construct is homed here too: the handling debug component folds it back out
// of GripCurveDebugWindow::Construct, which inlines it once per graph.

namespace BrnPhysics
{
namespace Vehicle
{
    // Two-phase init, inlined by the console into GripCurveDebugWindow::Construct (once per
    // graph). Four stores: the curve pointer word at +0x00, a zero quadword over mAxisTopLeft
    // (+0x10), a zero quadword over mAxisRanges (+0x20), and a two-lane insert into
    // mAxisDimensions (+0x30). The axis box is a fixed 300 x 225 pixels -- the same 225 the
    // window's Render uses as the vertical advance between its two stacked graphs.
    void GripCurveDebugGraph::Construct()
    {
        mpGripCurve = nullptr;

        mAxisTopLeft.SetZero();
        mAxisRanges.SetZero();

        // Lanes z/w of +0x30 are NOT touched: the console load-modify-stores the quadword and
        // only replaces the two lanes it sets.
        mAxisDimensions.x = 300.0f;   // box width, pixels
        mAxisDimensions.y = 225.0f;   // box height, pixels
    }

    // GetOrigin @0x825E8D00: {topLeft.x, topLeft.y + boxHeight}.
    rw::math::vpu::Vector2 GripCurveDebugGraph::GetOrigin() const
    {
        rw::math::vpu::Vector2 lOrigin;
        lOrigin.SetZero();
        lOrigin.x = mAxisTopLeft.x;                       // +0x10.x
        lOrigin.y = mAxisTopLeft.y + mAxisDimensions.y;   // +0x10.y + (+0x30).y (box height)
        return lOrigin;
    }

    // GetPointOnGraph @0x825E8DB8: origin + (normSlip*width, -normGrip*height).
    rw::math::vpu::Vector2 GripCurveDebugGraph::GetPointOnGraph(f32 lfSlipRatio,
                                                                f32 lfGripCoefficient) const
    {
        const f32 lfNormX = lfSlipRatio       / mAxisRanges.x;   // a3 / (+0x20)
        const f32 lfNormY = lfGripCoefficient / mAxisRanges.y;   // a4 / (+0x24)

        rw::math::vpu::Vector2 lOrigin = GetOrigin();

        rw::math::vpu::Vector2 lPoint;
        lPoint.SetZero();
        lPoint.x = lOrigin.x + lfNormX * mAxisDimensions.x;      // grow right
        lPoint.y = lOrigin.y - lfNormY * mAxisDimensions.y;      // grow up (screen-Y points down)
        return lPoint;
    }
}
}
