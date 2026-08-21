#ifndef BRN_TRAFFIC_MATHS_UTILS_H
#define BRN_TRAFFIC_MATHS_UTILS_H

// ============================================================================
// BrnTrafficMathsUtils.h -- OWNING HEADER.
//
// The traffic module's small geometry helpers: free functions in namespace BrnTraffic, all
// inline on the console, so none appears in the ARTIST ledger.
//
// The file lands DECLARED-ONLY, in the exact DWARF shape, the same convention this directory
// uses for LaneRung::EndianSwap. The Feb-2007 copy is NOT ported: its one function
// (`GetLineLineIntersectionParamXZ`) appears nowhere in the DWARF, which instead names two
// functions the leak lacks at lines :80 and :191, so the ship file is at least 191 lines
// against the leak's 74. The leaked body also needs Vector2 operations (Normalize, operator-,
// IsSimilar) that this tree's vector2_operation.h does not carry.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

namespace BrnTraffic
{
    // DWARF BrnTrafficMathsUtils.h:80. DECLARED-ONLY: inlined at every ARTIST call site, so
    // there is no out-of-line body, and every attested call site (the avoidance and
    // crash-awareness legs of TrafficEntityModule) is itself unreconstructed. Recovering it
    // means decompiling the fold-in inside one of those legs. The DWARF carries parameter
    // positions but not names.
    bool IsPointWithinSquishedCone(Vector3 lPoint,
                                   Vector3 lConeOrigin,
                                   VecFloat lfParam2,
                                   VecFloat lfParam3,
                                   VecFloat lfParam4,
                                   Vector3 lConeDirection);

    // DWARF BrnTrafficMathsUtils.h:191. DECLARED-ONLY, same reason.
    void Convert3DVectorTo2D(Vector3 lVector, Vector2& lrOut2D);
}

#endif // BRN_TRAFFIC_MATHS_UTILS_H
