// FX-AINAN2 regression (crash parity 2026-09-24): RouteRequestManager::ChooseDistanceFunction
// @0x82788CC0, extracted verbatim from BrnRouteRequestManager.cpp by run_fxainan2_route_request.py.
//
// Console: lbDrivingAway is `vcmpgtfp. v0, splat(var_80.x), dot(diff, dir)` @0x82788DA0 with
// var_80 = {flt_82001CC0 (0.0), 0, 0, 0} (0x82788D68..0x82788D8C) -- the destination is BEHIND
// the heading. flt_820C4318 (200.0) is written into var_80 only afterwards (0x82788DBC/0x82788DC0)
// for the countryside test `200 > end.x && 200 > start.x` (0x82788DE8 / 0x82788E2C). The PC
// compared the dot against 200, so a destination less than 200 m ahead along the heading took
// the "driving away" (dominant-offset-axis) arm instead of the 2x / heading-axis arm.
// Result encoding: 0 EUCLIDEAN, 1 X_BIASED, 2 Y_BIASED.
#include "GameSource/World/AI/BrnRouteRequestManager.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace
{
    Vector3 gDestination = { 0.0f, 0.0f, 0.0f, 0.0f };
}
namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
Vector3 AISection::GetMiddle() const { return gDestination; }
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

    alignas(16) unsigned char gaManager[sizeof(RouteRequestManager)];
    AICar gCar{};
    AISection gaSections[1];
    AISectionsData gData;

    int Choose(Vector3 lCarPosition, Vector3 lCarDirection, Vector3 lDestination)
    {
        gCar.mPosition = lCarPosition;
        gCar.mDirection = lCarDirection;
        gDestination = lDestination;
        gData.mpaSections = gaSections;
        gData.muNumSections = 1;
        return static_cast<int>(reinterpret_cast<RouteRequestManager*>(gaManager)->ChooseDistanceFunction(
            &gCar, &gData, E_ASTAR_DISTANCE_EUCLIDEAN, 0));
    }
}

int main()
{
    std::memset(gaManager, 0, sizeof(gaManager));
    const Vector3 kHeadingZ = { 0.0f, 0.0f, 1.0f, 0.0f };
    const Vector3 kCar = { 1000.0f, 0.0f, 0.0f, 0.0f };   // city half (x >= 200)

    Check(Choose(Vector3{ 50.0f, 0.0f, 0.0f, 0.0f }, kHeadingZ, Vector3{ 150.0f, 0.0f, 60.0f, 0.0f }) == 0,
          "control: both ends in the countryside (x < 200) -> EUCLIDEAN");
    // Offset (100, 60) along heading +Z: dot 60 -- ahead, but less than 200.
    Check(Choose(kCar, kHeadingZ, Vector3{ 1100.0f, 0.0f, 60.0f, 0.0f }) == 2,
          "destination 60 m AHEAD: 0 > 60 is false -> the toward arm; no axis dominates 2x -> heading axis Z -> Y_BIASED");
    // Offset (100, -60): dot -60 -- behind.
    Check(Choose(kCar, kHeadingZ, Vector3{ 1100.0f, 0.0f, -60.0f, 0.0f }) == 1,
          "control: destination BEHIND -> driving away -> dominant offset axis X -> X_BIASED");
    // Offset (500, 300) along heading +Z: dot 300 > 200 -- both spellings take the toward arm.
    Check(Choose(kCar, kHeadingZ, Vector3{ 1500.0f, 0.0f, 300.0f, 0.0f }) == 2,
          "control: destination 300 m ahead -> toward arm -> heading axis -> Y_BIASED");
    Check(Choose(kCar, kHeadingZ, Vector3{ 1500.0f, 0.0f, 100.0f, 0.0f }) == 1,
          "control: |dx| 500 > 2 * |dz| 100 -> X_BIASED on the toward arm");
    Check(Choose(kCar, Vector3{ KF_NAN, 0.0f, KF_NAN, 0.0f }, Vector3{ 1100.0f, 0.0f, 60.0f, 0.0f }) == 1,
          "control: a NaN heading -> not driving away, no 2x axis, NaN > NaN false -> X_BIASED");

    std::printf("FxAinan2RouteRequest: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
