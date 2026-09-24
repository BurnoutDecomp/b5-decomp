// FX-TRAFFIC3 item 1b (crash parity wave 5, 2026-09-24): CC-2's back-off. The PRODUCTION
// TrafficEntityModule::CheckIfPhysicalVehicleIsStuck @0x8272C010 and UpdateStuckReverseManoeuvre
// @0x82719430 with their constants, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic3_stuck_reverse.py together with every body they reach (GetVehicle,
// GetVehicleTransform, GetTrafficPhysicsInfoForVehicl, NeedToTakeActionAgainstJunctionFUP, the
// Vehicle manoeuvre / speed / target accessors and CgsNumeric::Random::RandomFloat(f32, f32)).
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   CheckIfPhysicalVehicleIsStuck
//     0x8272C058  mbDEBUGOverrideJunctionFUP, or mbAllowDivergentBehaviour && mfJunctionFUP >= 65
//                 -> back / front / debounce timers zeroed, return false
//     0x8272C114  stuck == timer > 3.2 (flt_820BA868), strictly, NaN not stuck
//     both        -> GIVE_UP, phase 1 (StartGiveUpManoeuvre + SetCurrentManoeuvrePhase(1)), true
//     front only  -> ONE mEffectRand draw (this+0x1360, not mRand) * 100 (flt_820BA5C8);
//                    > 0.4 (flt_8200473C) -> NONE, else STUCK_REVERSE; true
//     back only   -> NONE, no draw, true
//   UpdateStuckReverseManoeuvre (K = {3.0, 0.866, 2.0, 0} @unk_8300C9C0)
//     phase 0     steer = Dot(dir to target, At); back contact (+0x1008 & 2), or 0.866 > |steer|,
//                 or time >= 3.0 -> phase 1 + time 0; else steer, gas 0, brake 0.75 (flt_82004018)
//     phase 1     |speed| > 2.0 && 3.0 > time -> gas 0, brake 0, handbrake 0.4; else NONE, time 0
//     other       the "Invalid phase" assert, nothing written
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

    struct StuckFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::maVehicles)                   maVehicles;
        decltype(M::maVehicleTransforms)          maVehicleTransforms;
        decltype(M::maTrafficPhysicsInfoList)     maTrafficPhysicsInfoList;
        decltype(M::maTrafficPhysicsInfoListBits) maTrafficPhysicsInfoListBits;
        decltype(M::mRand)                        mRand;
        decltype(M::mEffectRand)                  mEffectRand;
        decltype(M::mbDEBUGOverrideJunctionFUP)   mbDEBUGOverrideJunctionFUP;
        decltype(M::mbAllowDivergentBehaviour)    mbAllowDivergentBehaviour;
        decltype(M::mfJunctionFUP)                mfJunctionFUP;

        Vehicle*            GetVehicle(u32 luIndex);
        Matrix44Affine      GetVehicleTransform(u32 luIndex) const;
        TrafficPhysicsInfo* GetTrafficPhysicsInfoForVehicl(u32 luVehicle);
        bool                NeedToTakeActionAgainstJunctionFUP();

        bool CheckIfPhysicalVehicleIsStuck(u32 luVehicle);
        void UpdateStuckReverseManoeuvre(u32 luVehicle, BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
    };
}

// The production bodies under test.
#include "stuck_reverse.inc"

using namespace BrnTraffic;
typedef StuckFixture Fixture;
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

static const u32 KU_CAR   = 7;   // a standard traffic car
static const u32 KU_SLOT  = 3;   // its TrafficPhysicsInfo slot
static const f32 KF_NAN   = std::numeric_limits<f32>::quiet_NaN();

static Vehicle&            Car()  { return F().maVehicles[KU_CAR]; }
static TrafficPhysicsInfo& Info() { return F().maTrafficPhysicsInfoList[KU_SLOT]; }

static void Fresh()
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    Fixture& lr = F();
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicleTransforms[luVehicle].SetIdentity();
    }
    Vehicle& lrCar = Car();
    lrCar.mxFlags              = Vehicle::E_FLAG_ALIVE | Vehicle::E_FLAG_PHYSICAL;
    lrCar.muSpecies            = Vehicle::E_SPECIES_STANDARD;
    lrCar.miPhysicalPartsIndex = static_cast<s8>(KU_SLOT);
    lrCar.miManoeuvre          = Vehicle::E_MANOEUVRE_EXTREME_SWERVE;   // anything but NONE, to see it change
    lrCar.miManoeuvrePhase     = 1;
    lrCar.mfManoeuvreTime      = 1.5f;
    lr.maTrafficPhysicsInfoListBits.SetBit(KU_SLOT);
    Info().muOwningVehicleIndex = static_cast<u16>(KU_CAR);
    lr.mRand.Construct();
    lr.mEffectRand.Construct();
    lr.mEffectRand.muSeed = 0x0123456789ABCDEFull;   // distinct streams
}

// The next mEffectRand draw returns lfFraction * 100.
static void NextRoll(f32 lfOnePlusFraction)
{
    CgsNumeric::Random& lr = F().mEffectRand;
    lr.mafFloatBuffer[lr.muOldestBufferIndex] = lfOnePlusFraction;
}

static void Timers(f32 lfFront, f32 lfBack, f32 lfDebounce = 0.7f)
{
    Info().mfStuckTimeFront     = lfFront;
    Info().mfStuckTimeBack      = lfBack;
    Info().mfStuckTimerDebounce = lfDebounce;
}

static bool SameRandom(const CgsNumeric::Random& a, const CgsNumeric::Random& b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

// ---- UpdateStuckReverseManoeuvre helpers ----------------------------------------------------
static Controls Sentinel()
{
    Controls l;
    std::memset(&l, 0, sizeof(l));
    l.mfGas = 9.0f; l.mfBrake = 9.0f; l.mfHandBrake = 9.0f; l.mfSteering = 9.0f;
    return l;
}

static void Reversing(s8 liPhase, f32 lfTime, Vector3 lTarget, f32 lfSpeed = 0.0f, u8 luContactFlags = 0)
{
    Fresh();
    Car().miManoeuvre      = Vehicle::E_MANOEUVRE_STUCK_REVERSE;
    Car().miManoeuvrePhase = liPhase;
    Car().mfManoeuvreTime  = lfTime;
    Car().mTargetPos       = lTarget;
    Car().mSpeed_DistAcrossLane_SwerveAmount_W.x = lfSpeed;
    Info().muContactSideFlags = luContactFlags;
    // The car sits at (10, 0, 20) facing +Z.
    F().maVehicleTransforms[KU_CAR].wAxis = { 10.0f, 0.0f, 20.0f, 1.0f };
}

int main()
{
    // =================================== CheckIfPhysicalVehicleIsStuck ===========================
    // ---- the junction-FUP reset --------------------------------------------------------------
    Fresh();
    Timers(5.0f, 5.0f);
    F().mbDEBUGOverrideJunctionFUP = true;
    Check(!F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Info().mfStuckTimeFront == 0.0f && Info().mfStuckTimeBack == 0.0f
          && Info().mfStuckTimerDebounce == 0.0f && Car().miManoeuvre == Vehicle::E_MANOEUVRE_EXTREME_SWERVE,
          "S1 FUP override: false, the back / front / debounce timers are zeroed, the manoeuvre is untouched");

    Fresh();
    Timers(5.0f, 5.0f);
    F().mbAllowDivergentBehaviour = true;
    F().mfJunctionFUP = 65.0f;
    Check(!F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Info().mfStuckTimeFront == 0.0f,
          "S2 divergent && mfJunctionFUP >= 65 (flt_820BA290) takes the same reset");

    Fresh();
    Timers(5.0f, 0.0f);
    F().mbAllowDivergentBehaviour = true;
    F().mfJunctionFUP = 64.9f;
    NextRoll(1.5f);
    Check(F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Info().mfStuckTimeFront == 5.0f,
          "S3 below 65 the FUP reset is not taken and the timers are kept");

    // ---- not stuck -----------------------------------------------------------------------------
    Fresh();
    Timers(3.2f, 3.2f);
    {
        const CgsNumeric::Random lBefore = F().mEffectRand;
        Check(!F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Car().miManoeuvre == Vehicle::E_MANOEUVRE_EXTREME_SWERVE
              && SameRandom(lBefore, F().mEffectRand) && Info().mfStuckTimeFront == 3.2f,
              "S4 exactly 3.2 s (flt_820BA868) is not stuck (bgt): false, nothing changes, no draw");
    }

    Fresh();
    Timers(KF_NAN, KF_NAN);
    Check(!F().CheckIfPhysicalVehicleIsStuck(KU_CAR), "S5 a NaN timer is not stuck");

    // ---- front stuck: the reverse roll ---------------------------------------------------------
    Fresh();
    Timers(3.3f, 0.0f);
    NextRoll(1.003f);   // 0.3 %
    {
        const CgsNumeric::Random lRandBefore = F().mRand;
        const u32 luIndexBefore = F().mEffectRand.muOldestBufferIndex;
        const bool lbStuck = F().CheckIfPhysicalVehicleIsStuck(KU_CAR);
        Check(lbStuck && Car().miManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE && Car().miManoeuvrePhase == 0
              && Car().mfManoeuvreTime == 0.0f,
              "S6 front stuck, roll 0.3 <= 0.4 (flt_8200473C): STUCK_REVERSE, phase 0, time 0, true");
        Check(F().mEffectRand.muOldestBufferIndex == ((luIndexBefore + 1) & 7) && SameRandom(lRandBefore, F().mRand),
              "S7 the roll is ONE mEffectRand draw (this+0x1360); mRand is untouched");
        Check(Info().mfStuckTimeFront == 3.3f && Info().mfStuckTimerDebounce == 0.7f,
              "S8 outside the FUP reset the timers are not touched");
    }

    Fresh();
    Timers(3.3f, 0.0f);
    NextRoll(1.005f);   // 0.5 %
    Check(F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE
          && Car().miManoeuvrePhase == 0 && Car().mfManoeuvreTime == 0.0f,
          "S9 front stuck, roll 0.5 > 0.4: NONE (phase reset on the change, time 0), still true -- no driving");

    Fresh();
    Timers(3.3f, 0.0f);
    NextRoll(1.9f);     // 90 %: the range is 100 (flt_820BA5C8), not 1
    Check(F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE,
          "S10 the roll is a percent: a 0.9 fraction (90) is far above 0.4");

    // ---- back stuck only: no draw --------------------------------------------------------------
    Fresh();
    Timers(0.0f, 3.3f);
    {
        const CgsNumeric::Random lBefore = F().mEffectRand;
        Check(F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE
              && SameRandom(lBefore, F().mEffectRand),
              "S11 back stuck only: NONE, true, and no random draw");
    }

    // ---- both ends: give up --------------------------------------------------------------------
    Fresh();
    Timers(3.3f, 3.3f);
    {
        const CgsNumeric::Random lBefore = F().mEffectRand;
        Check(F().CheckIfPhysicalVehicleIsStuck(KU_CAR) && Car().miManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP
              && Car().miManoeuvrePhase == 1 && Car().mfManoeuvreTime == 0.0f && SameRandom(lBefore, F().mEffectRand),
              "S12 stuck at both ends: GIVE_UP phase 1 (StartGiveUpManoeuvre + SetCurrentManoeuvrePhase(1)), no draw");
    }

    // =================================== UpdateStuckReverseManoeuvre =============================
    // Behind the car (target at -Z): the direction to target is (0, 0, -1), Dot with At == -1.
    const Vector3 lBehind = { 10.0f, 0.0f, 10.0f, 1.0f };

    Reversing(0, 0.5f, lBehind);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(l.mfGas == 0.0f && l.mfBrake == 0.75f && l.mfSteering == -1.0f && l.mfHandBrake == 9.0f,
              "R1 phase 0 reverses: gas 0, brake 0.75 (flt_82004018), steering = Dot(dir, At) = -1, handbrake untouched");
        Check(Car().miManoeuvrePhase == 0 && Car().mfManoeuvreTime == 0.5f
              && Car().miManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE, "R2 ... and stays in phase 0");
    }

    Reversing(0, 0.5f, lBehind, 0.0f, TrafficPhysicsInfo::E_CONTACT_SIDE_BACK);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 1 && Car().mfManoeuvreTime == 0.0f && l.mfGas == 9.0f && l.mfBrake == 9.0f,
              "R3 phase 0 with the BACK in contact (+0x1008 & 2): phase 1, time 0, no controls written");
    }

    Reversing(0, 0.5f, lBehind, 0.0f, TrafficPhysicsInfo::E_CONTACT_SIDE_FRONT);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 0 && l.mfBrake == 0.75f, "R4 a FRONT contact does not end the reverse");
    }

    // 45 degrees off: |Dot| == 0.707 < 0.866 -> turned enough.
    Reversing(0, 0.5f, Vector3{ 20.0f, 0.0f, 10.0f, 1.0f });
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 1 && Car().mfManoeuvreTime == 0.0f && l.mfBrake == 9.0f,
              "R5 phase 0 ends once 0.866 (flt_820C0800) > |steer| (45 degrees off)");
    }

    // 20 degrees off: |Dot| == 0.94 > 0.866 -> keep reversing, steering carries the dot.
    Reversing(0, 0.5f, Vector3{ 10.0f + 3.6397023f, 0.0f, 10.0f, 1.0f });
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        const f32 lfExpected = -10.0f / std::sqrt(10.0f * 10.0f + 3.6397023f * 3.6397023f);
        Check(Car().miManoeuvrePhase == 0 && std::fabs(l.mfSteering - lfExpected) < 1e-5f && l.mfBrake == 0.75f,
              "R6 20 degrees off keeps reversing with steering == the dot (-0.94)");
    }

    Reversing(0, 3.0f, lBehind);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 1 && Car().mfManoeuvreTime == 0.0f && l.mfBrake == 9.0f,
              "R7 phase 0 times out at time >= 3.0 (flt_820BA5F4), exactly 3.0 included");
    }

    Reversing(0, 2.9f, lBehind);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 0 && l.mfBrake == 0.75f, "R8 2.9 s is still reversing");
    }

    // Sitting on its target: the direction falls back to the car's own At (Dot == +1).
    Reversing(0, 0.5f, Vector3{ 10.0f, 0.0f, 20.0f, 1.0f });
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(l.mfSteering == 1.0f && l.mfBrake == 0.75f, "R9 a zero offset selects At (vsel128): steering +1");
    }

    // ---- phase 1: stop on the handbrake, then hand back -------------------------------------------
    Reversing(1, 1.0f, lBehind, -3.0f);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(l.mfGas == 0.0f && l.mfBrake == 0.0f && l.mfHandBrake == 0.4f && l.mfSteering == 9.0f
              && Car().miManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE && Car().miManoeuvrePhase == 1,
              "R10 phase 1 while |speed| (3, reversing) > 2.0 (flt_820BA86C): gas 0, brake 0, handbrake 0.4, steering kept");
    }

    Reversing(1, 1.0f, lBehind, -2.0f);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE && Car().miManoeuvrePhase == 0
              && Car().mfManoeuvreTime == 0.0f && l.mfHandBrake == 9.0f,
              "R11 |speed| == 2.0 is slow enough: the manoeuvre ends (NONE, phase 0, time 0), no controls written");
    }

    Reversing(1, 3.0f, lBehind, 5.0f);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE, "R12 phase 1 ends at 3.0 s even while still fast");
    }

    const unsigned luAssertsBeforeBadPhase = gAsserts;
    Check(luAssertsBeforeBadPhase == 0, "A1 no assert fired on any valid path");

    Reversing(2, 1.0f, lBehind, 5.0f);
    {
        Controls l = Sentinel();
        F().UpdateStuckReverseManoeuvre(KU_CAR, &l);
        Check(gAsserts == luAssertsBeforeBadPhase + 1 && l.mfGas == 9.0f && l.mfHandBrake == 9.0f
              && Car().miManoeuvre == Vehicle::E_MANOEUVRE_STUCK_REVERSE,
              "R13 an invalid phase fires the one \"Invalid phase\" assert (.cpp 17053) and writes nothing");
    }

    std::printf("FxTraffic3StuckReverse: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
