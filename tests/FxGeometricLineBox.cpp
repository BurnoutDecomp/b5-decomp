// FX-GEOMETRIC (crash parity 2026-09-24): CgsGeometric::TestLineStartEndAxisAlignedBox @0x82812498, the
// segment-vs-AABB slab test that PolygonSoupListSpatialMap::RunQuery(const Line&) @0x82843E98 and the long
// arm of BaseCollisionGenerator::CollideLineAgainstPolySoupList @0x82812AE0 inline per node / per leaf.
// run_fxgeometric_line_box.py pastes the WHOLE production CgsLineTests.cpp (working tree, or --pre-fix <rev>)
// into fxg_linebox_source.inc; this harness only supplies the assert hooks and the checks.
//
// Every expectation is the ARTIST asm's, not a re-derivation of slab geometry:
//   reciprocal  vrefp128 + three Newton-Raphson steps (0x828124FC..0x82812538): a ZERO direction lane
//               refines to NaN, an INFINITE one to NaN too -> "Line reciprocal X is 0" can never fire for it
//   inside      (P >= min) & !(P > max)                  per lane
//   onSegment   (1 >= t) & !(0 > t)
//   result      startInside | endInside | the six faces  (0x828128BC; the endInside OR is v9, 0x82812880)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
}

#include "fxg_linebox_source.inc"   // the production CgsLineTests.cpp, verbatim

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static int CountAsserts(const char* lpcNeedle)
{
    int n = 0;
    for (const std::string& s : gaAsserts) if (s.find(lpcNeedle) != std::string::npos) ++n;
    return n;
}
static f32 Bits(u32 luBits) { f32 lf; std::memcpy(&lf, &luBits, sizeof(lf)); return lf; }
static Vector4 V(f32 x, f32 y, f32 z) { return Vector4{ x, y, z, 0.0f }; }
static CgsGeometric::AxisAlignedBox Box(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1)
{
    CgsGeometric::AxisAlignedBox b;
    b.mMin = Vector4{ x0, y0, z0, 0.0f };
    b.mMax = Vector4{ x1, y1, z1, 0.0f };
    return b;
}
static bool Test(const Vector4& s, const Vector4& e, const CgsGeometric::AxisAlignedBox& b)
{
    return CgsGeometric::TestLineStartEndAxisAlignedBox(s, e, b);
}

int main()
{
    const f32 lfInf = std::numeric_limits<f32>::infinity();
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
    const CgsGeometric::AxisAlignedBox lUnit = Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

    // ---- the place-on-track shape: a VERTICAL segment (two zero direction lanes) ----------------
    gaAsserts.clear();
    Check(Test(V(0.5f, 60.0f, 0.5f), V(0.5f, -40.0f, 0.5f), lUnit),
          "V1 vertical 100 m line through the box's xz footprint hits it (the Y faces carry it)");
    Check(!Test(V(1.5f, 60.0f, 0.5f), V(1.5f, -40.0f, 0.5f), lUnit),
          "V2 vertical line outside the xz footprint misses");
    Check(!Test(V(0.5f, 60.0f, 0.5f), V(0.5f, 10.0f, 0.5f), lUnit),
          "V3 vertical segment ending ABOVE the box misses");
    Check(Test(V(0.5f, 60.0f, 1.0f), V(0.5f, -40.0f, 1.0f), lUnit),
          "V4 a line ON the max-z face is inside (!(z > max) keeps the boundary)");
    Check(!Test(V(0.5f, 60.0f, -1.0e-6f), V(0.5f, -40.0f, -1.0e-6f), lUnit),
          "V5 a hair below min z is outside ((z >= min) is strict about the boundary's other side)");
    Check(gaAsserts.empty(),
          "V6 zero direction lanes refine to NaN on the console -> no 'Line reciprocal' tripwire fires");

    // ---- endpoints -------------------------------------------------------------------------------
    Check(Test(V(0.25f, 0.25f, 0.25f), V(5.0f, 5.0f, 5.0f), lUnit), "E1 a start inside the box is a hit");
    Check(Test(V(0.5f, 0.5f, 0.5f), V(0.5f, 0.5f, 0.5f), lUnit), "E2 a zero-length segment inside is a hit");
    Check(!Test(V(2.0f, 2.0f, 2.0f), V(2.0f, 2.0f, 2.0f), lUnit), "E3 a zero-length segment outside is not");

    // ---- the END-INSIDE term (0x82812880: v9 = endIn.x & endIn.y & endIn.z, OR'd into the result) ---
    // Start outside, end on the max corner. Float32 rounding puts all three face parameters a hair
    // past 1.0, so no face test accepts; only the end-inside term reports the hit. (Found by a float32
    // search; the three start lanes are pinned by bit pattern.)
    gaAsserts.clear();
    {
        const Vector4 lStart = V(Bits(0x3FABD01Bu), Bits(0x40BB75BCu), Bits(0x40F8C119u));   // (1.34229, 5.85812, 7.77357)
        Check(Test(lStart, V(1.0f, 1.0f, 1.0f), lUnit),
              "N1 end on the max corner, start outside, no face accepted -> the end-inside term still hits");
        Check(Test(V(Bits(0x41D12695u), Bits(0x422F0ECCu), Bits(0x42458CECu)), V(1.0f, 1.0f, 1.0f), lUnit),
              "N2 a second rounding case of the same shape (start 26.14/43.76/49.39)");
    }
    // An INFINITE start lane: the console's reciprocal of an infinite direction refines to NaN (vrefp(inf)=0,
    // then 1 - inf*0), so the faces cannot accept and the X tripwire cannot fire; the end inside still hits.
    gaAsserts.clear();
    Check(Test(V(-lfInf, 0.5f, 0.5f), V(0.5f, 0.5f, 0.5f), lUnit),
          "N3 start at -inf x, end inside -> hit through the end-inside term");
    Check(CountAsserts("Line reciprocal X is 0") == 0,
          "N4 ... and no 'Line reciprocal X is 0' tripwire (the refined reciprocal is NaN, not 0)");

    // ---- faces and NaN polarity ------------------------------------------------------------------
    Check(Test(V(-1.0f, 0.5f, 0.5f), V(2.0f, 0.5f, 0.5f), lUnit), "F1 a segment straight through two x faces");
    Check(!Test(V(-1.0f, 1.5f, 0.5f), V(2.0f, 1.5f, 0.5f), lUnit), "F2 the same segment above the box misses");
    Check(!Test(V(-3.0f, 0.5f, 0.5f), V(-2.0f, 0.5f, 0.5f), lUnit), "F3 a segment that stops short misses (t > 1)");
    Check(!Test(V(3.0f, 0.5f, 0.5f), V(2.0f, 0.5f, 0.5f), lUnit), "F4 a segment pointing away misses (t < 0)");
    Check(!Test(V(lfNaN, 0.5f, 0.5f), V(lfNaN, 0.5f, 0.5f), lUnit),
          "F5 a NaN segment is never inside and never on a face");
    Check(Test(V(-1.0f, -1.0f, 0.5f), V(2.0f, 2.0f, 0.5f), lUnit), "F6 a diagonal through the xy corner region");

    std::printf("FxGeometricLineBox: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
