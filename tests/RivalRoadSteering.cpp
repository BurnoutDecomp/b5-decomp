#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"
#include "GameSource/World/AI/Route/BrnRacingLine.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { std::fprintf(stderr, "%s\n", lpcMessage); std::abort(); }
void* EndAssert() { return nullptr; }
}
// The extracted GenerateFanVectors / IncludeCentreLineTracking carry env-gated PC witnesses
// (BRN_AI_NAN, [aidrv]); a null print sink keeps them silent and the stream operators link.
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
StrStreamBase& StrStreamBase::operator<<(s32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(f32) { return *this; }
}
namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetUsefulDirection() const { return mDirection; }
}
#include "restored_methods.inc"

int main()
{
    using namespace BrnAI;
    s32 liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcLabel) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcLabel); }
    };
    auto Near = [](f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) < 0.001f; };
    AICar lCar{};
    SteeringFan lFan{};
    lFan.mfReciprocalSteps = 1.0f / 16.0f;
    lFan.mfFanAngle = 1.3962634f;
    lFan.mfLookAheadRadius = 20;
    lFan.mfLookAheadHNGRadius = 15;
    // Non-axis-aligned headings cover all quadrants; ARTIST's exact antiparallel
    // angle early-out is outside this reconstruction change.
    const f32 lafHeadings[] = {0, 0.4f, 1.2f, 1.5707963f, 2.4f, -0.4f, -1.2f, -1.5707963f, -2.4f};
    for (f32 lfHeading : lafHeadings)
    {
        lCar.mPosition = {3020, 75, -2180, 0};
        lCar.mDirection = {std::sin(lfHeading), 0, std::cos(lfHeading), 0};
        lFan.GenerateFanVectors(&lCar);
        Check(Near(lFan.mUnitDirection[8].x, lCar.mDirection.x) &&
              Near(lFan.mUnitDirection[8].y, lCar.mDirection.z), "centre ray follows vehicle heading");
        Check(Near(lFan.mCentreTarget.x, lCar.mPosition.x + lCar.mDirection.x * 20) &&
              Near(lFan.mCentreTarget.y, lCar.mPosition.z + lCar.mDirection.z * 20), "road target is ahead on XZ plane");
        const auto& lrLeft = lFan.mUnitDirection[0];
        const auto& lrRight = lFan.mUnitDirection[16];
        Check(lCar.mDirection.x * lrLeft.y - lCar.mDirection.z * lrLeft.x > 0 &&
              lCar.mDirection.x * lrRight.y - lCar.mDirection.z * lrRight.x < 0,
              "fan rays bracket the heading with original handedness");
    }
    RacingLine lLine{};
    lLine.mbIsInitialised = true;
    lLine.mfCentreLineAhead = 0.5f;
    lLine.mfCentreLineAheadRecip = 2.0f;
    lFan.mbCentreHereKnown = true;
    lFan.mFanOrigin = {10, 70, -2000, 0};
    lFan.mCentreHere = {10, -1990, 0, 0};
    for (s32 liRay = 0; liRay < KI_FAN_STEPS; ++liRay)
        lFan.mUnitDirection[liRay] = {0, 1, 0, 0};
    lFan.IncludeCentreLineTracking(nullptr, &lLine);
    Check(Near(lFan.mfWeighting[eFan_SteerToCentre][8], -1), "centre tracking flattens world Z rather than height");
    lFan.mFanOrigin = {1010, -700, 3000, 0};
    lFan.mCentreHere = {1010, 3010, 0, 0};
    lFan.IncludeCentreLineTracking(nullptr, &lLine);
    Check(Near(lFan.mfWeighting[eFan_SteerToCentre][8], -1), "centre weighting is invariant under world translation and height");
    lFan.mCentreAhead = {1010, 3011, 0, 0};
    lFan.IncludeRouteParallelTracking(nullptr, &lLine);
    Check(Near(lFan.mfWeighting[eFan_DriveParallel][8], 0), "forward road direction has no parallel penalty");
    lFan.mUnitDirection[8] = {0, -1, 0, 0};
    lFan.IncludeRouteParallelTracking(nullptr, &lLine);
    Check(Near(lFan.mfWeighting[eFan_DriveParallel][8], 1), "backwards road direction has full parallel penalty");
    lFan.mbCentreHereKnown = false;
    lFan.IncludeCentreLineTracking(nullptr, &lLine);
    Check(Near(lFan.mfWeighting[eFan_SteerToCentre][8], 0), "unknown centre clears tracking weight");
    std::printf("%s: %d road-steering checks, %d failures\n", liFailures ? "FAIL" : "PASS", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
