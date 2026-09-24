// Harness for run_fxxlane_is_normal.py (crash parity G07-D1 leftover; FX-XLANE).
//
// BrnMath::IsNormal(Vector3) @0x822B1CF8 and IsNormal(Vector2) (sub_8276AC48; TestCarHNG's :2216
// assert), extracted from the real BrnMathUtils.cpp. Both run the same console pipeline:
//   mag^2 (vmsum3fp128 / lanes 0+1) -> mag = mag^2 * rsqrt(mag^2), vsel 0 when mag^2 == 0
//   -> |mag - 1.0| (flt_82001C98, vandc) -> vcmpgtfp > flt_82002138 (x360rd 0x3C23D70A = 0.01)
//   -> cntlzw: TRUE unless the compare is greater (so NaN and inf read as normal).
#include "GameSource/Math/BrnMathUtils.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { return 0; }
void* EndAssert() { return nullptr; }
} }

#include "methods.inc"

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}
static Vector3 V3(float x, float y, float z) { Vector3 v; v.x = x; v.y = y; v.z = z; v.w = 0.0f; return v; }
static Vector2 V2(float x, float y, float z = 0.0f, float w = 0.0f) { Vector2 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }

int main()
{
    const float lfNaN = std::numeric_limits<float>::quiet_NaN();
    const float lfInf = std::numeric_limits<float>::infinity();

    // ---- IsNormal(Vector3) @0x822B1CF8 ----
    Check(BrnMath::IsNormal(V3(1.0f, 0.0f, 0.0f)), "V3 (1,0,0) is normal (control)");
    Check(BrnMath::IsNormal(V3(0.0f, 1.005f, 0.0f)), "V3 |v| = 1.005 within 0.01 (control)");
    Check(!BrnMath::IsNormal(V3(0.0f, 0.0f, 1.02f)), "V3 |v| = 1.02 outside 0.01 (control)");
    Check(!BrnMath::IsNormal(V3(0.0f, 0.0f, 0.0f)), "V3 zero vector: vsel -> magnitude 0 -> not normal (control)");
    Check(BrnMath::IsNormal(V3(lfNaN, 0.0f, 0.0f)), "V3 NaN reads normal (vcmpgtfp false -> cntlzw TRUE @0x822B1DB8..DCC)");
    Check(BrnMath::IsNormal(V3(lfInf, 0.0f, 0.0f)), "V3 inf: mag^2 * rsqrt(mag^2) = inf * 0 = NaN -> normal");

    // ---- IsNormal(Vector2) sub_8276AC48 ----
#ifdef FXXLANE_HAS_IS_NORMAL_VECTOR2
    Check(BrnMath::IsNormal(V2(0.6f, 0.8f)), "V2 (0.6,0.8) is normal");
    Check(BrnMath::IsNormal(V2(1.0f, 0.0f, 5.0f, 7.0f)), "V2 reads lanes x and y only (z/w ignored)");
    Check(!BrnMath::IsNormal(V2(0.0f, 1.02f)), "V2 |v| = 1.02 outside 0.01");
    Check(!BrnMath::IsNormal(V2(0.0f, 0.0f)), "V2 zero vector: not normal");
    Check(BrnMath::IsNormal(V2(lfNaN, 0.0f)), "V2 NaN reads normal");
#else
    for (int li = 0; li < 5; ++li)
        Check(false, "V2 IsNormal(Vector2) has no body in this revision (sub_8276AC48 missing)");
#endif

    std::printf("FxXlaneIsNormal: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
