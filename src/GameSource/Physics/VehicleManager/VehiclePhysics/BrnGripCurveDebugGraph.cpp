#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h"
#include <cmath>
#include <cstring>

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
    namespace
    {
        f32 GraphFlush(f32 lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        }
        f32 GraphAdd(f32 lfA, f32 lfB) { return GraphFlush(GraphFlush(lfA) + GraphFlush(lfB)); }
        f32 GraphSubtract(f32 lfA, f32 lfB) { return GraphFlush(GraphFlush(lfA) - GraphFlush(lfB)); }
        f32 GraphMultiply(f32 lfA, f32 lfB) { return GraphFlush(GraphFlush(lfA) * GraphFlush(lfB)); }
        f32 GraphFlipSign(f32 lfValue)
        {
            u32 luBits;
            std::memcpy(&luBits, &lfValue, sizeof(luBits));
            luBits ^= 0x80000000u; // ARTIST vxor with the sign mask, including NaN.
            std::memcpy(&lfValue, &luBits, sizeof(lfValue));
            return lfValue;
        }
    }
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

    // GetOrigin @0x825E8D00: the two 36-unit insets leave room for labels.
    // vrlimi replaces X/Y only; the location's remaining lanes survive.
    rw::math::vpu::Vector2 GripCurveDebugGraph::GetOrigin() const
    {
        rw::math::vpu::Vector2 lOrigin = mAxisTopLeft;
        lOrigin.x = GraphAdd(mAxisTopLeft.x, 36.0f);
        lOrigin.y = GraphAdd(mAxisTopLeft.y, GraphSubtract(mAxisDimensions.y, 36.0f));
        return lOrigin;
    }

    // ARTIST 825E8E24/8E64 are scalar fdivs, followed by separate VMX
    // products and addition. There is no reciprocal refinement or fused offset.
    rw::math::vpu::Vector2 GripCurveDebugGraph::GetPointOnGraph(f32 lfSlipRatio,
                                                                f32 lfGripCoefficient) const
    {
        const f32 lfNormX = lfSlipRatio       / mAxisRanges.x;   // a3 / (+0x20)
        const f32 lfNormY = lfGripCoefficient / mAxisRanges.y;   // a4 / (+0x24)

        const f32 lfHeight = GraphSubtract(mAxisDimensions.y, 36.0f);
        const f32 lfWidth = GraphSubtract(mAxisDimensions.x, 36.0f);
        const f32 lfOffsetY = GraphMultiply(GraphFlipSign(lfNormY), lfHeight);
        const f32 lfOffsetX = GraphMultiply(lfNormX, lfWidth);
        const rw::math::vpu::Vector2 lOrigin = GetOrigin();
        rw::math::vpu::Vector2 lPoint;
        lPoint.x = GraphAdd(lOrigin.x, lfOffsetX);
        lPoint.y = GraphAdd(lOrigin.y, lfOffsetY);
        // The original vperm mask 82CDA350 supplies {X, Y, X, X}.
        lPoint.z = GraphAdd(lOrigin.z, lfOffsetX);
        lPoint.w = GraphAdd(lOrigin.w, lfOffsetX);
        return lPoint;
    }
}
}
