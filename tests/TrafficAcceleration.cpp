// Numerical checks on the production acceleration body. The receiver fixture
// supplies the real pool/member types without constructing the entire world module.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#undef private
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{ std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); std::abort(); }
void* EndAssert() { return nullptr; }
} }

namespace BrnTraffic {
inline CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }
struct AccelerationFixture
{
    decltype(TrafficEntityModule::mfSpeedMultiplier) mfSpeedMultiplier = 1;
    decltype(TrafficEntityModule::mbAllowDivergentBehaviour) mbAllowDivergentBehaviour = true;
    decltype(TrafficEntityModule::mbPlayingShowtimeMode) mbPlayingShowtimeMode = false;
    decltype(TrafficEntityModule::mbAtStartLineSoProtectRaceCarsFromTraffic) mbAtStartLineSoProtectRaceCarsFromTraffic = false;
    decltype(TrafficEntityModule::meLocalPlayerIndex) meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    decltype(TrafficEntityModule::mRaceCarState) mRaceCarState{};
    decltype(TrafficEntityModule::maParamTransforms) maParamTransforms{};
    decltype(TrafficEntityModule::maVehicles) maVehicles{};
    decltype(TrafficEntityModule::maVehicleTransforms) maVehicleTransforms{};
    decltype(TrafficEntityModule::mTweakValues) mTweakValues{};
    Vehicle* GetVehicle(u32);
    const Vehicle* GetVehicle(u32) const;
    const ParamTransform* GetParamTransform(u32) const;
    Matrix44Affine GetVehicleTransform(u32) const;
    f32 UpdateParams_CalcAcceleration(u32, const Param*, const Section*,
        const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>&) const;
};
// Only used when the runner explicitly loads a pre-fix source snapshot.
inline void LogMissingLeg_T2(bool&, const char*) {}
}
#include "traffic_acceleration_methods.inc"

int main()
{
    using namespace BrnTraffic;
    auto lpFixture = std::make_unique<AccelerationFixture>();
    auto& lrFixture = *lpFixture;
    lrFixture.mTweakValues.mfMinNormalAcceleration = -1000;
    lrFixture.mTweakValues.mfMaxNormalAcceleration = 1000;
    lrFixture.mTweakValues.mfMinAcceleration = -1000;
    lrFixture.mTweakValues.mfMaxAcceleration = 1000;
    lrFixture.mTweakValues.mfMinStopDist = 1;
    Section lSection{}; lSection.mfSpeed = 30;
    Param lParam{}; lParam.miBehaviour = 6; lParam.mfSpeed = 20;
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lAvoid;
    u32 luChecks = 0, luFailures = 0;
    auto Check = [&](u32 luIndex, f32 lfExpected, const char* lpcLabel) {
        ++luChecks;
        const f32 lfActual = lrFixture.UpdateParams_CalcAcceleration(luIndex, &lParam, &lSection, lAvoid);
        if (!std::isfinite(lfActual) || std::fabs(lfActual - lfExpected) > 0.0002f)
        { ++luFailures; std::printf("FAIL %s [%u]: got %.8g expected %.8g\n", lpcLabel, luIndex, lfActual, lfExpected); }
    };
    for (u32 luIndex : {0u, 63u, 64u, 127u, 399u})
    {
        lAvoid.Construct(); lAvoid.SetBit(luIndex);
        auto& lrVehicle = lrFixture.maVehicles[luIndex];
        lrVehicle.mxFlags = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
        lrVehicle.muCrashTrafficType = 3; lrVehicle.miPhysicalReason = 0;
        auto& lrTransform = lrFixture.maParamTransforms[luIndex];
        lrTransform.mLerpedPosAndSpeed.SetVector3({10, 50, 30, 0});
        lrTransform.mDirAndAccel.SetVector3({0, 0, 1, 0});
        auto& lrVehicleTransform = lrFixture.maVehicleTransforms[luIndex];
        lrVehicleTransform.SetIdentity();
        // Expected outputs independently evaluated from ARTIST's piecewise target.
        const struct { f32 lfDistance, lfAcceleration; } laCases[] = {
            {-80, -14}, {-50, -14}, {-20, -5}, {-19.99f, 10}, {-1, 10},
            {0, 5}, {10, 10}, {100, 20.2336f}
        };
        for (const auto& lrCase : laCases)
        {
            lrVehicleTransform.wAxis = {10, 50, 30 + lrCase.lfDistance, 0};
            Check(luIndex, lrCase.lfAcceleration, "slam recovery distance");
        }
        lrVehicleTransform.wAxis.z = 10; // distance -20
        lrVehicle.muCrashTrafficType = 0; lrVehicle.miPhysicalReason = 4;
        Check(luIndex, -5, "extreme swerve follows recovery rule");
        lrVehicle.miPhysicalReason = 0;
        Check(luIndex, 10, "normal vehicle excluded");
        lrVehicle.muCrashTrafficType = 3;
        lAvoid.UnSetBit(luIndex);
        Check(luIndex, 10, "avoid-set exclusion");
        lAvoid.SetBit(luIndex);
        lrFixture.mbAllowDivergentBehaviour = false;
        Check(luIndex, 10, "divergent gate disabled");
        lrFixture.mbAllowDivergentBehaviour = true;

        lParam.miBehaviour = 1; lParam.mfTargetSpeed = 40; lParam.mfStopDist = 10;
        Check(luIndex, 0, "slowing target scaled before squared-speed solve");
        lParam.mfStopDist = 0.5f;
        Check(luIndex, -1000, "minimum stop distance");
        lParam.miBehaviour = 6;

        lrFixture.mbPlayingShowtimeMode = true;
        lrFixture.meLocalPlayerIndex = static_cast<EActiveRaceCarIndex>(2);
        const struct { Vector3 lPosition; f32 lfAcceleration; } laShowtime[] = {
            {{10, 50, 40, 0}, -8}, {{10, 50, 20, 0}, -11},
            {{50, 50, 30, 0}, 10}, {{10, 50, 30, 0}, 1000},
            {{10, 58, 36, 0}, -8}, {{30, 50, 30, 0}, -2}
        };
        for (const auto& lrCase : laShowtime)
        {
            lrFixture.mRaceCarState.maActiveRaceCarPositions[2] = lrCase.lPosition;
            Check(luIndex, lrCase.lfAcceleration, "Showtime proximity overrides slam");
        }
        // Zero direction normalization produces a NaN cap on ARTIST. Its final
        // fsel chooses the upper acceleration bound in both behavior families.
        lrFixture.mRaceCarState.maActiveRaceCarPositions[2] = {10, 50, 30, 0};
        lParam.miBehaviour = 1; lParam.mfStopDist = 10;
        Check(luIndex, 1000, "coincident Showtime positions preserve fsel outcome");
        lParam.miBehaviour = 6;
        lrFixture.meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        Check(luIndex, -5, "invalid Showtime player falls through to slam");
        lrFixture.mbPlayingShowtimeMode = false;
    }
    std::printf("%s: %u traffic acceleration checks (%u failures)\n", luFailures ? "FAIL" : "PASS", luChecks, luFailures);
    return luFailures ? 1 : 0;
}
