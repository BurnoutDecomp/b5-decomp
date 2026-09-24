// FX-TRAFFIC4 item 2 (crash parity wave 5, 2026-09-24): the three-point turn. The PRODUCTION
// TrafficEntityModule::Update3PointTurnManoeuvre @0x827190B0 with its constants, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic4_three_point_turn.py together with every body it reaches (GetVehicle,
// GetVehicleTransform and the Vehicle manoeuvre / target / speed accessors).
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   lDirToTarget = |target - Pos|^2 != 0 ? normalised : At        (vcmpeqfp128 / vnot128 / vsel128)
//   Dot(At, lDirToTarget) > 0.707 (flt_82011C14, all lanes) -> SetCurrentManoeuvre(NONE): phase 0,
//     mfManoeuvreTime 0, nothing written to the record
//   phase 0: lfSteering = Dot(lDirToTarget, Right (row 0)); |lfSteering| > 0.96 (flt_82097A30, bgt)
//     -> SetCurrentManoeuvrePhase(1) and the phase-1 controls THIS frame; else mfSteering = lfSteering,
//     mfGas = 0, mfBrake = 0.8 (flt_820BA5B4)
//   phase 1: mfGas = 0.8, mfBrake = 0, mfSteering = -Dot(lDirToTarget, Right)
//   any other phase: the one "Invalid phase" assert (.cpp 16888), nothing written
//   mfHandBrake and mfManoeuvreTime are never written by a phase
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
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
}

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct TurnFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::maVehicles)          maVehicles;
        decltype(M::maVehicleTransforms) maVehicleTransforms;

        Vehicle*       GetVehicle(u32 luIndex);
        Matrix44Affine GetVehicleTransform(u32 luIndex) const;

        void Update3PointTurnManoeuvre(u32 luVehicle, BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
    };
}

// The production bodies under test.
#include "three_point_turn.inc"

using namespace BrnTraffic;
typedef TurnFixture Fixture;
typedef BrnPhysics::Vehicle::BrnTrafficDriverControls Controls;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaFixture[sizeof(Fixture)];
static Fixture& F() { return *reinterpret_cast<Fixture*>(gaFixture); }

static const u32 KU_CAR = 9;   // a standard traffic car
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

static Vehicle& Car() { return F().maVehicles[KU_CAR]; }

static Controls Sentinel()
{
    Controls l;
    std::memset(&l, 0, sizeof(l));
    l.mfGas = 9.0f; l.mfBrake = 9.0f; l.mfHandBrake = 9.0f; l.mfSteering = 9.0f;
    return l;
}

// The car sits at (10, 0, 20) facing +Z with Right = +X, in the given phase, 1.25 s into the manoeuvre.
static void Turning(s8 liPhase, Vector3 lTarget)
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        F().maVehicleTransforms[luVehicle].SetIdentity();
    }
    F().maVehicleTransforms[KU_CAR].wAxis = { 10.0f, 0.0f, 20.0f, 1.0f };
    Vehicle& lrCar = Car();
    lrCar.mxFlags          = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    lrCar.muSpecies        = Vehicle::E_SPECIES_STANDARD;
    lrCar.miManoeuvre      = Vehicle::E_MANOEUVRE_3_POINT_TURN;
    lrCar.miManoeuvrePhase = liPhase;
    lrCar.mfManoeuvreTime  = 1.25f;
    lrCar.mTargetPos       = lTarget;
}

static bool Near(f32 a, f32 b) { return std::fabs(a - b) < 1e-5f; }

int main()
{
    // Offsets from the car at (10, 0, 20): +Z ahead, +X right.
    const Vector3 lAheadRight30 = { 10.0f + 5.0f,  0.0f, 20.0f + 8.660254f, 1.0f };   // 30 deg off At
    const Vector3 lBehindLeft   = { 10.0f - 6.0f,  0.0f, 20.0f - 8.0f,      1.0f };   // dir (-0.6, 0, -0.8)
    const Vector3 lRight90      = { 10.0f + 10.0f, 0.0f, 20.0f,             1.0f };   // dir (1, 0, 0)
    const Vector3 lBehindRight  = { 10.0f + 6.0f,  0.0f, 20.0f - 8.0f,      1.0f };   // dir (0.6, 0, -0.8)

    // ---- the end test -----------------------------------------------------------------------------
    for (s8 liPhase = 0; liPhase <= 1; ++liPhase)
    {
        Turning(liPhase, lAheadRight30);
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE && Car().miManoeuvrePhase == 0
              && Car().mfManoeuvreTime == 0.0f,
              liPhase == 0 ? "T1 phase 0, target 30 deg off At (dot 0.866 > 0.707): the turn ends -- NONE, phase 0, time 0"
                           : "T2 phase 1, target 30 deg off At: the turn ends the same way");
        Check(l.mfGas == 9.0f && l.mfBrake == 9.0f && l.mfSteering == 9.0f && l.mfHandBrake == 9.0f,
              liPhase == 0 ? "T3 ... and the record is not written (the arm sends it as it stands)"
                           : "T4 ... and the record is not written in phase 1 either");
    }

    // 46 degrees off: dot 0.6947 < 0.707 -> not over (the threshold is 0.707 = flt_82011C14, not cos 45 exactly).
    Turning(1, Vector3{ 10.0f + 7.193398f, 0.0f, 20.0f + 6.946584f, 1.0f });
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN && l.mfGas == 0.8f,
              "T5 46 deg off (dot 0.6947 < 0.707) is not over: phase 1 keeps driving");
    }

    // ---- phase 0: reverse --------------------------------------------------------------------------
    Turning(0, lBehindLeft);
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Near(l.mfSteering, -0.6f) && l.mfGas == 0.0f && l.mfBrake == 0.8f && l.mfHandBrake == 9.0f,
              "P1 phase 0 reverses: steering = Dot(dir, Right) = -0.6, gas 0, brake 0.8 (flt_820BA5B4), handbrake untouched");
        Check(Car().miManoeuvrePhase == 0 && Car().miManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN
              && Car().mfManoeuvreTime == 1.25f,
              "P2 ... stays in phase 0 and the manoeuvre time is not touched");
    }

    Turning(0, lBehindRight);
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Near(l.mfSteering, 0.6f) && l.mfBrake == 0.8f,
              "P3 target behind-right: phase 0 steering +0.6 (Right is row 0, not At)");
    }

    // |side| just under 0.96 keeps reversing: dir (0.959, 0, -0.2834).
    Turning(0, Vector3{ 10.0f + 9.59f, 0.0f, 20.0f - 2.8334f, 1.0f });
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 0 && l.mfBrake == 0.8f && l.mfSteering > 0.95f && l.mfSteering < 0.96f,
              "P4 |side| 0.959 <= 0.96 (flt_82097A30) keeps reversing");
    }

    // |side| above 0.96: straight to phase 1 THIS frame.
    Turning(0, lRight90);
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 1 && Car().miManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN,
              "P5 |side| 1.0 > 0.96: phase 0 -> 1");
        Check(l.mfGas == 0.8f && l.mfBrake == 0.0f && Near(l.mfSteering, -1.0f) && l.mfHandBrake == 9.0f
              && Car().mfManoeuvreTime == 1.25f,
              "P6 ... and the SAME frame falls into phase 1: gas 0.8, brake 0, steering -1, time untouched");
    }

    Turning(0, Vector3{ 10.0f - 10.0f, 0.0f, 20.0f, 1.0f });   // left 90: side -1
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 1 && Near(l.mfSteering, 1.0f) && l.mfGas == 0.8f,
              "P7 |side| is an ABSOLUTE test: side -1 also hands over, steering +1");
    }

    // ---- phase 1: drive forward, opposite lock --------------------------------------------------
    Turning(1, lBehindLeft);
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(l.mfGas == 0.8f && l.mfBrake == 0.0f && Near(l.mfSteering, 0.6f) && l.mfHandBrake == 9.0f
              && Car().miManoeuvrePhase == 1,
              "Q1 phase 1: gas 0.8, brake 0, steering = -Dot(dir, Right) = +0.6, stays phase 1");
    }

    // ---- degenerate directions -------------------------------------------------------------------
    Turning(0, Vector3{ 10.0f, 0.0f, 20.0f, 1.0f });   // sitting on its target
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE && l.mfBrake == 9.0f,
              "D1 a zero offset selects At (vsel128): Dot(At, At) = 1 > 0.707, the turn ends");
    }

    Turning(0, Vector3{ KF_NAN, 0.0f, 20.0f, 1.0f });
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN && Car().miManoeuvrePhase == 0
              && l.mfGas == 0.0f && l.mfBrake == 0.8f && l.mfSteering != l.mfSteering,
              "D2 a NaN target: the end test is false (vcmpgtfp), the phase test keeps phase 0 (bgt): brake 0.8, NaN steering");
    }

    const unsigned luAssertsBeforeBadPhase = gAsserts;
    Check(luAssertsBeforeBadPhase == 0, "A1 no assert fired on any valid path");

    Turning(2, lBehindLeft);
    {
        Controls l = Sentinel();
        F().Update3PointTurnManoeuvre(KU_CAR, &l);
        Check(gAsserts == luAssertsBeforeBadPhase + 1 && l.mfGas == 9.0f && l.mfBrake == 9.0f && l.mfSteering == 9.0f
              && Car().miManoeuvre == Vehicle::E_MANOEUVRE_3_POINT_TURN && Car().miManoeuvrePhase == 2,
              "A2 an invalid phase fires the one \"Invalid phase\" assert (.cpp 16888) and writes nothing");
    }

    std::printf("FxTraffic4ThreePointTurn: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
