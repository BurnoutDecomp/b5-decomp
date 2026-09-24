// FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): TrafficEntityModule::DriveTowardsTarget @0x8273DFC0, the
// console arithmetic of three legs FX-TRAFFIC4 flagged / left behind.
// The PRODUCTION body, extracted from src/GameSource/World/EntityModules/TrafficEntityModule/
// BrnTrafficEntityModule.cpp by run_fxtraffic5_drive.py with its constants, helpers and the Vehicle /
// ParamTransform accessors it reaches. Its steering / pedal / stuck / return callees are RECORDING doubles
// (they are not what is under test): CalculateDriverGasBrake returns a scripted pedal, and the avoidance
// steering double records the direction it is handed and writes a scripted mfSteering.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   0x8273E248..0x8273E288  |diff|^2 -> vrsqrtefp128 + 2 Newton steps; the DISTANCE is vsel-guarded to
//                           0 (vcmpeqfp128 0x8273E258), the UNIT DIRECTION is not: diff * rsqrt
//                           (0x8273E284 / 0x8273E518) -> a car exactly on its target gets NaN (0 * inf)
//   0x8273E5CC..0x8273E5F8  gas = vminfp128(vmaxfp128(pedal, 0), 1); brake = the same on -pedal (sign
//                           XOR) -- a NaN pedal gives NaN gas AND NaN brake
//   0x8273E6C4..0x8273E700  mfSteering *= rw::math::fpu::Sgn(GetSpeed()) @0x825BC920:
//                           == 0 -> 0.0, >= 0 -> 1.0, else -1.0 (a NaN speed is -1)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
namespace PerfMonCpu
{
    void StartMonitor(s32) {}
    void StopMonitor(s32) {}
}
}

typedef BrnPhysics::Vehicle::BrnTrafficDriverControls Controls;

static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

// ---- the doubles' state ---------------------------------------------------------------------------
static unsigned gAvoidCalls = 0, gPlainCalls = 0, gGiveUps = 0, gReturns = 0;
static f32      gScriptPedal    = 0.0f;   // CalculateDriverGasBrake's return
static f32      gScriptSteering = 0.0f;   // what the avoidance double writes into mfSteering
static f32      gAvoidDist      = -1.0f;  // the distance the avoidance double was handed
static Vector3  gAvoidDir       = {};     // the direction it was handed

void BrnTraffic::Vehicle::StartGiveUpManoeuvre() { ++gGiveUps; }

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    ParamTransform gParamTransform;

    struct DriveFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::maVehicles)          maVehicles;
        decltype(M::maVehicleTransforms) maVehicleTransforms;
        decltype(M::mVehicleSoaData)     mVehicleSoaData;
        decltype(M::miPerfMon_Driving)   miPerfMon_Driving;

        Vehicle*       GetVehicle(u32 luIndex);
        Matrix44Affine GetVehicleTransform(u32 luIndex) const;

        ParamTransform* GetParamTransform(u32) { return &gParamTransform; }
        bool            CheckIfPhysicalVehicleIsStuck(u32) { return false; }
        void            ReturnPhysicalVehicleToTraffic(u32) { ++gReturns; }
        VecFloat        CalculateDriverGasBrake(u32, VecFloat, Vector3)
        {
            return VecFloat{ gScriptPedal, gScriptPedal, gScriptPedal, gScriptPedal };
        }
        void CalculateAndSetSteering(u32, Vector3, Controls*, VecFloat) { ++gPlainCalls; }
        void CalculateAndSetSteeringUsingAvoidance(u32, Vector3& lNewDirection, VecFloat lfDistFromTarget,
                                                   Controls* lpControls, VecFloat& lfOverallRisk)
        {
            ++gAvoidCalls;
            gAvoidDist           = lfDistFromTarget.x;
            gAvoidDir            = lNewDirection;
            lpControls->mfSteering = gScriptSteering;
            lfOverallRisk        = VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
        }

        void DriveTowardsTarget(u32 luVehicle, bool lbAllowReturnToTraffic, Controls* lpControls);
    };
}

// The production bodies under test.
#include "drive.inc"

using namespace BrnTraffic;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
    else
    {
        std::printf("ok: %s\n", lpcName);
    }
}

static bool Near(f32 a, f32 b, f32 tol = 1e-5f) { return std::fabs(a - b) <= tol; }
static const u32 KU_CAR = 7;

alignas(64) static unsigned char gaDrive[sizeof(DriveFixture)];
static DriveFixture& D() { return *reinterpret_cast<DriveFixture*>(gaDrive); }

// A physical standard car at the origin facing +Z, NORMAL physical reason (5), 10 s physical, target
// lfTargetZ ahead, param heading +Z at speed 0, moving at lfSpeed.
static void Fresh(f32 lfTargetZ, f32 lfSpeed, f32 lfPedal, f32 lfSteering)
{
    std::memset(gaDrive, 0, sizeof(gaDrive));
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        D().maVehicleTransforms[luVehicle].SetIdentity();
    }
    std::memset(&gParamTransform, 0, sizeof(gParamTransform));
    Vector3 lDirection = {};
    lDirection.z = 1.0f;
    gParamTransform.mDirAndAccel.SetVector3(lDirection);

    Vehicle& lrCar = D().maVehicles[KU_CAR];
    lrCar.mxFlags          = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    lrCar.muSpecies        = Vehicle::E_SPECIES_STANDARD;
    lrCar.miPhysicalReason = 5;
    lrCar.mfPhysicalTime   = 10.0f;
    lrCar.mfRandomVal      = 0.5f;
    lrCar.mTargetPos       = Vector3{ 0.0f, 0.0f, lfTargetZ, 1.0f };
    lrCar.mSpeed_DistAcrossLane_SwerveAmount_W = Vector4{ lfSpeed, 0.0f, 0.0f, 0.0f };

    gAvoidCalls = gPlainCalls = 0;
    gAvoidDist      = -1.0f;
    gAvoidDir       = Vector3{};
    gScriptPedal    = lfPedal;
    gScriptSteering = lfSteering;
}

static Controls Run(bool lbAllowReturnToTraffic = false)
{
    Controls l;
    std::memset(&l, 0, sizeof(l));
    l.mfGas = 9.0f; l.mfBrake = 9.0f; l.mfHandBrake = 9.0f; l.mfSteering = 9.0f;
    D().DriveTowardsTarget(KU_CAR, lbAllowReturnToTraffic, &l);
    return l;
}

int main()
{
    // ---- (a) the pedal split: vmaxfp / vminfp pairs ------------------------------------------------
    {
        Fresh(25.0f, 10.0f, KF_NAN, 0.25f);
        Controls l = Run();
        Check(std::isnan(l.mfGas) && std::isnan(l.mfBrake),
              "A1 0x8273E5CC..0x8273E5F8: a NaN pedal reaches mfGas AND mfBrake as NaN (vmaxfp/vminfp pass it)");

        Fresh(25.0f, 10.0f, 0.4f, 0.25f);
        l = Run();
        Check(Near(l.mfGas, 0.4f) && l.mfBrake == 0.0f, "A2 pedal 0.4: gas 0.4, brake 0");
        Fresh(25.0f, 10.0f, -0.3f, 0.25f);
        l = Run();
        Check(l.mfGas == 0.0f && Near(l.mfBrake, 0.3f), "A3 pedal -0.3: gas 0, brake 0.3 (-pedal)");
        Fresh(25.0f, 10.0f, 2.5f, 0.25f);
        l = Run();
        Check(l.mfGas == 1.0f && l.mfBrake == 0.0f, "A4 pedal 2.5: gas clamped to 1.0");
        Fresh(25.0f, 10.0f, -7.0f, 0.25f);
        l = Run();
        Check(l.mfGas == 0.0f && l.mfBrake == 1.0f, "A5 pedal -7: brake clamped to 1.0");
    }

    // ---- (b) the unit direction to the target: diff * rsqrt, unguarded ---------------------------
    {
        Fresh(25.0f, 10.0f, 0.0f, 0.25f);
        Run();
        Check(gAvoidCalls == 1 && Near(gAvoidDist, 25.0f) && Near(gAvoidDir.z, 1.0f) && gAvoidDir.x == 0.0f,
              "B1 target 25 m ahead: avoidance gets |diff| = 25 and the unit direction (0, 0, 1)");

        Fresh(0.0f, 10.0f, 0.0f, 0.25f);
        Run(true);
        Check(gAvoidCalls == 1 && gAvoidDist == 0.0f,
              "B2 0x8273E288 vsel: a car ON its target gets distance 0 (the guarded result)");
        Check(std::isnan(gAvoidDir.x) && std::isnan(gAvoidDir.y) && std::isnan(gAvoidDir.z),
              "B3 0x8273E284 / 0x8273E518: ...and the UNGUARDED unit direction 0 * rsqrt(0) = NaN");
        Check(gReturns == 0, "B4 hand-back allowed, on the target, 10 s physical: the NaN direction fails the > 0.98 dot test (no hand-back)");
    }

    // ---- (c) the reversing flip: rw::math::fpu::Sgn(GetSpeed()) --------------------------------
    {
        Fresh(25.0f, 5.0f, 0.0f, 0.25f);
        Controls l = Run();
        Check(Near(l.mfSteering, 0.25f), "C1 moving forward: steering kept (Sgn 1)");
        Fresh(25.0f, -3.0f, 0.0f, 0.25f);
        l = Run();
        Check(Near(l.mfSteering, -0.25f), "C2 reversing: steering flipped (Sgn -1)");
        Fresh(25.0f, 0.0f, 0.0f, 0.25f);
        l = Run();
        Check(l.mfSteering == 0.0f, "C3 stopped: steering * Sgn(0) = 0");
        Fresh(25.0f, KF_NAN, 0.0f, 0.25f);
        l = Run();
        Check(Near(l.mfSteering, -0.25f),
              "C4 0x825BC920: Sgn(NaN) = -1 (vcmpeqfp. and vcmpgefp. both fail) -> steering flipped, not zeroed");
    }

    Check(gAsserts == 0 && gGiveUps == 0 && gPlainCalls == 0, "Z1 no assert, give-up or plain steering on any path");
    std::printf("FxTraffic5Drive: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
