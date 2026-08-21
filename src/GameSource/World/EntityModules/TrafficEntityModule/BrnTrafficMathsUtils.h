#ifndef BRN_TRAFFIC_MATHS_UTILS_H
#define BRN_TRAFFIC_MATHS_UTILS_H

// ============================================================================
// BrnTrafficMathsUtils.h -- OWNING HEADER (NEW, cluster C3, wave T1).
//
// The traffic module's small geometry helpers. Free functions in namespace BrnTraffic,
// all of them inline on the console (none appears in the ARTIST ledger; the module's
// callers carry them folded in).
//
// -------------------------------------------------------------------------------------
// WHAT LANDED, AND WHY SO LITTLE. The wave brief asked for this file "from the leak", and
// the leak's copy (references/Feb-2007/.../BrnTrafficMathsUtils.h, 74 lines) holds exactly
// one function, `GetLineLineIntersectionParamXZ`. It is NOT ported, for two independent
// reasons, either of which alone would be disqualifying:
//
//   1. VERSION DRIFT -- the ship file is a different file. The DecFIGS DWARF for this
//      header names two functions the leak does not have, and names them at lines :80 and
//      :191, so the shipped file is at least 191 lines against the leak's 74. The leak's
//      single helper appears nowhere in the DWARF. Copying a Feb-2007 body into a header
//      that grew ~2.5x is the version-drift trap this project has been bitten by before.
//   2. UNSATISFIED VENDOR DEPENDENCY -- the leaked body is written entirely in Vector2:
//      Normalize(Vector2), Vector2 - Vector2, IsSimilar(VecFloat, f32) and MAX_FLOAT.
//      This tree's vendor/renderware/include/rw/math/vpu/vector2_operation.h carries ONE
//      function, Dot. Porting the body would mean growing a vendor header cluster C3 does
//      not own, to serve a body no reconstructed caller wants yet.
//
// So the file lands as the DWARF-shaped owning home with its two ship functions
// DECLARED-ONLY -- the same convention this directory already uses for
// LaneRung::EndianSwap (BrnTrafficSection.h) and the six StaticTrafficVehicle /
// VehicleType endian shims (cluster C2). The declaration is the exact DWARF shape; only
// the body is missing, and it lands with the first reconstructed caller (both are wave-2
// driving-traffic helpers -- the squished-cone test is the avoidance/awareness predicate).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

namespace BrnTraffic
{
    // DWARF BrnTrafficMathsUtils.h:80.
    //   IsPointWithinSquishedCone(Vector3, Vector3, VecFloat, VecFloat, VecFloat, Vector3)
    // DECLARED-ONLY: inlined at every ARTIST call site, so there is no out-of-line body to
    // decompile, and every attested call site (the avoidance / crash-awareness legs of
    // TrafficEntityModule) is itself unreconstructed. Recovering it means decompiling the
    // fold-in inside one of those legs -- a wave-2 job, listed in the C3 report's parks.
    // Parameter NAMES are not carried by the DWARF; positions are.
    bool IsPointWithinSquishedCone(Vector3 lPoint,
                                   Vector3 lConeOrigin,
                                   VecFloat lfParam2,
                                   VecFloat lfParam3,
                                   VecFloat lfParam4,
                                   Vector3 lConeDirection);

    // DWARF BrnTrafficMathsUtils.h:191.
    //   Convert3DVectorTo2D(Vector3, Vector2&)
    // DECLARED-ONLY, same reason.
    void Convert3DVectorTo2D(Vector3 lVector, Vector2& lrOut2D);
}

#endif // BRN_TRAFFIC_MATHS_UTILS_H
