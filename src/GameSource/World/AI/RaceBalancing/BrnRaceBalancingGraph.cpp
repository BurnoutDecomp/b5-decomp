#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnAI
{
// BrnAI::RaceBalancingGraph::Construct -- no standalone console symbol; the console inlines it at its
// caller. AIModule::SetupRaceBalancingManager is that caller and carries
// the whole body: a nested pair of loops over the stack graph writing flt_82001CC0 (0.0) to all
// KI_GRAPH_POINT_COUNT * E_GRAPH_TYPE_COUNT slots -- outer 8 iterations stepping the cursor by 4
// bytes (the point), inner 2 stepping by 0x20 (the row stride, 8 floats), i.e. the table is walked
// point-major but every slot is covered exactly once. Rendered row-major here: same stores, same
// values, no ordering dependency (every write is the same constant).
void RaceBalancingGraph::Construct()
{
    for (s32 liGraphType = 0; liGraphType < E_GRAPH_TYPE_COUNT; ++liGraphType)
    {
        for (s32 liPoint = 0; liPoint < KI_GRAPH_POINT_COUNT; ++liPoint)
        {
            mafSpeedRatios[liGraphType][liPoint] = 0.0f;
        }
    }
}

// BrnAI::RaceBalancingGraph::SetPoint -- likewise inlined, and likewise fully attested by
// SetupRaceBalancingManager: the two `stfsx` stores land
// graph + (graphType * 0x20) + (point * 4) with no bounds assert of their own (the two asserts
// bracketing them are the SOURCE side's, BrnRaceBalance.h in OpponentBalanceData's
// getters). A bare indexed store is therefore the whole body.
void RaceBalancingGraph::SetPoint(GraphType leGraphType, s32 liPoint, f32 lfSpeedRatio)
{
    mafSpeedRatios[leGraphType][liPoint] = lfSpeedRatio;
}

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
