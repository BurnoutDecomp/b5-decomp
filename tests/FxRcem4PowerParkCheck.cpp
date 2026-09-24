// FX-RCEM4 (crash parity 2026-09-24): the Power Parking candidacy test and its line-distance callee.
// run_fxrcem4_power_park_check.py extracts VERBATIM:
//   BrnWorld::CheckVehicleForPowerPark + PowerParkingDetail (ATan2, RwMathFPU constants) and
//   KF_POWER_PARK_NEARBY_RADIUS                             (PowerParking/BrnPowerParkingManager.h)
//   BrnMath::GetPointToInfiniteLineDistance and
//   KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT             (PowerParking/BrnPowerParkingManager.cpp)
// A piece the pre-fix source lacks is replayed as a stub that does nothing (returns false / 0 / NaN).
//   0x822B1FA0 CheckVehicleForPowerPark: x*x + z*z > 225.0 (flt_82018E3C) -> false ; bge/bge (NaN
//              taken) -> true ; closest/second shuffle ; ATan2(dir.z, dir.x) twice ; fabs ; two fsel
//              Min folds (2pi flt_82001C94, pi flt_8201443C) ; asserts :205/:209/:213 (blt fires, ble
//              skips) ; perpendicular = GetPointToInfiniteLineDistance(player, vehicle, vehicle + dir)
//   0x82540448 GetPointToInfiniteLineDistance: assert MagnitudeSquared(dir) > 0 (fires on NaN) ;
//              t = Dot(dir, point - start) (NOT divided by |dir|^2) ; |dir * t + start - point|
//   flt_82CDB4D4 == 3.0 (KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT, DetermineOutcome 0x822A74D0)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/fpu/scalar_operation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAssertions = 0;
static const char* gpcLastAssertion = "";
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; gpcLastAssertion = lpcMessage; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnMath {
#include "fxrcem4_pp_line.inc"
}
namespace BrnWorld {
#include "fxrcem4_pp_kf.inc"
#include "fxrcem4_pp_detail.inc"
#include "fxrcem4_pp_check.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(f32 lfA, f64 lfB, f64 lfTolerance = 1.0e-5) { return std::fabs(static_cast<f64>(lfA) - lfB) <= lfTolerance; }
static Vector3 V(f32 x, f32 y, f32 z) { Vector3 v; v.x = x; v.y = y; v.z = z; v.w = 0.0f; return v; }

struct Slots {
    f32 mfClosest = FLT_MAX, mfSecond = FLT_MAX, mfAngle = -7.0f, mfPerp = -7.0f;   // the producers' seed: flt_820BA23C
    bool Run(Vector3 lPlayerPos, Vector3 lPlayerDir, Vector3 lVehiclePos, Vector3 lVehicleDir) {
        return BrnWorld::CheckVehicleForPowerPark(lPlayerPos, lPlayerDir, lVehiclePos, lVehicleDir,
                                                  mfClosest, mfSecond, mfAngle, mfPerp);
    }
};

int main() {
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
    const Vector3 lOrigin = V(0.0f, 0.0f, 0.0f), lAlongX = V(1.0f, 0.0f, 0.0f), lAlongZ = V(0.0f, 0.0f, 1.0f);

    // ---- the constants, as read from the image --------------------------------------------------------
    Check(BrnWorld::KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT == 3.0f, "KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT == flt_82CDB4D4 (3.0)");

    // ---- GetPointToInfiniteLineDistance (0x82540448) --------------------------------------------------
    guAssertions = 0;
    Check(Near(BrnMath::GetPointToInfiniteLineDistance(V(3, 5, 4), lOrigin, lAlongZ), std::sqrt(34.0)),
          "unit line: the full 3-D distance to the projection (height counts), sqrt(34)");
    Check(Near(BrnMath::GetPointToInfiniteLineDistance(V(3, 0, 4), lOrigin, V(0, 0, 2)), std::sqrt(153.0)),
          "a length-2 direction is NOT normalised: t = Dot = 8, projection (0,0,16), sqrt(153) -- not 3");
    Check(guAssertions == 0, "no assertion for a non-degenerate line");
    const f32 lfDegenerate = BrnMath::GetPointToInfiniteLineDistance(V(1, 2, 2), V(5, 0, 0), V(5, 0, 0));
    Check(guAssertions == 1 && std::strcmp(gpcLastAssertion, "RwMath::MagnitudeSquared( lLineDir ) > 0.0f") == 0,
          "a zero-length line fires the :118 assert (vcmpgtfp. all-true fails)");
    Check(Near(lfDegenerate, std::sqrt(24.0)), "...and answers |start - point| (t = 0), sqrt(24)");
    guAssertions = 0;
    (void)BrnMath::GetPointToInfiniteLineDistance(V(1, 2, 2), lOrigin, V(lfNan, 0, 0));
    Check(guAssertions == 1, "a NaN direction fires the :118 assert too (NaN > 0 is false)");

    // ---- CheckVehicleForPowerPark (0x822B1FA0) --------------------------------------------------------
    guAssertions = 0;
    {
        Slots s;
        Check(!s.Run(lOrigin, lAlongX, V(16, 0, 0), lAlongZ) && s.mfClosest == FLT_MAX && s.mfSecond == FLT_MAX && s.mfAngle == -7.0f,
              "256 > 225 (flt_82018E3C): not a candidate, nothing written (bgt -> li r3, 0)");
        Check(s.Run(lOrigin, lAlongX, V(9, 0, 12), lAlongZ) && s.mfClosest == 225.0f && s.mfSecond == FLT_MAX,
              "exactly 225 is inside the radius (bgt not taken); the first candidate becomes the closest");
    }
    {
        Slots s;
        Check(s.Run(lOrigin, lAlongX, V(0, 100, 3), lAlongZ) && s.mfClosest == 9.0f,
              "the distance is the ground-plane x*x + z*z: 100 m of height is ignored");
    }
    {
        Slots s;
        Check(s.Run(lOrigin, lAlongX, V(3, 0, 4), lAlongZ), "a car 5 m away is a candidate");
        Check(s.mfClosest == 25.0f && s.mfSecond == FLT_MAX, "closest = 25, second = the old closest (stfs f12 / f13)");
        Check(Near(s.mfAngle, 1.5707964), "player heading ATan2(0, 1) = 0, car heading ATan2(1, 0) = pi/2 -> diff pi/2");
        Check(Near(s.mfPerp, 3.0), "perpendicular distance from the player to the car's heading line = 3");

        s.mfAngle = -7.0f; s.mfPerp = -7.0f;
        Check(s.Run(lOrigin, lAlongX, V(0, 0, 6), lAlongX) && s.mfClosest == 25.0f && s.mfSecond == 36.0f,
              "a farther car only takes the second slot (0x822B22A4)");
        Check(s.mfAngle == -7.0f && s.mfPerp == -7.0f, "...and leaves the angle / perpendicular measurements alone");
        Check(s.Run(lOrigin, lAlongX, V(0, 0, 7), lAlongX) && s.mfClosest == 25.0f && s.mfSecond == 36.0f,
              "a car beyond the second still counts (returns true) but ranks nowhere");
        Check(s.Run(lOrigin, lAlongX, V(5, 0, 0), lAlongX) && s.mfClosest == 25.0f && s.mfSecond == 25.0f && s.mfAngle == -7.0f,
              "a tie with the closest is not closer (bge): it becomes the second, no angle refresh");
        Check(s.Run(lOrigin, lAlongX, V(lfNan, 0, 0), lAlongX) && s.mfClosest == 25.0f && s.mfSecond == 25.0f,
              "a NaN distance is not > 225 and bge is taken on NaN: counted, ranks nowhere");
    }
    {
        Slots s;
        Check(s.Run(lOrigin, V(-1, 0, 0.1f), V(0, 0, 2), V(-1, 0, -0.1f)), "headings either side of the -x axis");
        Check(Near(s.mfAngle, 2.0 * std::atan(0.1)), "raw diff 2pi - 2atan(0.1) folds through Min(a, 2pi - a) to 2atan(0.1)");
        Check(Near(s.mfPerp, std::sqrt(0.04 + 1.98 * 1.98)), "the length-sqrt(1.01) heading is used unnormalised: t = 0.2");
    }
    {
        Slots s;
        Check(s.Run(lOrigin, lAlongX, V(0, 0, 3), V(-1, 0, 0)) && Near(s.mfAngle, 0.0),
              "an antiparallel car: diff pi -> Min(pi, pi) -> Min(pi, 0) = 0, i.e. aligned");
    }
    {
        Slots s;
        Check(s.Run(lOrigin, lOrigin, V(0, 0, 3), lAlongX) && Near(s.mfAngle, 1.5707964),
              "a zero player heading is ATan2(0, 0) = copysign(pi/2, 0) (vcmpeqfp arm), not std::atan2's 0");
    }
    Check(guAssertions == 0, "no range assertion for any finite heading");
    {
        Slots s;
        Check(s.Run(lOrigin, V(lfNan, 0, lfNan), V(0, 0, 3), lAlongX), "a NaN player heading is still a candidate");
        Check(s.mfClosest == 9.0f && std::isnan(s.mfAngle), "the closest updates and the angle is NaN");
        Check(guAssertions == 0, "a NaN angle does NOT fire :205/:209/:213 (blt not taken, ble taken on NaN)");
    }

    std::printf("FxRcem4PowerParkCheck: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
