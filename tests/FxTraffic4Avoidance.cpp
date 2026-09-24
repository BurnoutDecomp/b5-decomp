// FX-TRAFFIC4 item 3 (crash parity wave 5, 2026-09-24): the avoidance steering. The PRODUCTION bodies
// of src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp, extracted by
// run_fxtraffic4_avoidance.py with their constants and every accessor they reach:
//   PrecalculateAvoidanceFeelerData @0x82708E78, Avoidance_CalculateDistancePosVelToOrigin @0x82708DD0,
//   Avoidance_CalculatePassingScore @0x827199B8, Avoidance_CalculateFeelers (inlined),
//   Avoidance_GetBestVehicleDirection @0x8272C248, CalculateAndSetSteeringUsingAvoidance @0x8273D258,
//   the six GetAvoid* lane accessors and CalculateAndSetSteering @0x82718E48 (AvoidFixture), and
//   DriveTowardsTarget @0x8273DFC0 (DriveFixture), whose steering / pedal / stuck / return callees are
//   RECORDING doubles here -- they are not what is under test.
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   maFeelerCosSin[i] = (cos, sin, cos, cos) of pi/2 (flt_820BA254) minus 15 deg (flt_82F2FDFC) once
//     per pair (fsubs 0x82708EC4 / 0x82708F00): 75 then 60 degrees
//   feelers [0] Dir, [1] Dir*sin75 - Right*cos75, [2] Dir*sin60 - Right*cos60, [3] / [4] the + side;
//     Dir = At, negated when 0 > speed; flown at |speed|
//   passing score: |dy| > 3 (lane 3), relPos^2 == 0, relVel^2 == 0 or t = relPos^2/relVel^2 > 4
//     (lane 0) -> 0; else (4 - t) * 10 (lane 1) + 10 - min(|XZ miss|, 10) (lane 2); v5/v6 unread
//   total = max over every packet lane + (1 - clamp01(dot(target, feeler))) * 10 (+0x72790 lane 0);
//     best = first strict minimum (seeded FLT_MAX); risk = clamp01(sum * 0.2 / 50 (lane 1))
//   CalculateAndSetSteeringUsingAvoidance: risk >= 0.2 && dist >= 1 && mbDEBUGEnableAvoidance ->
//     dot(avoid, target) < 0.94 ? target + (avoid - target) * mfSimTimeStep : avoid; then
//     CalculateAndSetSteering with the risk as its scale (record * 1.3 from 0.6)
//   DriveTowardsTarget: CalculateAndSetSteeringUsingAvoidance(luVehicle, unit diff, |diff|, controls,
//     risk) unless extreme swerving under 3 s; then !IsExtremeSwerving && risk >= 0.7 (all lanes,
//     NaN fails) -> mfHandBrake = 0.5
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include <cfloat>
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
namespace Log { DebugPrint* gpDebugPrint = nullptr; }             // the witness stream stays off
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
namespace PerfMonCpu
{
    void StartMonitor(s32) {}
    void StopMonitor(s32) {}
}
}

typedef BrnPhysics::Vehicle::BrnTrafficDriverControls Controls;

// ---- the DriveFixture doubles' state --------------------------------------------------------------
static unsigned gAvoidCalls = 0, gPlainCalls = 0, gGiveUps = 0, gReturns = 0;
static f32      gScriptRisk = 0.0f;       // what the avoidance double reports through lfOverallRisk
static f32      gAvoidDist  = -1.0f;      // its lfDistFromTarget
static Vector3  gAvoidDir   = {};         // its lNewDirection on entry
static f32      gPlainScale = -1.0f;      // the plain CalculateAndSetSteering double's lvfScale

// The one Vehicle member DriveTowardsTarget can reach that the test never takes (a slammed car's 5%).
void BrnTraffic::Vehicle::StartGiveUpManoeuvre() { ++gGiveUps; }

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct AvoidFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::maVehicles)                                 maVehicles;
        decltype(M::maVehicleTransforms)                        maVehicleTransforms;
        decltype(M::mCachedCollidableList)                      mCachedCollidableList;
        decltype(M::maFeelerCosSin)                             maFeelerCosSin;
        decltype(M::kfVehicle_AvoidancePassingFactor_Constants) kfVehicle_AvoidancePassingFactor_Constants;
        decltype(M::kfVehicle_Avoidance_Constants)              kfVehicle_Avoidance_Constants;
        decltype(M::KF_MAX_FLOAT)                               KF_MAX_FLOAT;
        decltype(M::KF_VEHICLE_MAX_STEERING_DELTA)              KF_VEHICLE_MAX_STEERING_DELTA;
        decltype(M::KF_VEHICLE_SIN_MAX_STEERING_ANGLE)          KF_VEHICLE_SIN_MAX_STEERING_ANGLE;
        decltype(M::mfSimTimeStep)                              mfSimTimeStep;
        decltype(M::mbDEBUGEnableAvoidance)                     mbDEBUGEnableAvoidance;

        Vehicle*       GetVehicle(u32 luIndex);
        Matrix44Affine GetVehicleTransform(u32 luIndex) const;

        VecFloat GetAvoidPassImpactTimeMax() const;
        VecFloat GetAvoidPassImpactTimeScoreFactor() const;
        VecFloat GetAvoidPassMaxDistance() const;
        VecFloat GetAvoidPassHeightSkip() const;
        VecFloat GetAvoidOffcourseScoreFactor() const;
        VecFloat GetAvoidMaxOverallRisk() const;

        void     PrecalculateAvoidanceFeelerData();
        VecFloat Avoidance_CalculateDistancePosVelToOrigin(Vector2 lStart, Vector2 lVel);
        VecFloat Avoidance_CalculatePassingScore(Vector3 lPositionA, Vector3 lVelocityA, Vector3 lPositionB,
                                                 Vector3 lVelocityB, VecFloat lfObjectBHalfLength,
                                                 VecFloat lfObjectBHalfWidth);
        void     Avoidance_CalculateFeelers(Vector3 lDirection, Vector3 lRight, Vector3* laFeelers);
        void     Avoidance_GetBestVehicleDirection(u32 luVehicle, Vector3& lNewDirection, VecFloat& lfOverallRisk);
        void     CalculateAndSetSteeringUsingAvoidance(u32 luVehicle, Vector3& lNewDirection, VecFloat lfDistFromTarget,
                                                       Controls* lpOutControls, VecFloat& lfOverallRisk);
        void     CalculateAndSetSteering(u32 luVehicle, Vector3 lTargetDirection, Controls* lpControls,
                                         VecFloat lvfScale);
    };

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
        VecFloat        CalculateDriverGasBrake(u32, VecFloat, Vector3) { return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f }; }
        void CalculateAndSetSteering(u32, Vector3, Controls*, VecFloat lvfScale)
        {
            ++gPlainCalls;
            gPlainScale = lvfScale.x;
        }
        void CalculateAndSetSteeringUsingAvoidance(u32, Vector3& lNewDirection, VecFloat lfDistFromTarget,
                                                   Controls*, VecFloat& lfOverallRisk)
        {
            ++gAvoidCalls;
            gAvoidDist    = lfDistFromTarget.x;
            gAvoidDir     = lNewDirection;
            lfOverallRisk = VecFloat{ gScriptRisk, gScriptRisk, gScriptRisk, gScriptRisk };
        }

        void DriveTowardsTarget(u32 luVehicle, bool lbAllowReturnToTraffic, Controls* lpControls);
    };
}

// The production bodies under test.
#include "avoidance.inc"

using namespace BrnTraffic;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool Near(f32 a, f32 b, f32 tol = 1e-4f) { return std::fabs(a - b) <= tol; }
static bool NearV(const Vector3& a, const Vector3& b, f32 tol = 1e-4f)
{
    return Near(a.x, b.x, tol) && Near(a.y, b.y, tol) && Near(a.z, b.z, tol);
}
static Vector3 V3(f32 x, f32 y, f32 z) { return Vector3{ x, y, z, 0.0f }; }
static Vector2 V2(f32 x, f32 y) { return Vector2{ x, y, 0.0f, 0.0f }; }
static bool Splat(const VecFloat& v) { return Bits(v.x) == Bits(v.y) && Bits(v.x) == Bits(v.z) && Bits(v.x) == Bits(v.w); }

alignas(64) static unsigned char gaAvoid[sizeof(AvoidFixture)];
alignas(64) static unsigned char gaDrive[sizeof(DriveFixture)];
static AvoidFixture& A() { return *reinterpret_cast<AvoidFixture*>(gaAvoid); }
static DriveFixture& D() { return *reinterpret_cast<DriveFixture*>(gaDrive); }

static const u32 KU_CAR = 7;
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();
static const f32 KF_DT  = 1.0f / 30.0f;

// The image's own values, by address (never the reconstruction's literals).
static const f32 KF_IMG_HALF_PI = FromBits(0x3FC90FDB);   // flt_820BA254
static const f32 KF_IMG_STEP    = FromBits(0x3E860A92);   // flt_82F2FDFC

// The Construct seeds of the two tuning members (0x82740690 / 0x82740694) and the rest of a module
// the avoidance reads. The car sits at the origin facing +Z (At), Right +X, at the given speed.
static void FreshAvoid(f32 lfSpeed)
{
    std::memset(gaAvoid, 0, sizeof(gaAvoid));
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        A().maVehicleTransforms[luVehicle].SetIdentity();
    }
    A().kfVehicle_AvoidancePassingFactor_Constants = Vector4{ 4.0f, 10.0f, 10.0f, 3.0f };
    A().kfVehicle_Avoidance_Constants              = Vector4{ 10.0f, 50.0f, 0.0f, 0.0f };
    A().KF_MAX_FLOAT                      = VecFloat{ FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
    A().KF_VEHICLE_MAX_STEERING_DELTA     = VecFloat{ 0.025f, 0.025f, 0.025f, 0.025f };
    const f32 lfSinMax = std::sin(25.0f * 0.01745329238474369f);
    A().KF_VEHICLE_SIN_MAX_STEERING_ANGLE = VecFloat{ lfSinMax, lfSinMax, lfSinMax, lfSinMax };
    A().mfSimTimeStep                     = KF_DT;
    A().mbDEBUGEnableAvoidance            = true;
    A().mCachedCollidableList.Clear();
    A().PrecalculateAvoidanceFeelerData();

    Vehicle& lrCar = A().maVehicles[KU_CAR];
    lrCar.mxFlags   = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    lrCar.muSpecies = Vehicle::E_SPECIES_STANDARD;
    lrCar.mSpeed_DistAcrossLane_SwerveAmount_W = Vector4{ lfSpeed, 0.0f, 0.0f, 0.0f };
}

static void SetLaneOf(Vector4& lrMember, u32 luLane, f32 lf)
{
    if (luLane == 0) { lrMember.x = lf; } else if (luLane == 1) { lrMember.y = lf; }
    else if (luLane == 2) { lrMember.z = lf; } else { lrMember.w = lf; }
}

// One packet: the given car in lane luLane, the other lanes parked the way UpdateCollidableVehicles
// pads a partial packet (position FLT_MAX, velocity 0).
static CollidableVehicleInfo4 Packet(u32 luLane, Vector3 lPos, Vector3 lVel, f32 lfHalfLength = 2.0f, f32 lfHalfWidth = 1.0f)
{
    CollidableVehicleInfo4 l;
    std::memset(&l, 0, sizeof(l));
    for (u32 luPad = 0; luPad < 4; ++luPad)
    {
        SetLaneOf(l.mPosition_X, luPad, FLT_MAX);
        SetLaneOf(l.mPosition_Y, luPad, FLT_MAX);
        SetLaneOf(l.mPosition_Z, luPad, FLT_MAX);
    }
    SetLaneOf(l.mPosition_X, luLane, lPos.x);
    SetLaneOf(l.mPosition_Y, luLane, lPos.y);
    SetLaneOf(l.mPosition_Z, luLane, lPos.z);
    SetLaneOf(l.mLinearVelocity_X, luLane, lVel.x);
    SetLaneOf(l.mLinearVelocity_Y, luLane, lVel.y);
    SetLaneOf(l.mLinearVelocity_Z, luLane, lVel.z);
    SetLaneOf(l.mHalfLengths, luLane, lfHalfLength);
    SetLaneOf(l.mHalfWidths, luLane, lfHalfWidth);
    return l;
}

static VecFloat gLastRisk;   // the whole register of the last Best() risk

static void Best(Vector3 lTarget, Vector3& lrOutDirection, f32& lrfOutRisk)
{
    Vector3  lDirection = lTarget;
    VecFloat lRisk      = VecFloat{ -7.0f, -7.0f, -7.0f, -7.0f };
    A().Avoidance_GetBestVehicleDirection(KU_CAR, lDirection, lRisk);
    lrOutDirection = lDirection;
    lrfOutRisk     = lRisk.x;
    gLastRisk      = lRisk;
}

static Controls Sentinel()
{
    Controls l;
    std::memset(&l, 0, sizeof(l));
    l.mfGas = 9.0f; l.mfBrake = 9.0f; l.mfHandBrake = 9.0f; l.mfSteering = 9.0f;
    return l;
}

// DriveTowardsTarget's car at the origin facing +Z, target 25 m ahead, param heading +Z at speed 0.
static void FreshDrive(s8 liPhysicalReason, f32 lfPhysicalTime, f32 lfRisk)
{
    std::memset(gaDrive, 0, sizeof(gaDrive));
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        D().maVehicleTransforms[luVehicle].SetIdentity();
    }
    std::memset(&gParamTransform, 0, sizeof(gParamTransform));
    gParamTransform.mDirAndAccel.SetVector3(V3(0.0f, 0.0f, 1.0f));

    Vehicle& lrCar = D().maVehicles[KU_CAR];
    lrCar.mxFlags          = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    lrCar.muSpecies        = Vehicle::E_SPECIES_STANDARD;
    lrCar.miPhysicalReason = liPhysicalReason;
    lrCar.mfPhysicalTime   = lfPhysicalTime;
    lrCar.mfRandomVal      = 0.5f;
    lrCar.mTargetPos       = Vector3{ 0.0f, 0.0f, 25.0f, 1.0f };
    lrCar.mSpeed_DistAcrossLane_SwerveAmount_W = Vector4{ 10.0f, 0.0f, 0.0f, 0.0f };

    gAvoidCalls = gPlainCalls = 0;
    gAvoidDist  = -1.0f;
    gAvoidDir   = Vector3{};
    gPlainScale = -1.0f;
    gScriptRisk = lfRisk;
}

int main()
{
    // ==== the constants are the image's bit patterns ============================================
    Check(Bits(KF_AVOIDANCE_FEELERS_START_ANGLE) == 0x3FC90FDB && Bits(KF_TRAFFIC_AVOIDANCE_FEELERS_ANGLE) == 0x3E860A92
          && Bits(KF_AVOIDANCE_FEELER_MEAN) == 0x3E4CCCCD && Bits(KF_AVOIDANCE_MIN_RISK) == 0x3E4CCCCD
          && Bits(KF_AVOIDANCE_MIN_TARGET_DIST) == 0x3F800000 && Bits(KF_AVOIDANCE_SNAP_DOT) == 0x3F70A3D7
          && Bits(KF_AVOIDANCE_HANDBRAKE_RISK) == 0x3F333333 && Bits(KF_AVOIDANCE_HANDBRAKE) == 0x3F000000,
          "K1 flt_820BA254 / flt_82F2FDFC / unk_8300CF40 / unk_8300CBE0 lanes 0..2 / unk_8300CEE0 / flt_820BA62C bit patterns");

    // ==== PrecalculateAvoidanceFeelerData ========================================================
    FreshAvoid(10.0f);
    {
        const f32 lfA0 = KF_IMG_HALF_PI - KF_IMG_STEP;   // fsubs 0x82708EC4
        const f32 lfA1 = lfA0 - KF_IMG_STEP;             // fsubs 0x82708F00
        const Vector2& l0 = A().maFeelerCosSin[0];
        const Vector2& l1 = A().maFeelerCosSin[1];
        Check(Near(l0.x, std::cos(lfA0), 1e-6f) && Near(l0.y, std::sin(lfA0), 1e-6f) && l0.z == l0.x && l0.w == l0.x,
              "F1 maFeelerCosSin[0] = (cos, sin, cos, cos) of 75 deg (the vperm128 control unk_82CDA350)");
        Check(Near(l1.x, 0.5f, 1e-6f) && Near(l1.y, std::sin(lfA1), 1e-6f) && l1.z == l1.x && l1.w == l1.x,
              "F2 maFeelerCosSin[1] = (cos, sin, cos, cos) of 60 deg (the angle steps down twice)");
    }

    // ==== Avoidance_CalculateFeelers ==============================================================
    {
        Vector3 la[KI_TRAFFIC_AVOIDANCE_FEELERS];
        A().Avoidance_CalculateFeelers(V3(0.0f, 0.0f, 1.0f), V3(1.0f, 0.0f, 0.0f), la);
        const f32 c75 = A().maFeelerCosSin[0].x, s75 = A().maFeelerCosSin[0].y;
        const f32 c60 = A().maFeelerCosSin[1].x, s60 = A().maFeelerCosSin[1].y;
        Check(NearV(la[0], V3(0, 0, 1), 1e-6f) && NearV(la[1], V3(-c75, 0, s75), 1e-6f) && NearV(la[2], V3(-c60, 0, s60), 1e-6f)
              && NearV(la[3], V3(c75, 0, s75), 1e-6f) && NearV(la[4], V3(c60, 0, s60), 1e-6f),
              "F3 feelers: [0] Dir, [1]/[2] Dir*sin - Right*cos (75, 60), [3]/[4] Dir*sin + Right*cos (var_1E0..var_1A0)");
    }

    // ==== Avoidance_CalculateDistancePosVelToOrigin ===============================================
    {
        const VecFloat l1 = A().Avoidance_CalculateDistancePosVelToOrigin(V2(3.0f, 4.0f), V2(1.0f, 0.0f));
        const VecFloat l2 = A().Avoidance_CalculateDistancePosVelToOrigin(V2(3.0f, 4.0f), V2(0.0f, 0.0f));
        const VecFloat l3 = A().Avoidance_CalculateDistancePosVelToOrigin(V2(0.0f, 0.0f), V2(1.0f, 1.0f));
        const VecFloat l4 = A().Avoidance_CalculateDistancePosVelToOrigin(V2(-2.0f, 6.0f), V2(0.0f, -3.0f));
        Check(Near(l1.x, 4.0f, 1e-5f) && Splat(l1),
              "M1 the line through (3,4) along (1,0) passes 4 from the origin: sqrt(cross^2 / |vel|^2), a splat");
        Check(Near(l2.x, 5.0f, 1e-5f), "M2 a zero velocity falls back to |start| = 5 (vcmpeqfp / vsel)");
        Check(l3.x == 0.0f, "M3 a zero result is 0, not the NaN of 0 * rsqrt(0) (the second vsel)");
        Check(Near(l4.x, 2.0f, 1e-5f), "M4 unsigned: the line through (-2,6) along (0,-3) passes 2 away, not -2");
    }

    // ==== Avoidance_CalculatePassingScore (A at the origin moving +Z at 10 m/s) ====================
    {
        const Vector3 lPosA = V3(0, 0, 0), lVelA = V3(0, 0, 10);
        const VecFloat lHalf = VecFloat{ 2.0f, 2.0f, 2.0f, 2.0f };
        const VecFloat lHead = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 0, 15), V3(0, 0, -10), lHalf, lHalf);
        Check(Near(lHead.x, 44.375f) && Splat(lHead),
              "P1 head-on, 15 m, closing 20 m/s: t = 225/400, (4 - t) * 10 + (10 - 0) = 44.375");
        const f32 lfOffset = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(3, 0, 15), V3(0, 0, -10), lHalf, lHalf).x;
        Check(Near(lfOffset, 41.15f), "P2 3 m to the side: t = 234/400, miss 3 -> 34.15 + 7 = 41.15");
        const f32 lfWide = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(12, 0, 15), V3(0, 0, -10), lHalf, lHalf).x;
        Check(Near(lfWide, 30.775f), "P3 12 m to the side: the miss is capped at 10 (vminfp), 30.775 + 0");
        const f32 lfHigh = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 3.5f, 15), V3(0, 0, -10), lHalf, lHalf).x;
        const f32 lfEdge = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 3.0f, 15), V3(0, 0, -10), lHalf, lHalf).x;
        Check(lfHigh == 0.0f && Near(lfEdge, (4.0f - 234.0f / 400.0f) * 10.0f + 10.0f),
              "P4 3.5 m above scores 0 (|dy| > 3, lane 3); exactly 3 m still scores (vcmpgtfp: > only), in the XZ plane");
        const f32 lfSame  = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 0, 0), V3(0, 0, -10), lHalf, lHalf).x;
        const f32 lfWith  = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 0, 15), V3(0, 0, 10), lHalf, lHalf).x;
        const f32 lfFar   = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 0, 30), V3(0, 0, 5), lHalf, lHalf).x;
        const f32 lfEdgeT = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(0, 0, 40), V3(0, 0, -10), lHalf, lHalf).x;
        Check(lfSame == 0.0f && lfWith == 0.0f && lfFar == 0.0f && Near(lfEdgeT, 10.0f),
              "P5 coincident, co-moving and t = 36 > 4 score 0; t = 1600/400 = 4 exactly still scores (0 + 10)");
        const f32 lfOtherHalf = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(3, 0, 15), V3(0, 0, -10),
                                                                     VecFloat{ 9.0f, 9.0f, 9.0f, 9.0f }, VecFloat{ 0.1f, 0.1f, 0.1f, 0.1f }).x;
        Check(lfOtherHalf == lfOffset, "P6 the half extents (v5 / v6) are never read");
        const f32 lfNaN = A().Avoidance_CalculatePassingScore(lPosA, lVelA, V3(KF_NAN, 0, 15), V3(0, 0, -10), lHalf, lHalf).x;
        Check(lfNaN != lfNaN, "P7 a NaN position fails every gate (all-lanes compares) and scores NaN, not 0");
    }

    // ==== Avoidance_GetBestVehicleDirection ========================================================
    const f32 c75 = A().maFeelerCosSin[0].x, s75 = A().maFeelerCosSin[0].y;
    const f32 c60 = A().maFeelerCosSin[1].x, s60 = A().maFeelerCosSin[1].y;
    const f32 kfQuietRisk = ((1.0f - s75) * 10.0f * 2.0f + (1.0f - s60) * 10.0f * 2.0f) * 0.2f / 50.0f;
    {
        FreshAvoid(10.0f);
        Vector3 lDir; f32 lfRisk;
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(NearV(lDir, V3(0, 0, 1), 1e-6f) && Near(lfRisk, kfQuietRisk, 1e-5f),
              "B1 nothing cached, target ahead: straight on, risk = mean off-course (0, 2 x 0.34, 2 x 1.34) * 0.2 / 50 = 0.0134");

        Best(V3(0.5f, 0, 0.8660254f), lDir, lfRisk);
        Check(NearV(lDir, V3(c60, 0, s60), 1e-6f), "B2 target 30 deg to the + side: feeler [4] (off-course 0)");
    }
    f32 lfHeadOnRisk = 0.0f;
    {
        // An oncoming car 20 m dead ahead at 10 m/s. Totals: [0] 40, [1]/[3] 37.557, [2]/[4] 35.445.
        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, 20), V3(0, 0, -10)));
        Vector3 lDir; f32 lfRisk;
        Best(V3(0, 0, 1), lDir, lfRisk);
        lfHeadOnRisk = lfRisk;
        Check(NearV(lDir, V3(-c60, 0, s60), 1e-6f),
              "B3 oncoming car ahead: the 30 deg feelers score lowest and tie EXACTLY; the first ([2], the - side) wins (strict >)");
        Check(Near(lfRisk, 0.744018f, 2e-4f) && Splat(gLastRisk),
              "B4 ... risk = (40 + 2 x 37.557 + 2 x 35.445) * 0.2 / 50 = 0.744, written as a splat");

        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(2, V3(0, 0, 20), V3(0, 0, -10)));
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(lfRisk == lfHeadOnRisk && NearV(lDir, V3(-c60, 0, s60), 1e-6f),
              "B5 the same car in lane 2 of a padded packet: the same answer (every lane is read, pads score 0)");

        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, 20), V3(0, 0, -10)));
        A().mCachedCollidableList.Append(Packet(3, V3(0, 0, 20), V3(0, 0, -10)));
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(lfRisk == lfHeadOnRisk, "B6 two identical cars: each feeler keeps its WORST pass (vmaxfp), not the sum");

        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 3.5f, 20), V3(0, 0, -10)));
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(Near(lfRisk, kfQuietRisk, 1e-5f) && NearV(lDir, V3(0, 0, 1), 1e-6f),
              "B7 the same car 3.5 m above is skipped: back to straight on at the quiet risk");

        FreshAvoid(10.0f);
        A().kfVehicle_Avoidance_Constants.y = 5.0f;
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, 20), V3(0, 0, -10)));
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(lfRisk == 1.0f, "B8 the risk divides by lane 1 of +0x72790 (GetAvoidMaxOverallRisk) and clamps at 1");

        FreshAvoid(10.0f);
        A().kfVehicle_Avoidance_Constants.x = 0.0f;
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(lfRisk == 0.0f, "B9 the off-course factor is lane 0 of +0x72790 (GetAvoidOffcourseScoreFactor)");
    }
    {
        // Reversing: At +Z, speed -10, target behind. The feelers turn round with the direction.
        FreshAvoid(-10.0f);
        Vector3 lDir; f32 lfRisk;
        Best(V3(0, 0, -1), lDir, lfRisk);
        Check(NearV(lDir, V3(0, 0, -1), 1e-6f) && Near(lfRisk, kfQuietRisk, 1e-5f),
              "B10 reversing (0 > speed): Dir = -At, so straight back is feeler [0] at the quiet risk");

        // Flown at |speed|: an oncoming car behind a reversing car scores like B3 mirrored.
        FreshAvoid(-10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, -20), V3(0, 0, 10)));
        Best(V3(0, 0, -1), lDir, lfRisk);
        Check(Near(lfRisk, lfHeadOnRisk, 1e-5f), "B11 ... and the feelers fly at |speed|: the mirrored head-on risk");
    }
    {
        // Target 90 deg to the + side: the - feelers' dots are negative and clamp to 0 (vmaxfp 0).
        FreshAvoid(10.0f);
        Vector3 lDir; f32 lfRisk;
        Best(V3(1, 0, 0), lDir, lfRisk);
        const f32 lfSideRisk = (10.0f + 10.0f + 10.0f + (1.0f - c75) * 10.0f + (1.0f - c60) * 10.0f) * 0.2f / 50.0f;
        Check(NearV(lDir, V3(c60, 0, s60), 1e-6f) && Near(lfRisk, lfSideRisk, 1e-5f),
              "B13 target 90 deg right: dots 0, -0.26, -0.5 clamp to 0 (off-course 10 each), best [4], risk 0.1697 not 0.2");

        // A stopped car: 0 > 0 is false (vcmpgtfp128.), so Dir stays At.
        FreshAvoid(0.0f);
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(NearV(lDir, V3(0, 0, 1), 1e-6f) && Near(lfRisk, kfQuietRisk, 1e-5f),
              "B14 speed exactly 0: Dir stays At (strict 0 > speed), straight on at the quiet risk");
    }
    {
        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(KF_NAN, 0, 20), V3(0, 0, -10)));
        Vector3 lDir; f32 lfRisk;
        Best(V3(0, 0, 1), lDir, lfRisk);
        Check(lfRisk != lfRisk && NearV(lDir, V3(0, 0, 1), 1e-6f),
              "B12 a NaN cached car: every total is NaN (vmaxfp keeps it), no feeler beats FLT_MAX -> [0], the risk is NaN");
    }

    // ==== CalculateAndSetSteeringUsingAvoidance =====================================================
    {
        // BLEND: the head-on car, target ahead. Best [2] is 30 deg off (dot 0.866 < 0.94).
        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, 20), V3(0, 0, -10)));
        Vector3  lDir  = V3(0, 0, 1);
        VecFloat lRisk = VecFloat{};
        Controls l     = Sentinel();
        A().CalculateAndSetSteeringUsingAvoidance(KU_CAR, lDir, VecFloat{ 25, 25, 25, 25 }, &l, lRisk);
        const Vector3 lBlend = V3(0.0f + (-c60 - 0.0f) * KF_DT, 0.0f, 1.0f + (s60 - 1.0f) * KF_DT);
        Check(NearV(lDir, lBlend, 1e-6f) && lRisk.x == lfHeadOnRisk,
              "S1 risk 0.744, 25 m out, dot 0.866 < 0.94: target + (avoid - target) * mfSimTimeStep, written back");
        // CalculateAndSetSteering on that direction with the risk as its scale: 0.744 >= 0.6 -> x 1.3.
        const Vector3 lCross = rw::math::vpu::Cross(lBlend, V3(0, 0, 1));
        const f32 lfSin  = std::sqrt(rw::math::vpu::Dot(lCross, lCross));
        const f32 lfSign = (lCross.y > 0.0f) ? 1.0f : ((lCross.y < 0.0f) ? -1.0f : 0.0f);
        Check(Near(l.mfSteering, lfSin * lfSign * 1.3f, 1e-6f) && l.mfGas == 9.0f && l.mfHandBrake == 9.0f,
              "S2 ... then CalculateAndSetSteering(direction, risk): the record is the delta x 1.3 (risk >= 0.6)");

        // SNAP: target 15 deg to the + side, same car. Best [4] is 15 deg from it (dot 0.966 >= 0.94).
        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, 20), V3(0, 0, -10)));
        lDir = V3(c75, 0, s75);
        A().CalculateAndSetSteeringUsingAvoidance(KU_CAR, lDir, VecFloat{ 25, 25, 25, 25 }, &l, lRisk);
        Check(NearV(lDir, V3(c60, 0, s60), 1e-6f) && lRisk.x >= 0.7f,
              "S3 target 15 deg right, best [4] 15 deg from it (dot >= 0.94): the direction SNAPS to the feeler");

        // The gates: under 1 m from the target, avoidance off, and a quiet road.
        FreshAvoid(10.0f);
        A().mCachedCollidableList.Append(Packet(0, V3(0, 0, 20), V3(0, 0, -10)));
        lDir = V3(0, 0, 1);
        A().CalculateAndSetSteeringUsingAvoidance(KU_CAR, lDir, VecFloat{ 0.99f, 0.99f, 0.99f, 0.99f }, &l, lRisk);
        const bool lbNear = NearV(lDir, V3(0, 0, 1), 0.0f) && lRisk.x == lfHeadOnRisk;
        lDir = V3(0, 0, 1);
        A().CalculateAndSetSteeringUsingAvoidance(KU_CAR, lDir, VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f }, &l, lRisk);
        const bool lbAtOne = !NearV(lDir, V3(0, 0, 1), 1e-7f);
        Check(lbNear && lbAtOne, "S4 0.99 m from the target keeps the target direction (lane 1: >= 1.0, so 1.0 itself avoids)");

        A().mbDEBUGEnableAvoidance = false;
        lDir = V3(0, 0, 1);
        A().CalculateAndSetSteeringUsingAvoidance(KU_CAR, lDir, VecFloat{ 25, 25, 25, 25 }, &l, lRisk);
        Check(NearV(lDir, V3(0, 0, 1), 0.0f) && lRisk.x == lfHeadOnRisk,
              "S5 mbDEBUGEnableAvoidance false (+0x72869): the target direction, but the risk is still reported");

        FreshAvoid(10.0f);
        const f32 lfSin10 = std::sin(10.0f * 0.01745329238474369f), lfCos10 = std::cos(10.0f * 0.01745329238474369f);
        lDir = V3(lfSin10, 0, lfCos10);
        l = Sentinel();
        A().CalculateAndSetSteeringUsingAvoidance(KU_CAR, lDir, VecFloat{ 25, 25, 25, 25 }, &l, lRisk);
        const Vector3 lCross10 = rw::math::vpu::Cross(V3(lfSin10, 0, lfCos10), V3(0, 0, 1));
        const f32 lfSign10 = (lCross10.y > 0.0f) ? 1.0f : -1.0f;
        Check(lRisk.x < 0.2f && NearV(lDir, V3(lfSin10, 0, lfCos10), 0.0f)
              && Near(l.mfSteering, std::sqrt(rw::math::vpu::Dot(lCross10, lCross10)) * lfSign10, 1e-6f),
              "S6 a quiet road (risk < 0.2, lane 0): the target direction and an unscaled record");
    }

    // ==== DriveTowardsTarget: the call and the handbrake leg ========================================
    {
        FreshDrive(5, 10.0f, 0.7f);
        Controls l = Sentinel();
        D().DriveTowardsTarget(KU_CAR, false, &l);
        Check(gAvoidCalls == 1 && gPlainCalls == 0 && Near(gAvoidDist, 25.0f, 1e-5f) && NearV(gAvoidDir, V3(0, 0, 1), 1e-6f),
              "D1 a driving car steers through CalculateAndSetSteeringUsingAvoidance(unit diff, |diff| = 25) (0x8273E56C)");
        Check(l.mfHandBrake == 0.5f, "D2 risk 0.7 (>= unk_8300CEE0, not >): mfHandBrake 0.5 (flt_820BA62C)");

        FreshDrive(5, 10.0f, 0.69999f);
        l = Sentinel();
        D().DriveTowardsTarget(KU_CAR, false, &l);
        Check(l.mfHandBrake == 9.0f, "D3 risk 0.69999: the handbrake is not written");

        FreshDrive(5, 10.0f, KF_NAN);
        l = Sentinel();
        D().DriveTowardsTarget(KU_CAR, false, &l);
        Check(l.mfHandBrake == 9.0f, "D4 a NaN risk fails the all-lanes vcmpgefp.: no handbrake");

        FreshDrive(4, 1.0f, 0.9f);
        l = Sentinel();
        D().DriveTowardsTarget(KU_CAR, false, &l);
        Check(gAvoidCalls == 0 && gPlainCalls == 1 && gPlainScale == 0.0f && l.mfHandBrake == 9.0f,
              "D5 extreme swerving for 1 s (< 3): plain CalculateAndSetSteering at scale 0, no avoidance, no handbrake");

        FreshDrive(4, 5.0f, 0.9f);
        l = Sentinel();
        D().DriveTowardsTarget(KU_CAR, false, &l);
        Check(gAvoidCalls == 1 && gPlainCalls == 0 && l.mfHandBrake == 9.0f,
              "D6 extreme swerving for 5 s: avoidance steers, but the handbrake leg re-tests IsExtremeSwerving and skips");
    }

    Check(gAsserts == 0 && gGiveUps == 0 && gReturns == 0, "Z1 no assert, give-up or return fired on any path");

    std::printf("FxTraffic4Avoidance: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
