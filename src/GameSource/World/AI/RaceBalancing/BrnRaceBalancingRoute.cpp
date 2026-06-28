// BrnAI::RaceBalancingRoute -- the per-racer rubber-band timing model.
//
// Bodied here:
//   ComputeRaceCompletionRatio (@0x8277AD98) -- within-segment distance -> 0..1
//                                               global race-completion fraction.
//   GetTime                    (@0x827658F0) -- mafTimes[gt][idx] + takedown bias.
//   GetAISectionSpeed          (@0x827697A0) -- lerp(min,max,ratio) by speed class.
//   Prepare                    (@0x82789368) -- forward pass: build the cumulative
//                                               ahead/behind par-time tables.
//   Recalculate                (@0x82789708) -- reverse pass: rebuild the tables as
//                                               time-remaining from the live totals.

#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"

#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.h" // ComputeSpeedRatio
#include "GameSource/World/AI/Route/BrnRoute.h"                      // Route / RouteNode
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT

#include <cmath>   // sqrtf
#include <cstddef> // offsetof

namespace BrnAI
{
namespace
{
    // Euclidean length of a route segment in the (x,y) plane. The X360 form uses a
    // VMX vrsqrtefp + 2-step Newton refinement on (dx*dx + dy*dy) and a vcmpeqfp/
    // vsel guard that forces a 0 result when the squared length is exactly 0;
    // semantically this is sqrt with a zero-input guard.
    inline f32 ComputeSegmentDistance(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1)
    {
        const f32 lfDX = lfX1 - lfX0;
        const f32 lfDY = lfY1 - lfY0;
        const f32 lfLenSq = (lfDX * lfDX) + (lfDY * lfDY);
        if (lfLenSq == 0.0f)
        {
            return 0.0f;
        }
        return sqrtf(lfLenSq);
    }
}

// @0x8277AD98
// The X360 form uses fsel-based branchless clamps (the rwmath Clamp<float>/
// Max<float> helpers inlined): the segment fraction is clamped to [0,1], the
// per-checkpoint start/end fractions are computed from the checkpoint index and
// count, and the lerped result is clamped to [0,1].
f32 RaceBalancingRoute::ComputeRaceCompletionRatio(f32 lfDistanceToNextCheckpoint,
                                                   s32 liCheckpointCount) const
{
    if (mfDistance == 0.0f)
    {
        return 0.0f;
    }

    // Fraction of the current segment already travelled, clamped to [0,1].
    f32 lfCheckpointRatio = (mfDistance - lfDistanceToNextCheckpoint) / mfDistance;
    if (lfCheckpointRatio < 0.0f)
    {
        lfCheckpointRatio = 0.0f;
    }
    if (lfCheckpointRatio > 1.0f)
    {
        lfCheckpointRatio = 1.0f;
    }

    // This checkpoint's start/end fractions of the whole route.
    const f32 lfCheckpointStartRatio =
        static_cast<f32>(miCurrentCheckpointIndex) / static_cast<f32>(liCheckpointCount);
    const f32 lfCheckpointEndRatio =
        static_cast<f32>(miCurrentCheckpointIndex + 1) / static_cast<f32>(liCheckpointCount);

    // Interpolate then clamp to [0,1].
    f32 lfResult = (lfCheckpointEndRatio - lfCheckpointStartRatio) * lfCheckpointRatio
                   + lfCheckpointStartRatio;
    if (lfResult < 0.0f)
    {
        lfResult = 0.0f;
    }
    if (lfResult > 1.0f)
    {
        lfResult = 1.0f;
    }
    return lfResult;
}

// @0x827658F0
// Returns the par time for the (graph,index) cell, biased by the racer's
// take-down count: a take-down adds mfTakenDownTimePenalty seconds to every par
// time (asm: result = mafTimes[gt][idx] + (f32)miTakenDownCount * (*+0xA10)).
// The four ordered asserts fire when the table is invalid or the indices are out
// of range; their messages are taken verbatim from the X360 FireAssert calls.
f32 RaceBalancingRoute::GetTime(GraphType leGraphType, s32 liTimeIndex) const
{
    CGS_ASSERT(mbValid, "mbValid");
    CGS_ASSERT(liTimeIndex >= 0, "liTimeIndex >= 0");
    CGS_ASSERT(liTimeIndex < miTimeCount, "liTimeIndex < miTimeCount");
    CGS_ASSERT(leGraphType >= 0, "leGraphType >= 0");
    CGS_ASSERT(leGraphType < E_GRAPH_TYPE_COUNT, "leGraphType < E_GRAPH_TYPE_COUNT");

    return mafTimes[leGraphType][liTimeIndex]
           + static_cast<f32>(miTakenDownCount) * mfTakenDownTimePenalty;
}

// @0x827697A0
// The section's speed for a given speed-class, interpolated within its
// min..max speed band by lfSpeedRatio: lerp(min, max, ratio). The speed class is
// AISection::muSpeed (+0x15); the band comes from AISectionsData's parallel
// mafSectionMinSpeeds / mafSectionMaxSpeeds arrays.
f32 RaceBalancingRoute::GetAISectionSpeed(const AISection* lpAISection,
                                          const AISectionsData* lpAISectionsData,
                                          f32 lfSpeedRatio)
{
    const u32 luSpeed = lpAISection->muSpeed;
    CGS_ASSERT(luSpeed < E_SECTION_SPEED_COUNT, "muSpeed < E_SECTION_SPEED_COUNT");

    return (lpAISectionsData->mafSectionMaxSpeeds[luSpeed]
            - lpAISectionsData->mafSectionMinSpeeds[luSpeed]) * lfSpeedRatio
           + lpAISectionsData->mafSectionMinSpeeds[luSpeed];
}

// @0x82789368
// Forward pass. Walks the route nodes from the racer's current position,
// accumulating the par time to cover each node-to-node segment for both the
// AHEAD and BEHIND graphs. Each segment's time is its planar length divided by
// the section's interpolated speed (the interpolation fraction is the graph's
// speed ratio sampled at the node's global race-completion fraction). The
// cumulative totals are written into mafTimes[AHEAD][i] / mafTimes[BEHIND][i],
// continuing from the previous tables' last value when miTimeCount > 0.
bool RaceBalancingRoute::Prepare(Vector3 lPosition,
                                 const AISectionsData* lpAISectionsData,
                                 const RaceBalancingGraph* lpRaceBalancingGraph,
                                 const Route* lpRoute, s32 liCheckpointCount)
{
    CGS_ASSERT(lpRoute->GetNodeCount() > 0 && lpRoute->GetDistance() != 0.0f,
               "lpRoute->GetDistance() != 0.0f");

    // Seed the running totals: continue from the previous tables' last entry when
    // a prior Prepare/Recalculate already filled them (miTimeCount > 0), else 0.
    f32 lfAheadTime = 0.0f;
    f32 lfBehindTime = 0.0f;
    const s32 liPrevTimeCount = miTimeCount;
    if (liPrevTimeCount > 0)
    {
        lfAheadTime = mafTimes[E_GRAPH_TYPE_AHEAD][liPrevTimeCount - 1];
        lfBehindTime = mafTimes[E_GRAPH_TYPE_BEHIND][liPrevTimeCount - 1];
    }

    // Cache the whole-route distance (0 for an empty route) and (re)set the count.
    mfDistance = (lpRoute->GetNodeCount() > 0) ? lpRoute->GetDistance() : 0.0f;
    miTimeCount = lpRoute->GetNodeCount();

    // The running reference position for the first segment is the racer position;
    // it advances to each visited node afterwards. Only the (x,y) plane is used,
    // matching the VMX segment-length computation in the X360 form.
    f32 lfPrevX = lPosition.x;
    f32 lfPrevY = lPosition.y;

    const s32 liNodeCount = lpRoute->GetNodeCount();
    for (s32 liNode = 0; liNode < liNodeCount; ++liNode)
    {
        const RouteNode* lpNode = lpRoute->GetNode(liNode);

        const u32 luSectionIndex = lpNode->GetSectionIndex();
        CGS_ASSERT(luSectionIndex < lpAISectionsData->muNumSections,
                   "luSectionIndex < muNumSections");
        const AISection* lpSection = lpAISectionsData->GetAISection(luSectionIndex);

        // Planar length of this segment.
        const f32 lfSegment = ComputeSegmentDistance(lfPrevX, lfPrevY,
                                                      lpNode->GetX(), lpNode->GetY());

        // Global race-completion fraction at this node, then the graph speed
        // ratios for the AHEAD / BEHIND curves at that fraction.
        const f32 lfFraction =
            ComputeRaceCompletionRatio(lpNode->GetDistanceToCheckpoint(), liCheckpointCount);
        const f32 lfAheadRatio = lpRaceBalancingGraph->ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, lfFraction);
        const f32 lfBehindRatio = lpRaceBalancingGraph->ComputeSpeedRatio(E_GRAPH_TYPE_BEHIND, lfFraction);

        // Section speed for each graph, then accumulate time = length / speed.
        const f32 lfAheadSpeed = GetAISectionSpeed(lpSection, lpAISectionsData, lfAheadRatio);
        lfAheadTime += lfSegment / lfAheadSpeed;
        mafTimes[E_GRAPH_TYPE_AHEAD][liNode] = lfAheadTime;

        const f32 lfBehindSpeed = GetAISectionSpeed(lpSection, lpAISectionsData, lfBehindRatio);
        lfBehindTime += lfSegment / lfBehindSpeed;
        mafTimes[E_GRAPH_TYPE_BEHIND][liNode] = lfBehindTime;

        lfPrevX = lpNode->GetX();
        lfPrevY = lpNode->GetY();
    }

    mbValid = true;
    return true;
}

// @0x82789708
// Reverse pass. Re-derives the par-time tables as "time remaining to the route
// end" instead of "time elapsed": it seeds the final entry from the current live
// totals (GetTime of the last index, which folds in the take-down penalty), then
// walks the nodes backward subtracting each segment's per-graph time. mfDistance
// must already be valid (it is read, never written here).
bool RaceBalancingRoute::Recalculate(const AISectionsData* lpAISectionsData,
                                     const RaceBalancingGraph* lpRaceBalancingGraph,
                                     const Route* lpRoute, s32 liCheckpointCount)
{
    CGS_ASSERT(mfDistance != 0.0f, "mfDistance != 0.0f");
    CGS_ASSERT(lpRoute->GetNodeCount() > 0, "lpRoute->GetNodeCount() > 0");

    // Seed the running totals from the current tables' end (take-down biased).
    f32 lfAheadTime = (miTimeCount > 0)
                          ? GetTime(E_GRAPH_TYPE_AHEAD, miTimeCount - 1)
                          : 0.0f;
    f32 lfBehindTime = (miTimeCount > 0)
                           ? GetTime(E_GRAPH_TYPE_BEHIND, miTimeCount - 1)
                           : 0.0f;

    const s32 liNodeCount = lpRoute->GetNodeCount();
    miTimeCount = liNodeCount;

    // The last node carries the full remaining time.
    mafTimes[E_GRAPH_TYPE_AHEAD][liNodeCount - 1] = lfAheadTime;
    mafTimes[E_GRAPH_TYPE_BEHIND][liNodeCount - 1] = lfBehindTime;

    // Walk backward: for segment (i, i+1) subtract its time so mafTimes[gt][i]
    // becomes the time remaining from node i onward.
    for (s32 liNode = liNodeCount - 2; liNode >= 0; --liNode)
    {
        const RouteNode* lpNode = lpRoute->GetNode(liNode + 1);
        const RouteNode* lpPrevNode = lpRoute->GetNode(liNode);

        const u32 luSectionIndex = lpNode->GetSectionIndex();
        CGS_ASSERT(luSectionIndex < lpAISectionsData->muNumSections,
                   "luSectionIndex < muNumSections");
        const AISection* lpSection = lpAISectionsData->GetAISection(luSectionIndex);

        const f32 lfSegment = ComputeSegmentDistance(lpPrevNode->GetX(), lpPrevNode->GetY(),
                                                      lpNode->GetX(), lpNode->GetY());

        const f32 lfFraction =
            ComputeRaceCompletionRatio(lpNode->GetDistanceToCheckpoint(), liCheckpointCount);
        const f32 lfAheadRatio = lpRaceBalancingGraph->ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, lfFraction);
        const f32 lfBehindRatio = lpRaceBalancingGraph->ComputeSpeedRatio(E_GRAPH_TYPE_BEHIND, lfFraction);

        const f32 lfAheadSpeed = GetAISectionSpeed(lpSection, lpAISectionsData, lfAheadRatio);
        lfAheadTime -= lfSegment / lfAheadSpeed;
        mafTimes[E_GRAPH_TYPE_AHEAD][liNode] = lfAheadTime;

        const f32 lfBehindSpeed = GetAISectionSpeed(lpSection, lpAISectionsData, lfBehindRatio);
        lfBehindTime -= lfSegment / lfBehindSpeed;
        mafTimes[E_GRAPH_TYPE_BEHIND][liNode] = lfBehindTime;
    }

    mbValid = true;
    return true;
}

// Never called; exists so offsetof can pin the private member offsets the X360
// asm reads/writes and the compile gate enforces them.
void RaceBalancingRoute::_AssertLayout()
{
    static_assert(offsetof(RaceBalancingRoute, mafTimes) == 0, "mafTimes at +0");
    static_assert(offsetof(RaceBalancingRoute, miTimeCount) == 0xA00, "miTimeCount @+0xA00");
    static_assert(offsetof(RaceBalancingRoute, miTakenDownCount) == 0xA04, "miTakenDownCount @+0xA04");
    static_assert(offsetof(RaceBalancingRoute, miCurrentCheckpointIndex) == 0xA08,
                  "miCurrentCheckpointIndex @+0xA08");
    static_assert(offsetof(RaceBalancingRoute, mfDistance) == 0xA0C, "mfDistance @+0xA0C");
    static_assert(offsetof(RaceBalancingRoute, mfTakenDownTimePenalty) == 0xA10,
                  "mfTakenDownTimePenalty @+0xA10");
    static_assert(offsetof(RaceBalancingRoute, mbValid) == 0xA14, "mbValid @+0xA14");
    static_assert(sizeof(RaceBalancingRoute) == 0xA18, "sizeof RaceBalancingRoute == 0xA18");
}
}
