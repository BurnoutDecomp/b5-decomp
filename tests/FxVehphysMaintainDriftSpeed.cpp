// FX-VEHPHYS (crash parity 2026-09-23, G51-D1): BrnPhysics::Vehicle::VehiclePhysics::
// MaintainDriftSpeed @0x825D2270 -- the velocity term of the drift speed-maintain impulse.
//
// run_fxvehphys_maintain_drift_speed.py EXTRACTS the production MaintainDriftSpeed body from
// VehiclePhysics.cpp and compiles it into DriftFixture, which carries the members the body reads
// with their real types and records AddWorldSpaceImpulse.
//
// Console facts checked (ARTIST asm):
//   0x825D232C vsubfp v13 = splat(+0x1000 .y MaintainedSpeed) - v2   (deficit; v2 = speed param)
//   0x825D2358/2360/2364  v12 = deficit * splat(mpAttribs +0x70 .x)  (the vehicle mass)
//   0x825D2384/238C       v127 = v1(lvDirection) * v12 * splat(+0x1050 .x AlongZ)
//   0x825D2340..2390      |v2| > FLT_EPSILON (stru_8208F620) gates the velocity term
//   0x825D2398 vrefp v0,v2 ; 0x825D23B0..23BC two Newton steps  -> v0 = 1/speedParam
//   0x825D23AC lvx128 v8,[this+0x50] (mLinearVelocity) ; 0x825D23C0 v0 *= v8 ; 0x825D23C4 v0 *= v12
//   0x825D23C8 vmaddfp128 v127 += v0 * splat(+0x1050 .y AlongVel)
//   0x825D2414..2420      v1 = v127 - n * dot3(n, v127)   (n = +0x580 ground normal)
//   0x825D2424 bl AddWorldSpaceImpulse(v1)
// i.e. impulse = P_n( lvDirection*AlongZ + mLinearVelocity/speedParam*AlongVel ) * deficit * mass.
// The velocity term is NOT normalised: its length is |mLinearVelocity| / speedParam.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static std::string gLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { std::printf("ASSERT: %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 1; }
namespace Log
{
    // Harness-only sink for the opt-in [drift-maintain] witness.
    StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gLog += lpcText; return *this; }
    void WriteToLog(const char*) {}
    static DebugPrint sCapture;
    DebugPrint* gpDebugPrint = &sCapture;
}
}

// The production StrStreamBase (DebugPrint's base) so the sink's vtable links.
#include "GameShared/GameClasses/Development/CgsStrStream.cpp"

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // The production gate is the BRN_DRIFT_PROBE latch; here it is armed for case (A) only, so the
    // witness code runs once under the harness (its line is printed, not checked).
    static bool gbDriftProbe = false;
    inline bool DriftProbeArmed() { return gbDriftProbe; }

    struct DriftFixture
    {
        decltype(VehiclePhysics::mDriftFlags)                                   mDriftFlags{};
        decltype(VehiclePhysics::mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale)
                                                                                 mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale{};
        decltype(VehiclePhysics::mbAllWheelsHaveTraction)                       mbAllWheelsHaveTraction = true;
        decltype(VehiclePhysics::mbHandBrake)                                   mbHandBrake = false;
        decltype(VehiclePhysics::mAboveGroundTestResult)                        mAboveGroundTestResult{};
        decltype(VehiclePhysics::mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor)
                                                                                 mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor{};
        decltype(VehiclePhysics::mpAttribs)                                     mpAttribs = nullptr;
        decltype(VehiclePhysics::mLinearVelocity)                               mLinearVelocity{};

        std::vector<Vector3> maImpulses;
        void AddWorldSpaceImpulse(Vector3 lvImpulse) { maImpulses.push_back(lvImpulse); }

        void MaintainDriftSpeed(const BrnPlayerDriverControls* lpControls, Vector3 lvDirection, VecFloat lvfSpeed);
    };

#include "maintain_drift_speed.inc"
}
}

using namespace BrnPhysics::Vehicle;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

struct Case
{
    const char* mpcName;
    double      mfSpeedParam;
    double      maVelocity[3];       // what +0x50 holds when MaintainDriftSpeed runs
    double      mfMaintained;        // +0x1000 .y
    double      maNormal[3];         // +0x580
};

int main()
{
    VehicleAttribs lAttribs{};
    lAttribs.mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce = { 1000.0f, 17.0f, 23.0f, 29.0f };
    const double kfMass = 1000.0, kfAlongZ = -0.1, kfAlongVel = 1.0;   // EnterDrift seeds (unk_8208FAE4 / FAE8)

    // (D) is UpdateDrift's own shape: the speed parameter is |v| BEFORE the sideways damping, the
    // member holds v AFTER it -- the retail DriftSidewaysDamping 0.02 on xAxis = (1,0,0).
    const double kfPre = std::sqrt(10.0 * 10.0 + 20.0 * 20.0);
    const Case kaCases[] = {
        { "(A) |v| = 5, speed param 10: velocity term is v/10, not a unit vector", 10.0, { 3.0, 0.0, 4.0 }, 12.0, { 0.0, 1.0, 0.0 } },
        { "(B) |v| == speed param: both readings agree",                          10.0, { 6.0, 0.0, 8.0 }, 12.0, { 0.0, 1.0, 0.0 } },
        { "(C) speed param 0: the FLT_EPSILON gate drops the velocity term",        0.0, { 6.0, 0.0, 8.0 }, 12.0, { 0.0, 1.0, 0.0 } },
        { "(D) UpdateDrift shape: v_post / |v_pre|",                               kfPre, { 9.8, 0.0, 20.0 }, 25.0, { 0.0, 1.0, 0.0 } },
        { "(E) sloped ground, |v| != speed param",                                  8.0, { 2.0, 1.0, 5.0 }, 14.0, { 0.0, 0.8, 0.6 } },
    };

    for (const Case& lrCase : kaCases)
    {
        DriftFixture lCar;
        lCar.mDriftFlags.mu8DriftFlags = VehiclePhysics::DriftFlags::KU_DRIFT_FLAG_DO_ALL;
        lCar.mvSpare_MaintainedSpeed_NeutralControlTime_DriftScale.y = f32(lrCase.mfMaintained);
        lCar.mAboveGroundTestResult.mbValid = true;
        lCar.mAboveGroundTestResult.mIntersectionNormal = Vector3{ f32(lrCase.maNormal[0]), f32(lrCase.maNormal[1]), f32(lrCase.maNormal[2]), 0.0f };
        lCar.mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor = { f32(kfAlongZ), f32(kfAlongVel), 0.0f, 1.0f };
        lCar.mpAttribs = &lAttribs;
        lCar.mLinearVelocity = Vector3{ f32(lrCase.maVelocity[0]), f32(lrCase.maVelocity[1]), f32(lrCase.maVelocity[2]), 0.0f };

        BrnPlayerDriverControls lControls;
        lControls.mfGas = 1.0f;
        const f32 lfSpeed = f32(lrCase.mfSpeedParam);
        gbDriftProbe = (&lrCase == &kaCases[0]);
        gLog.clear();
        lCar.MaintainDriftSpeed(&lControls, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f }, VecFloat{ lfSpeed, lfSpeed, lfSpeed, lfSpeed });
        if (!gLog.empty())
            std::printf("    witness: %s", gLog.c_str());

        // The console formula, in double.
        const double lfScale = (lrCase.mfMaintained - double(lfSpeed)) * kfMass;
        const double lfInv   = (std::fabs(double(lfSpeed)) > 1.1920929e-07) ? 1.0 / double(lfSpeed) : 0.0;
        double laDir[3] = { 0.0, 0.0, kfAlongZ };
        for (int i = 0; i < 3; ++i) laDir[i] += double(f32(lrCase.maVelocity[i])) * lfInv * kfAlongVel;
        const double lfDot = laDir[0] * lrCase.maNormal[0] + laDir[1] * lrCase.maNormal[1] + laDir[2] * lrCase.maNormal[2];
        double laExpected[3];
        for (int i = 0; i < 3; ++i) laExpected[i] = (laDir[i] - lrCase.maNormal[i] * lfDot) * lfScale;

        bool lbPass = lCar.maImpulses.size() == 1;
        if (lbPass)
        {
            const Vector3& lrGot = lCar.maImpulses[0];
            const double laGot[3] = { lrGot.x, lrGot.y, lrGot.z };
            for (int i = 0; i < 3; ++i)
                lbPass = lbPass && std::fabs(laGot[i] - laExpected[i]) <= 1.0e-4 * (1.0 + std::fabs(laExpected[i]));
            std::printf("%s\n    got (%.4f, %.4f, %.4f) expected (%.4f, %.4f, %.4f)\n", lrCase.mpcName,
                        laGot[0], laGot[1], laGot[2], laExpected[0], laExpected[1], laExpected[2]);
        }
        Check(lbPass, lrCase.mpcName);
    }

    std::printf("FxVehphysMaintainDriftSpeed: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
