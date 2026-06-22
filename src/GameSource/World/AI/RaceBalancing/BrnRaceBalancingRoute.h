#ifndef BRN_RACE_BALANCING_ROUTE_H
#define BRN_RACE_BALANCING_ROUTE_H

// BrnAI::RaceBalancingRoute -- per-racer race-balancing route timing model.
// DWARF home: BrnRaceBalancingRoute.h. Records ahead/behind par times along the
// route (sampled per checkpoint segment) and derives a 0..1 race-completion
// ratio used by the rubber-band balancer.
//
// Only ComputeRaceCompletionRatio (@0x8277AD98) is bodied in this TU; the other
// members (Construct/Prepare/Recalculate/OnCheckpoint/.../GetAISectionSpeed) are
// declared here and bodied in their own TUs. Member layout follows the DecFIGS
// DWARF declarations; access is by name (mfDistance at +0xA0C and
// miCurrentCheckpointIndex at +0xA08 confirmed against the X360 asm).

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3
#include "SharedClasses/AI/AISectionsResourceType.h"     // BrnAI::AISection / AISectionsData

namespace BrnAI
{
// Separate-TU types referenced by pointer only.
struct Route;
struct RaceBalancingGraph;

// Which timing graph a sample belongs to. The route keeps two parallel time
// tables (mafTimes[2][...]): the par time for being AHEAD vs BEHIND the racer.
enum GraphType
{
    E_GRAPH_TYPE_AHEAD = 0,
    E_GRAPH_TYPE_BEHIND,
    E_GRAPH_TYPE_COUNT,
};

// Compile-gate probe (embed check) -- befriended so it can pin the private
// member offsets the X360 asm reads; no effect on layout or release behaviour.
struct RaceBalancingRouteEmbedProbe;

class RaceBalancingRoute
{
    friend struct RaceBalancingRouteEmbedProbe;
public:
    static const s32 KI_MAX_TIMES = 320; // mafTimes second extent

    void Construct();
    bool Prepare(Vector3 lPosition, const AISectionsData* lpAISectionsData,
                 const RaceBalancingGraph* lpRaceBalancingGraph,
                 const Route* lpRoute, s32 liCheckpointCount);
    bool Recalculate(const AISectionsData* lpAISectionsData,
                     const RaceBalancingGraph* lpRaceBalancingGraph,
                     const Route* lpRoute, s32 liCheckpointCount);
    void OnCheckpoint(s32 liCheckpointIndex, f32 lfTime);
    void OnTakenDown();

    // @0x8277AD98 -- bodied in this TU.
    f32  ComputeRaceCompletionRatio(f32 lfDistanceToNextCheckpoint,
                                    s32 liCheckpointCount) const;

    f32  GetTime(GraphType leGraphType, s32 liIndex) const;
    s32  GetTimeCount() const;
    f32  GetTotalTime(GraphType leGraphType) const;
    bool IsValid() const;

    f32  GetAISectionSpeed(const AISection* lpAISection,
                           const AISectionsData* lpAISectionsData, f32 lfSpeedRatio);
    s32  GetCurrentCheckpointIndex() const;
    f32  GetDistance() const;

private:
    // Seconds penalty applied to the par time per take-down (rodata).
    static const f32 KF_TAKEN_DOWN_TIME_PENALTY;

    f32  mafTimes[E_GRAPH_TYPE_COUNT][KI_MAX_TIMES]; // :108  (2 x 320 f32)
    s32  miTimeCount;                                 // :109  (+0xA00)
    s32  miTakenDownCount;                            // :110  (+0xA04)
    s32  miCurrentCheckpointIndex;                    // :111  (+0xA08)
    f32  mfDistance;                                  // :112  (+0xA0C)
    bool mbValid;                                     // :113
};
}

#endif
