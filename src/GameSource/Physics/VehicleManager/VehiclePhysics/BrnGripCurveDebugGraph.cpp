#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h"

// BrnPhysics::Vehicle::GripCurveDebugGraph -- the two pure-geometry accessors owned by the
// Vehicle-events group. Reconstructed from BURNOUT_X360_ARTIST.XEX. The console bodies are
// emitted in VMX (rw::math::vpu::Vector2 SIMD: splat / vsubfp / vaddfp / vmulfp / vxor
// sign-flip / vperm / vrlimi lane inserts), which have no PC implementation; the recovered
// math is reproduced here scalar over the named Vector2 lanes (semantic parity, members by
// name), matching the offsets +0x10/+0x20/+0x24/+0x30 the disassembly reads.

namespace BrnPhysics
{
namespace Vehicle
{
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
