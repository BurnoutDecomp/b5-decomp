// FX-TRAFFIC (crash parity 2026-09-23, G58-X1): the PRODUCTION seed of
// TrafficEntityModule::kfParamSympatheticConeShowTime_CosAngle_Length_RecipYScale_W (+0x726F0),
// extracted from TrafficEntityModule::Construct by run_fxtraffic_showtime_cone.py, applied
// through the production SetTuningLanes and read through the production (header-inline)
// IsPointWithinSquishedCone -- the test UpdateParams_TryStartSympatheticCrashing makes with it
// in Showtime (lane z == the recip-Y scale, 0x82716764..0x827167BC; 0x827147A0 vmulfp128).
//
// ARTIST Construct @0x82740220, r11 = this+0x726F0 (0x827405D4, unchanged to 0x82740680):
//   lane 0  cos(dbl_820BFBF0 == 20 deg)         stfs 0x827405F4
//   lane 1  flt_820BA5C0 == 50.0f               stfs 0x82740610 ; member write 0x82740620
//   lane 2  flt_820BA544 == 0x3E800000 == 0.25f stfs 0x82740670 ; member write 0x82740680
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

namespace BrnTraffic
{
#include "showtime_cone.inc"   // the production SetTuningLanes + SeedShowtimeCone(Vector4&)
}

static VecFloat Splat(f32 lf) { return VecFloat{ lf, lf, lf, lf }; }

int main()
{
    Vector4 lCone = { -1.0f, -1.0f, -1.0f, -1.0f };
    BrnTraffic::SeedShowtimeCone(lCone);

    u32 luLaneZ = 0;
    std::memcpy(&luLaneZ, &lCone.z, sizeof(luLaneZ));
    Check(luLaneZ == 0x3E800000u, "lane z (recip-Y scale) is flt_820BA544 == 0x3E800000 (0.25f) bit-exact  @0x82740670");
    Check(lCone.y == 50.0f, "lane y (length) is flt_820BA5C0 == 50.0f  @0x82740610");
    Check(std::fabs(lCone.x - static_cast<f32>(std::cos(0.3490658476948738))) == 0.0f,
          "lane x is cos(dbl_820BFBF0 == 20 degrees)  @0x827405F4");

    const Vector3 lOrigin    = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector3 lDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
    const VecFloat lfCos = Splat(lCone.x), lfLength = Splat(lCone.y), lfRecipY = Splat(lCone.z);

    // A crashing thing 20 m up, 10 m ahead: the console squashes it to (0,5,10), cos 0.894 <
    // cos 20 deg (0.9397) -> OUTSIDE. With a 0.0 scale it flattens to (0,0,10), cos 1 -> inside.
    const Vector3 lHigh = { 0.0f, 20.0f, 10.0f, 0.0f };
    Check(!BrnTraffic::IsPointWithinSquishedCone(lOrigin, lDirection, lfCos, lfLength, lfRecipY, lHigh),
          "Showtime: a crashing thing 20 m above, 10 m ahead is OUTSIDE the squished cone");
    // 2 m up, 10 m ahead: squashed to (0,0.5,10), cos 0.99875 -> inside.
    const Vector3 lLow = { 0.0f, 2.0f, 10.0f, 0.0f };
    Check(BrnTraffic::IsPointWithinSquishedCone(lOrigin, lDirection, lfCos, lfLength, lfRecipY, lLow),
          "Showtime: a crashing thing 2 m above, 10 m ahead is INSIDE");
    // 13 m up, 10 m ahead: squashed y 3.25, cos 0.951 > 0.9397 -> inside; 15 m: y 3.75, cos 0.936 -> outside.
    const Vector3 lEdgeIn  = { 0.0f, 13.0f, 10.0f, 0.0f };
    const Vector3 lEdgeOut = { 0.0f, 15.0f, 10.0f, 0.0f };
    Check(BrnTraffic::IsPointWithinSquishedCone(lOrigin, lDirection, lfCos, lfLength, lfRecipY, lEdgeIn)
              && !BrnTraffic::IsPointWithinSquishedCone(lOrigin, lDirection, lfCos, lfLength, lfRecipY, lEdgeOut),
          "Showtime: the height cut-off sits between 13 m and 15 m at 10 m range (y * 0.25)");
    // Length: 49 m ahead in, 51 m out (lane y), independent of the Y scale.
    const Vector3 lNear = { 0.0f, 0.0f, 49.0f, 0.0f };
    const Vector3 lFar  = { 0.0f, 0.0f, 51.0f, 0.0f };
    Check(BrnTraffic::IsPointWithinSquishedCone(lOrigin, lDirection, lfCos, lfLength, lfRecipY, lNear)
              && !BrnTraffic::IsPointWithinSquishedCone(lOrigin, lDirection, lfCos, lfLength, lfRecipY, lFar),
          "Showtime: the cone is 50 m long");

    std::printf("FxTrafficShowtimeCone: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
