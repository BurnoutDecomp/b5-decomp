// FX-FOLLOWUPS (crash parity 2026-09-25, a NaN-class sweep item from FX-GATE): the two line boxes of
// BaseCollisionGenerator's poly-soup line tests are VMX min / max, not C selects --
//   CollideLineAgainstPolySoupListNearest @0x828131C0: vmaxfp128 v0,v127,v123 (0x82813318) / vminfp128 v13 (0x82813320)
//   CollideLineAgainstPolySoupList        @0x82812AE0: vmaxfp128 v0,v127,v123 (0x82812C20) / vminfp128 v13 (0x82812C2C)
// with v127 = the line start and v123 = the line end (0x82813298 / 0x828132AC `lvx128`). A NaN lane in either
// operand gives a NaN box lane (vA's NaN when both are), and +0 orders above -0; the old selects
// `(s < e) ? s : e` / `(s > e) ? s : e` answered the END for a NaN start lane and ordered the zeros by operand.
// run_fxfollowups_line_box_nan.py pastes each function's PRODUCTION box block (from `AxisAlignedBox lBox;` to the
// RunQuery line) into NearestBox / AllHitsBox below, with the file-local VmxMaxFp / VmxMinFp when the revision
// has them.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "fxfu_linebox.inc"

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPassed, const char* lpcName)
{
    ++giChecks;
    if (!lbPassed) { ++giFailures; std::printf("FAIL  %s\n", lpcName); }
    else           { std::printf("ok    %s\n", lpcName); }
}

static Vector3 V3(f32 x, f32 y, f32 z, f32 w) { Vector3 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
static bool IsNaN(f32 f) { return f != f; }
static u32 Bits(f32 f) { u32 u; std::memcpy(&u, &f, 4); return u; }

typedef void (*BoxFn)(const Vector3&, const Vector3&, CgsGeometric::AxisAlignedBox&);

static void Run(BoxFn lpfnBox, const char* lpcName)
{
    char lac[256];
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
    CgsGeometric::AxisAlignedBox lBox;

    lpfnBox(V3(1, 5, -2, 1), V3(3, -1, -2, 0), lBox);
    std::snprintf(lac, sizeof(lac), "%s: finite lanes -> min {1,-1,-2,0}, max {3,5,-2,1}", lpcName);
    Check(lBox.mMin.x == 1 && lBox.mMin.y == -1 && lBox.mMin.z == -2 && lBox.mMin.w == 0
          && lBox.mMax.x == 3 && lBox.mMax.y == 5 && lBox.mMax.z == -2 && lBox.mMax.w == 1, lac);

    lpfnBox(V3(lfNaN, 5, -2, 1), V3(3, -1, -2, 0), lBox);
    std::snprintf(lac, sizeof(lac), "%s: a NaN START lane (vA) gives a NaN box lane in min AND max (the select answered the end)", lpcName);
    Check(IsNaN(lBox.mMin.x) && IsNaN(lBox.mMax.x) && lBox.mMin.y == -1 && lBox.mMax.y == 5, lac);

    lpfnBox(V3(1, 5, -2, 1), V3(3, lfNaN, -2, 0), lBox);
    std::snprintf(lac, sizeof(lac), "%s: a NaN END lane (vB) gives a NaN box lane in min AND max", lpcName);
    Check(IsNaN(lBox.mMin.y) && IsNaN(lBox.mMax.y) && lBox.mMin.x == 1 && lBox.mMax.x == 3, lac);

    lpfnBox(V3(1, 5, -0.0f, 0.0f), V3(3, -1, 0.0f, -0.0f), lBox);
    std::snprintf(lac, sizeof(lac), "%s: zeros order -0 < +0 whichever operand holds them (min -0, max +0 in z and w)", lpcName);
    Check(Bits(lBox.mMin.z) == 0x80000000u && Bits(lBox.mMax.z) == 0u
          && Bits(lBox.mMin.w) == 0x80000000u && Bits(lBox.mMax.w) == 0u, lac);
}

int main()
{
    Run(NearestBox, "CollideLineAgainstPolySoupListNearest (0x82813318 / 0x82813320)");
    Run(AllHitsBox, "CollideLineAgainstPolySoupList (0x82812C20 / 0x82812C2C)");
    std::printf("FxFollowupsLineBoxNan: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
