// FX-LADDER item 5 regression (crash parity 2026-09-25): AICar::IsExtrapolatedRouteGettingOld
// @0x8276FD50 and the polarity of its drift test. The production bodies (the method and
// AICar::HasValidRoute) are extracted verbatim from BrnAICar_Update.cpp by
// run_fxladder_route_age.py; GetPosition / GetDirection are fixtures returning the members.
//
// ARTIST, read off the asm (not the pseudocode):
//   0x8276FD90..0x8276FDDC  drift = |mLastRoutePosition (+0x1480) - mPosition (+0x1430)|
//                           (rsqrt + 2 Newton steps, vsel 0 when the square is exactly 0)
//   0x8276FD8C/0x8276FDE4   limit = mbIsPlayer (+0x1549) ? flt_820C4244 (50.0) : flt_820C4318 (200.0)
//   0x8276FDFC  fcmpu f13, f0 ; ble 0x8276FE0C   -- ble is `bc 4,gt`, TAKEN whenever GT is clear,
//               i.e. for an ordered drift <= limit AND for an unordered (NaN) drift: both go on to the
//               route tests. Only an ordered drift ABOVE the limit falls through to `li r3, 1` ("old").
//   0x8276FE0C..0x8276FE64  status (+0x1408) != 0 && count (+0x1400) > 0 && count > 1, else old
//   0x8276FE74  count - next (+0x1524) < 1 -> old
//   0x8276FE7C..0x8276FE88  node = min(next + 1, count - 1)
//   0x8276FF14  vcmpgtfp. 0 > dot(direction, (node.x, 0, node.y) - position) -> old
//               (a NaN dot is not "all greater": NOT old)
// cccfeed8 wrote `!(lfDrift <= lfLimit)`, which returns "old" for a NaN drift before the route tests
// run -- the ble misreading REVIEW_A.md logged for five other commits. The console (and `lfDrift >
// lfLimit`) sends a NaN drift on to the route tests; with the fixture's valid route ahead they say
// "not old". The four NaN-drift checks below are RED on cccfeed8.
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace BrnAI {
Vector3 AICar::GetPosition() const  { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();
    Vector3 V(f32 lfX, f32 lfY, f32 lfZ) { return Vector3{ lfX, lfY, lfZ, 0.0f }; }

    AICar gCar;

    // A car at the origin facing world +Z whose route was built where it stands: three nodes
    // straight ahead on the (x, z) ground plane (a RouteNode stores the plane's z as mfY),
    // next node index 0, so the node the test reads (index 1) is 20 m ahead.
    void Reset(bool lbIsPlayer)
    {
        gCar = AICar{};
        gCar.mbIsPlayer = lbIsPlayer;
        gCar.mPosition = V(0.0f, 0.0f, 0.0f);
        gCar.mDirection = V(0.0f, 0.0f, 1.0f);
        gCar.mLastRoutePosition = V(0.0f, 0.0f, 0.0f);
        Route* lpRoute = gCar.GetRoute();
        lpRoute->miNodeCount = 3;
        lpRoute->meStatus = Route::E_STATUS_COMPLETE;
        for (s32 liNode = 0; liNode < 3; ++liNode)
        {
            lpRoute->maNodes[liNode] = Vector4{ 0.0f, 10.0f + 10.0f * static_cast<f32>(liNode), 0.0f, 0.0f };
        }
        gCar.miNextRouteNodeIndex = 0;
    }

    void Drift(f32 lfX, f32 lfZ)
    {
        gCar.mLastRoutePosition = V(lfX, 0.0f, lfZ);
    }
}

int main()
{
    // ---- the drift test @0x8276FDFC ------------------------------------------------------
    Reset(false);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "AI, fresh route ahead -> not old");
    Reset(false); Drift(150.0f, 0.0f);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "AI, drift 150 <= 200 -> not old (route ahead)");
    Reset(false); Drift(200.0f, 0.0f);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "AI, drift exactly 200 -> ble taken -> not old");
    Reset(false); Drift(250.0f, 0.0f);
    Check(gCar.IsExtrapolatedRouteGettingOld(), "AI, drift 250 > 200 -> old");
    Reset(true); Drift(40.0f, 0.0f);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "player, drift 40 <= 50 -> not old");
    Reset(true); Drift(60.0f, 0.0f);
    Check(gCar.IsExtrapolatedRouteGettingOld(), "player, drift 60 > 50 -> old");

    // A NaN drift: the ble IS taken on unordered, so the console goes on to the route tests. With a
    // valid route ahead they answer "not old": for a NaN position the facing dot is NaN and
    // `vcmpgtfp. 0 > dot` is not all-true (0x8276FF14); for a NaN route snapshot the dot is +20.
    Reset(false); gCar.mPosition = V(KF_NAN, 0.0f, 0.0f);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "AI, NaN position -> NaN drift -> route tests -> not old");
    Reset(true); gCar.mPosition = V(0.0f, KF_NAN, 0.0f);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "player, NaN position -> NaN drift -> route tests -> not old");
    Reset(false); gCar.mLastRoutePosition = V(0.0f, 0.0f, KF_NAN);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "AI, NaN route snapshot -> NaN drift -> route ahead -> not old");
    Reset(true); gCar.mLastRoutePosition = V(KF_NAN, KF_NAN, KF_NAN);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "player, NaN route snapshot -> route ahead -> not old");
    // ... and the route tests still decide for a NaN drift: no valid route -> old, facing away -> old.
    Reset(false); gCar.mPosition = V(KF_NAN, 0.0f, 0.0f); gCar.GetRoute()->meStatus = Route::E_STATUS_UNINITIALISED;
    Check(gCar.IsExtrapolatedRouteGettingOld(), "AI, NaN drift, no valid route -> old (route test)");
    Reset(true); gCar.mLastRoutePosition = V(KF_NAN, 0.0f, 0.0f); gCar.mDirection = V(0.0f, 0.0f, -1.0f);
    Check(gCar.IsExtrapolatedRouteGettingOld(), "player, NaN drift, facing away -> old (route test)");

    // ---- the route tests @0x8276FE0C..0x8276FF14 (ordered controls, both bodies agree) ----
    Reset(false); gCar.GetRoute()->meStatus = Route::E_STATUS_UNINITIALISED;
    Check(gCar.IsExtrapolatedRouteGettingOld(), "no valid route -> old");
    Reset(false); gCar.GetRoute()->miNodeCount = 1;
    Check(gCar.IsExtrapolatedRouteGettingOld(), "one-node route -> old");
    Reset(false); gCar.miNextRouteNodeIndex = 3;
    Check(gCar.IsExtrapolatedRouteGettingOld(), "next index at the count -> old");
    Reset(false); gCar.miNextRouteNodeIndex = 2;
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "next = last -> node clamped to count-1, still ahead -> not old");
    Reset(false); gCar.mDirection = V(0.0f, 0.0f, -1.0f);
    Check(gCar.IsExtrapolatedRouteGettingOld(), "facing away from the next node -> old");
    Reset(false); gCar.mDirection = V(KF_NAN, KF_NAN, KF_NAN);
    Check(!gCar.IsExtrapolatedRouteGettingOld(), "NaN facing -> vcmpgtfp. not all true -> not old");

    std::printf("%s: %u/%u checks passed\n", guFailures ? "FAIL" : "PASS", guChecks - guFailures, guChecks);
    return guFailures ? 1 : 0;
}
