// BrnAI::RaceBalancingManager -- the five standalone X360 symbols of the race
// rubber-band controller. The manager keeps, per opponent slot, a speed-ratio curve
// (RaceBalancingGraph) and a par-time model (RaceBalancingRoute); from those it derives
// how fast an AI opponent should drive so the field stays competitive.
//
//   OnRaceStart                  @0x82789AF8
//   OnOpponentReachedCheckpoint  @0x82789D88
//   ComputeParSpeed (private)    @0x82789EC0
//   ComputeTargetSpeed (private) @0x827916E0
//   CalculateScheduleOffset      @0x82789E00
//
// The remaining DWARF methods were inlined on X360 (no standalone symbol) and are bodied
// in their own TUs. The baked d:\p4 assert file/line are dropped in favour of
// __FILE__/__LINE__ by CGS_ASSERT; the assert MESSAGE strings are verbatim from the asm.

#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"

#include <cstddef>                                   // offsetof (layout pins)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/World/AI/BrnAICar.h"            // BrnAI::AICar + its accessors
#include "GameSource/World/AI/BrnAISharedConstants.h"// EAICarState
#include "GameSource/World/AI/Route/BrnRoute.h"      // BrnAI::RouteNode (GetNextRouteNode deref)
#include "SharedClasses/AI/AISectionsResourceType.h" // BrnAI::AISection / AISectionsData

namespace BrnAI
{
// ===========================================================================================
// OnRaceStart @0x82789AF8
// Clears and (re)builds the per-opponent graph/route tables at the start of a race:
//   - copies the caller's RaceBalancingGraph array in (Construct + AppendArray<7>);
//   - sizes the route table to the same opponent count and resets each route to "empty,
//     invalid", seeding its per-takedown time penalty (20s when lbHighTakenDownPenalty,
//     else 5s -- the X360 chooses this from the r6 argument);
//   - zeroes the race clock, stores the checkpoint count, and raises mbInRace/mbOnStartLine.
// ===========================================================================================
void RaceBalancingManager::OnRaceStart(const Array<RaceBalancingGraph, 7u>* lpRaceBalancingGraphArray,
                                       s32 liCheckpointCount, bool lbHighTakenDownPenalty)
{
    CGS_ASSERT(lpRaceBalancingGraphArray != NULL, "lpRaceBalancingGraphArray != NULL");

    // Copy the incoming graph table in (the X360 stores 0 into maRaceBalancingGraphs.miCount
    // first == Construct/Clear, then AppendArray<7> appends every live source graph).
    maRaceBalancingGraphs.Construct();
    maRaceBalancingGraphs.AppendArray(*lpRaceBalancingGraphArray);

    // One route per graph (opponent). The X360 sets the route table's live count to the graph
    // count up-front then fills slots 0..count-1 in place; appending each freshly-reset route
    // (AddNew == reserve next slot, count++) reaches the identical end state via the public API.
    maRaceBalancingRoutes.Construct();
    const u32 luOpponentCount = maRaceBalancingGraphs.GetLength();
    for (u32 luOpponent = 0; luOpponent < luOpponentCount; ++luOpponent)
    {
        RaceBalancingRoute* lpRoute = maRaceBalancingRoutes.AddNew();

        // Inlined per-route reset (the X360 emits these six stores directly; manager is a
        // friend of RaceBalancingRoute so the de-inlined named writes are faithful).
        lpRoute->miTimeCount              = 0;            // +0xA00
        lpRoute->miTakenDownCount         = 0;            // +0xA04
        lpRoute->miCurrentCheckpointIndex = 0;            // +0xA08
        lpRoute->mfDistance               = 0.0f;         // +0xA0C
        lpRoute->mbValid                  = false;        // +0xA14
        // Per-takedown seconds penalty: 20.0s (0x41A00000) when the high-penalty flag is set,
        // else 5.0s (0x40A00000). Literals visible in the X360 pseudocode (not fabricated).
        lpRoute->mfTakenDownTimePenalty   = lbHighTakenDownPenalty ? 20.0f : 5.0f; // +0xA10
    }

    mfRaceTime        = 0.0f;             // +0x4870
    miCheckpointCount = liCheckpointCount;// +0x4874
    mbInRace          = true;             // +0x4878
    mbOnStartLine     = true;             // +0x4879
}

// ===========================================================================================
// OnOpponentReachedCheckpoint @0x82789D88
// An AI opponent crossed checkpoint liCheckpointIndex: invalidate its route's cached timing
// and advance its current-checkpoint cursor to the next checkpoint. (The X360 inlined what the
// PS3 DWARF spells as RaceBalancingRoute::OnCheckpoint to these two direct stores; the dev
// StrStream log present in the PS3 build is compiled out here.)
// ===========================================================================================
void RaceBalancingManager::OnOpponentReachedCheckpoint(const AICar* lpAICar, s32 liCheckpointIndex)
{
    if (mbInRace)
    {
        CGS_ASSERT(!lpAICar->IsPlayerCar(), "!lpAICar->IsPlayerCar()");

        RaceBalancingRoute& lrRoute = maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];
        lrRoute.mbValid                  = false;                  // +0xA14 -> 0
        lrRoute.miCurrentCheckpointIndex = liCheckpointIndex + 1;  // +0xA08 -> idx+1
    }
}

// ===========================================================================================
// ComputeParSpeed @0x82789EC0  (private)
// The "par" speed for this opponent at its current route position: sample the opponent's
// speed-ratio curve at its race-completion fraction, then convert that ratio into a concrete
// section speed. Returns KF_DEFAULT_SPEED when the racer has no valid route or the race is
// not running.
// ===========================================================================================
f32 RaceBalancingManager::ComputeParSpeed(GraphType leGraphType, const AICar* lpAICar,
                                          const AISectionsData* lpAISectionsData) const
{
    if (!(lpAICar->HasValidRoute() && mbInRace))
    {
        return KF_DEFAULT_SPEED;   // flt_8300D7F8
    }

    const s8 liOpponent = lpAICar->GetOpponentIndex();
    const RaceBalancingGraph& lrGraph = maRaceBalancingGraphs[static_cast<u32>(liOpponent)];
    const RaceBalancingRoute& lrRoute = maRaceBalancingRoutes[static_cast<u32>(liOpponent)];

    // Next route node (bounds-checked against the route node count; null if out of range).
    const RouteNode* lpNode = lpAICar->GetNextRouteNode();

    // The AI section this node sits in, and the racer's 0..1 completion of the whole route.
    const AISection* lpAISection = lpAISectionsData->GetAISection(lpNode->GetSectionIndex());
    const f32 lfRaceCompletionRatio =
        lrRoute.ComputeRaceCompletionRatio(lpNode->GetDistanceToCheckpoint(), miCheckpointCount);

    // Curve-sampled speed ratio for this completion fraction.
    const f32 lfSpeedRatio = lrGraph.ComputeSpeedRatio(leGraphType, lfRaceCompletionRatio);

    // Convert the ratio into a section speed. GetAISectionSpeed is declared non-const on
    // RaceBalancingRoute (X360 ignored const-correctness here); cast to match its signature.
    return const_cast<RaceBalancingRoute&>(lrRoute).GetAISectionSpeed(lpAISection, lpAISectionsData,
                                                                      lfSpeedRatio);
}

// ===========================================================================================
// ComputeTargetSpeed @0x827916E0  (private GraphType overload)
// Turns the par speed into the speed the opponent should actually drive: nudges it up when the
// racer is behind its scheduled par time and down when ahead, within a bounded multiplier band
// (wider when the car is OUT_OF_RANGE), then caps the result at the player's max speed (except
// for an out-of-range car that is already ahead of the player).
// ===========================================================================================
f32 RaceBalancingManager::ComputeTargetSpeed(GraphType leGraphType, const AICar* lpAICar,
                                             const AISectionsData* lpAISectionsData) const
{
    f32 lfSpeed = KF_DEFAULT_SPEED;   // flt_8300D7F8 (default if no valid route / not racing)

    if (lpAICar->HasValidRoute() && mbInRace)
    {
        const RaceBalancingRoute& lrRoute =
            maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];

        const f32 lfParSpeed = ComputeParSpeed(leGraphType, lpAICar, lpAISectionsData);
        lfSpeed = lfParSpeed;

        if (lrRoute.mbValid)
        {
            const f32 lfTargetTime = lrRoute.GetTime(leGraphType, lpAICar->GetNextRouteNodeIndex());

            // Wider band when the car is not in the player's range.
            f32 lfMinMultiplier;
            f32 lfMaxMultiplier;
            if (lpAICar->GetState() != E_AI_CAR_STATE_IN_RANGE)
            {
                lfMinMultiplier = KF_MIN_SPEED_MULTIPLIER_OUT_OF_RANGE; // 0.8
                lfMaxMultiplier = KF_MAX_SPEED_MULTIPLIER_OUT_OF_RANGE; // 1.2
            }
            else
            {
                lfMinMultiplier = KF_MIN_SPEED_MULTIPLIER_IN_RANGE;     // 0.9
                lfMaxMultiplier = KF_MAX_SPEED_MULTIPLIER_IN_RANGE;     // 1.1
            }

            // Schedule offset -> bounded speed multiplier: 1 + (raceTime - parTime)*0.1, clamped.
            f32 lfMultiplier = (mfRaceTime - lfTargetTime) * KF_SPEED_DIFFERENCE_MULTIPLIER + 1.0f;
            if (lfMultiplier < lfMinMultiplier)
            {
                lfMultiplier = lfMinMultiplier;
            }
            if (lfMultiplier > lfMaxMultiplier)
            {
                lfMultiplier = lfMaxMultiplier;
            }

            lfSpeed = lfParSpeed * lfMultiplier;
        }

        // Cap at the player's max speed, EXCEPT when an out-of-range car is already ahead of
        // the player (those are allowed to keep pulling away).
        if (!(lpAICar->GetState() != E_AI_CAR_STATE_IN_RANGE && lpAICar->IsAheadOfPlayer()))
        {
            const f32 lfMaxPlayerSpeed = lpAICar->GetMaxPlayerSpeed();
            if (lfSpeed > lfMaxPlayerSpeed)
            {
                lfSpeed = lfMaxPlayerSpeed;
            }
        }
    }

    return lfSpeed;
}

// ===========================================================================================
// CalculateScheduleOffset @0x82789E00
// Fills lafOutScheduleOffsets[2] with how far (in seconds) this opponent is from its AHEAD and
// BEHIND par schedules at the current checkpoint: positive == that much par time still remains
// (ahead of where it "should" be), relative to the live race clock. Both entries are 0 when the
// racer has no valid route / the race is not running / the route timing is not yet valid.
//   [E_GRAPH_TYPE_AHEAD]  = parTime(AHEAD,  nextNode) - raceTime
//   [E_GRAPH_TYPE_BEHIND] = parTime(BEHIND, nextNode) - raceTime
// (The X360 writes the BEHIND entry first, then the AHEAD entry.)
// ===========================================================================================
void RaceBalancingManager::CalculateScheduleOffset(const AICar* lpAICar,
                                                   f32* lafOutScheduleOffsets) const
{
    lafOutScheduleOffsets[E_GRAPH_TYPE_BEHIND] = 0.0f;
    lafOutScheduleOffsets[E_GRAPH_TYPE_AHEAD]  = 0.0f;

    if (mbInRace)
    {
        const RaceBalancingRoute& lrRoute =
            maRaceBalancingRoutes[static_cast<u32>(lpAICar->GetOpponentIndex())];

        if (lpAICar->HasValidRoute() && lrRoute.mbValid)
        {
            const s32 liNodeIndex = lpAICar->GetNextRouteNodeIndex();
            lafOutScheduleOffsets[E_GRAPH_TYPE_BEHIND] =
                lrRoute.GetTime(E_GRAPH_TYPE_BEHIND, liNodeIndex) - mfRaceTime;
            lafOutScheduleOffsets[E_GRAPH_TYPE_AHEAD] =
                lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, liNodeIndex) - mfRaceTime;
        }
    }
}

// Never called -- pins the touched private offsets so the compile gate enforces the X360
// layout (offsetof needs this member-function context to see the private members).
void RaceBalancingManager::_AssertLayout()
{
    static_assert(offsetof(RaceBalancingManager, maRaceBalancingGraphs) == 0x0000,
                  "maRaceBalancingGraphs @ +0");
    static_assert(offsetof(RaceBalancingManager, maRaceBalancingRoutes) == 0x01C4,
                  "maRaceBalancingRoutes @ +0x1C4");
    static_assert(offsetof(RaceBalancingManager, mfRaceTime)            == 0x4870,
                  "mfRaceTime @ +0x4870");
    static_assert(offsetof(RaceBalancingManager, miCheckpointCount)     == 0x4874,
                  "miCheckpointCount @ +0x4874");
    static_assert(offsetof(RaceBalancingManager, mbInRace)              == 0x4878,
                  "mbInRace @ +0x4878");
    static_assert(offsetof(RaceBalancingManager, mbOnStartLine)         == 0x4879,
                  "mbOnStartLine @ +0x4879");
}
}
