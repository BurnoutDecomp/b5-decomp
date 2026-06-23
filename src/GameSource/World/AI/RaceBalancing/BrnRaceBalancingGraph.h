#ifndef BRN_RACE_BALANCING_GRAPH_H
#define BRN_RACE_BALANCING_GRAPH_H

// BrnAI::RaceBalancingGraph -- a small lookup curve mapping a 0..1 race-progress
// fraction to a target speed ratio, kept per GraphType (AHEAD / BEHIND). The
// rubber-band balancer samples this graph to decide how much faster/slower than par
// a racer should run. DWARF home: GameSource/World/AI/RaceBalancing/
// BrnRaceBalancingGraph.h (struct BrnAI::RaceBalancingGraph, :43 in the DecFIGS dump).
// Forward-declared by BrnRaceBalancingRoute.h / BrnAStar.h; this header owns the
// RaceBalancingGraph layout.
//
// LAYOUT (DecFIGS DWARF, BrnRaceBalancingGraph.h:62): a single f32 mafSpeedRatios[2][8]
// table -- one row per GraphType, KI_GRAPH_POINT_COUNT (8) sample points per row, the
// points evenly spaced across the [0,1] progress fraction (point i at i/7). Confirmed
// against the X360 asm of ComputeSpeedRatio @0x8277B748 (element address
// = this + ((graphType * 8) + point) * 4). Access is by name.
//
// Only ComputeSpeedRatio (@0x8277B748) is bodied in this TU; Construct and SetPoint
// are declared here and bodied in their own TUs.

#include "types.hpp"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h" // BrnAI::GraphType

namespace BrnAI
{
// DWARF BrnRaceBalancingGraph.h:43.
struct RaceBalancingGraph
{
    // BrnRaceBalancingGraph.h:45 -- number of sample points per graph row.
    static const s32 KI_GRAPH_POINT_COUNT = 8;

    void Construct();                                          // :48
    void SetPoint(GraphType leGraphType, s32 liPoint, f32 lfSpeedRatio); // :54

    // @0x8277B748 -- bodied in this TU.
    f32  ComputeSpeedRatio(GraphType leGraphType, f32 lfFraction) const; // :59

private:
    f32 mafSpeedRatios[E_GRAPH_TYPE_COUNT][KI_GRAPH_POINT_COUNT]; // :62
};
}

#endif
