#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnAI
{
// BrnAI::RaceBalancingGraph::ComputeSpeedRatio @0x8277B748.
//
// Samples the per-GraphType speed-ratio curve at race-progress fraction lfFraction
// (expected 0..1) by linear interpolation between the two bracketing sample points.
// The KI_GRAPH_POINT_COUNT (8) points are evenly spaced, point i at fraction i/7, so
// the segment index is floor(lfFraction * 7). The X360 build:
//   - asserts leGraphType in [0, E_GRAPH_TYPE_COUNT)          (BrnRaceBalancingGraph.h:109)
//   - liPrevPoint = clamp(floor(lfFraction * 7), 0, 7)
//   - liNextPoint = clamp(liPrevPoint + 1, 0, 7)
//   - asserts liPrevPoint / liNextPoint in [0, KI_GRAPH_POINT_COUNT)   (:116 / :117)
//   - segment fraction lfSegment = clamp((lfFraction - liPrevPoint/7) * 7, 0, 1)
//     (the original uses fsel-based branchless clamps; rendered as structured clamps)
//   - returns lerp(mafSpeedRatios[gt][prev], mafSpeedRatios[gt][next], lfSegment)
//     (asm: fmadds f1 = (next - prev) * lfSegment + prev).
// The baked d:\p4 file/line are dropped in favour of __FILE__/__LINE__ by CGS_ASSERT.
// Called by RaceBalancingRoute::Prepare/Recalculate, RaceBalancingManager::ComputeParSpeed,
// RaceBalancingDebugComponent::RenderHUD.
f32 RaceBalancingGraph::ComputeSpeedRatio(GraphType leGraphType, f32 lfFraction) const
{
    CGS_ASSERT(leGraphType >= 0 && leGraphType < E_GRAPH_TYPE_COUNT,
               "leGraphType >= 0 && leGraphType < E_GRAPH_TYPE_COUNT");

    const s32 liSegment = static_cast<s32>(lfFraction * 7.0f);

    s32 liPrevPoint = liSegment;
    if (liPrevPoint < 0)
    {
        liPrevPoint = 0;
    }
    else if (liPrevPoint > 7)
    {
        liPrevPoint = 7;
    }

    s32 liNextPoint = liSegment + 1;
    if (liNextPoint < 0)
    {
        liNextPoint = 0;
    }
    else if (liNextPoint > 7)
    {
        liNextPoint = 7;
    }

    CGS_ASSERT(liPrevPoint >= 0 && liPrevPoint < KI_GRAPH_POINT_COUNT,
               "liPrevPoint >= 0 && liPrevPoint < KI_GRAPH_POINT_COUNT");
    CGS_ASSERT(liNextPoint >= 0 && liNextPoint < KI_GRAPH_POINT_COUNT,
               "liNextPoint >= 0 && liNextPoint < KI_GRAPH_POINT_COUNT");

    // Position within the [prev,next] segment, clamped to [0,1].
    f32 lfSegmentFraction = (lfFraction - (static_cast<f32>(liPrevPoint) * (1.0f / 7.0f))) * 7.0f;
    if (lfSegmentFraction < 0.0f)
    {
        lfSegmentFraction = 0.0f;
    }
    if (lfSegmentFraction > 1.0f)
    {
        lfSegmentFraction = 1.0f;
    }

    const f32 lfPrev = mafSpeedRatios[leGraphType][liPrevPoint];
    const f32 lfNext = mafSpeedRatios[leGraphType][liNextPoint];
    return (lfNext - lfPrev) * lfSegmentFraction + lfPrev;
}
}
