#ifndef BRN_RACE_BALANCING_ROUTE_H
#define BRN_RACE_BALANCING_ROUTE_H

// BrnAI::RaceBalancingRoute -- per-racer race-balancing route timing model.
// DWARF home: BrnRaceBalancingRoute.h. Records ahead/behind par times along the
// route (sampled per checkpoint segment) and derives a 0..1 race-completion
// ratio used by the rubber-band balancer.
//
// Bodied in this TU: ComputeRaceCompletionRatio (@0x8277AD98), GetTime
// (@0x827658F0), GetAISectionSpeed (@0x827697A0), Prepare (@0x82789368) and
// Recalculate (@0x82789708). The remaining members (Construct/OnCheckpoint/
// OnTakenDown/...) are declared here and bodied in their own TUs. Member layout
// follows the DecFIGS DWARF declarations; access is by name. Offsets confirmed
// against the X360 asm: mafTimes @+0, miTimeCount @+0xA00, miTakenDownCount
// @+0xA04, miCurrentCheckpointIndex @+0xA08, mfDistance @+0xA0C,
// mfTakenDownTimePenalty @+0xA10, mbValid @+0xA14.

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector3
#include "SharedClasses/AI/AISectionsResourceType.h"     // BrnAI::AISection / AISectionsData

namespace BrnAI
{
// Separate-TU types referenced by pointer only.
struct Route;
struct RaceBalancingGraph;

// The race-balancing debug HUD (its own TU) reads the route's per-checkpoint times + take-down
// count + validity directly to plot the time graph and the textual read-outs. On X360 those were
// direct member loads (the debug component inlined the access); befriending it is the faithful
// de-inlined form -- additive, no layout or release-behaviour change.
class RaceBalancingDebugComponent;

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

// The rubber-band controller that owns the per-racer routes. On X360 it inlined
// direct loads/stores into the route's private members (mfTakenDownTimePenalty /
// mbValid / miCurrentCheckpointIndex / miTimeCount) in OnRaceStart and
// OnOpponentReachedCheckpoint; befriending it is the faithful de-inlined form of
// that direct access -- additive (like the existing EmbedProbe friend), with no
// layout or release-behaviour change.
struct RaceBalancingManager;

class RaceBalancingRoute
{
    friend struct RaceBalancingRouteEmbedProbe;
    friend struct RaceBalancingManager;
    friend class RaceBalancingDebugComponent;
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

    // @0x827658F0 -- bodied in this TU.
    f32  GetTime(GraphType leGraphType, s32 liIndex) const;
    s32  GetTimeCount() const;
    f32  GetTotalTime(GraphType leGraphType) const;
    bool IsValid() const;

    // @0x827697A0 -- bodied in this TU.
    f32  GetAISectionSpeed(const AISection* lpAISection,
                           const AISectionsData* lpAISectionsData, f32 lfSpeedRatio);
    s32  GetCurrentCheckpointIndex() const;
    f32  GetDistance() const;

private:
    // Never called; bodied in BrnRaceBalancingRoute.cpp so offsetof can see the
    // private members and the compile gate enforces the X360 offsets.
    static void _AssertLayout();

    // Seconds penalty applied to the par time per take-down (rodata).
    static const f32 KF_TAKEN_DOWN_TIME_PENALTY;

    f32  mafTimes[E_GRAPH_TYPE_COUNT][KI_MAX_TIMES]; // :108  (+0)     2 x 320 f32 == 2560B
    s32  miTimeCount;                                 // :109  (+0xA00)
    s32  miTakenDownCount;                            // :110  (+0xA04)
    s32  miCurrentCheckpointIndex;                    // :111  (+0xA08)
    f32  mfDistance;                                  // :112  (+0xA0C)
    // X360 LAYOUT CORRECTION: GetTime (@0x827658F0) reads a per-instance f32 at
    // +0xA10 and returns mafTimes[gt][idx] + (f32)miTakenDownCount * (*+0xA10),
    // and writes mbValid at +0xA14 (Prepare/Recalculate: stb 0xA14). The DecFIGS
    // DWARF (a PS3 build) lists NO member between mfDistance and mbValid and is
    // not offset-authoritative; the X360 asm is. mfTakenDownTimePenalty is the
    // per-racer seconds-per-takedown weight GetTime multiplies by miTakenDownCount
    // (presumably seeded from KF_TAKEN_DOWN_TIME_PENALTY in Construct). Named from
    // its GetTime use; see open_questions.
    f32  mfTakenDownTimePenalty;                       // (+0xA10) X360-attested f32
    bool mbValid;                                     // :113  (+0xA14)
    // Trailing pad: the X360 lays this class out at stride 0xA18 (2584) -- the
    // Array<RaceBalancingRoute,7> element stride and count offset (+0x46A8 ==
    // 7*0xA18) read directly by the indexed-accessor asm (@0x8276A7F8 /
    // @0x8276A900) attest sizeof==0xA18. mbValid sits at +0xA14; the three bytes
    // up to the 0xA18 stride are trailing pad.
    u8   maPad0xA15[3];                               // +0xA15 .. +0xA17
};
}

#endif
