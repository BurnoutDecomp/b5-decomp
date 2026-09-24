// FX-TRAFFIC3 item 1c (crash parity wave 5, 2026-09-24): the GIVE_UP manoeuvre. The PRODUCTION
// TrafficEntityModule::UpdateGiveUpManoeuvre @0x8273EB60, TrafficPhysicsInfo::IsStuckFront /
// IsStuckBack and KF_VEHICLE_IS_STUCK_TIME, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic3_give_up.py with every body they reach (CheckIfPhysicalVehicleIsStuck and its
// callees, the Vehicle manoeuvre / speed / indicator / reason accessors, RandomFloat(f32, f32)).
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   phase 0   |speed| > 1.0 (flt_82001C98): gas = Min(Max(-speed, 0), 1), steering 0.5
//             (flt_820BA62C), brake = Min(Max(speed, 0), 1); else phase 1 + time 0, and the SAME
//             call runs phase 1 (time 0 < 4 -> gas / brake / steering 0)
//   phase 1   time < 4.0 (flt_820BA8DC): gas / brake / steering 0 (handbrake untouched); else
//             front > 0.5 && back > 0.5 (IsStuckFront / IsStuckBack, flt_820BA62C) -> time 0;
//             else unless CheckIfPhysicalVehicleIsStuck && still GIVE_UP: indicators off,
//             mfTimeNotDriving 0, physical reason 5 (NORMAL); the manoeuvre byte is not touched
//   other     the "Invalid phase" assert, nothing written
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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

    struct GiveUpFixture
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
        decltype(M::mfSimTimeStep)                mfSimTimeStep;

        Vehicle*            GetVehicle(u32 luIndex);
        TrafficPhysicsInfo* GetTrafficPhysicsInfoForVehicl(u32 luVehicle);
        bool                NeedToTakeActionAgainstJunctionFUP();

        bool CheckIfPhysicalVehicleIsStuck(u32 luVehicle);
        void UpdateGiveUpManoeuvre(u32 luVehicle, BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
    };
}

// The production bodies under test.
#include "give_up.inc"

using namespace BrnTraffic;
typedef GiveUpFixture Fixture;
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

static const u32 KU_CAR  = 11;   // a standard traffic car
static const u32 KU_SLOT = 5;    // its TrafficPhysicsInfo slot

static Vehicle&            Car()  { return F().maVehicles[KU_CAR]; }
static TrafficPhysicsInfo& Info() { return F().maTrafficPhysicsInfoList[KU_SLOT]; }

static Controls Sentinel()
{
    Controls l;
    std::memset(&l, 0, sizeof(l));
    l.mfGas = 9.0f; l.mfBrake = 9.0f; l.mfHandBrake = 9.0f; l.mfSteering = 9.0f;
    return l;
}

// A given-up car in liPhase, liPhase's clock at lfTime, moving at lfSpeed, with the stuck timers.
static void GivenUp(s8 liPhase, f32 lfTime, f32 lfSpeed, f32 lfFront = 0.0f, f32 lfBack = 0.0f)
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
    lrCar.miPhysicalReason     = static_cast<s8>(E_PHYSICALREASON_SWERVING);
    lrCar.miManoeuvre          = Vehicle::E_MANOEUVRE_GIVE_UP;
    lrCar.miManoeuvrePhase     = liPhase;
    lrCar.mfManoeuvreTime      = lfTime;
    lrCar.mSpeed_DistAcrossLane_SwerveAmount_W.x = lfSpeed;
    lrCar.mxEffectState        = 0x20 | 0x02;   // indicating left, left bulb lit
    lr.maTrafficPhysicsInfoListBits.SetBit(KU_SLOT);
    Info().muOwningVehicleIndex = static_cast<u16>(KU_CAR);
    Info().mfStuckTimeFront     = lfFront;
    Info().mfStuckTimeBack      = lfBack;
    Info().mfTimeNotDriving     = 7.0f;
    lr.mfSimTimeStep            = 1.0f / 60.0f;
    lr.mRand.Construct();
    lr.mEffectRand.Construct();
}

static bool HandedBack()
{
    return Car().miPhysicalReason == E_PHYSICALREASON_NORMAL && Info().mfTimeNotDriving == 0.0f
        && (Car().mxEffectState & 0x22) == 0;
}

static bool Untouched(const Controls& l)
{
    return l.mfGas == 9.0f && l.mfBrake == 9.0f && l.mfSteering == 9.0f && l.mfHandBrake == 9.0f;
}

int main()
{
    // ---- phase 0: brake against the motion ------------------------------------------------------
    GivenUp(0, 0.5f, 5.0f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(l.mfGas == 0.0f && l.mfBrake == 1.0f && l.mfSteering == 0.5f && l.mfHandBrake == 9.0f
              && Car().miManoeuvrePhase == 0,
              "G1 phase 0 rolling forward at 5: brake Min(Max(5,0),1) = 1, gas 0, steering 0.5 (flt_820BA62C)");
    }

    GivenUp(0, 0.5f, -3.0f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(l.mfGas == 1.0f && l.mfBrake == 0.0f && l.mfSteering == 0.5f,
              "G2 phase 0 rolling backwards at 3: gas Min(Max(3,0),1) = 1, brake 0");
    }

    GivenUp(0, 0.5f, 1.5f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 0 && l.mfBrake == 1.0f && l.mfGas == 0.0f,
              "G2b 1.5 m/s is still moving (> 1.0): phase 0 keeps braking (Min(1.5, 1) = 1)");
    }

    GivenUp(0, 0.5f, 1.0f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvrePhase == 1 && Car().mfManoeuvreTime == 0.0f,
              "G3 |speed| == 1.0 (flt_82001C98) is stopped: phase 1, time 0");
        Check(l.mfGas == 0.0f && l.mfBrake == 0.0f && l.mfSteering == 0.0f && l.mfHandBrake == 9.0f,
              "G4 ... and the same call runs phase 1: gas / brake / steering 0, handbrake untouched");
    }

    // ---- phase 1: wait 4 s, then hand back ------------------------------------------------------
    GivenUp(1, 3.9f, 0.0f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(l.mfGas == 0.0f && l.mfBrake == 0.0f && l.mfSteering == 0.0f && !HandedBack()
              && Car().miPhysicalReason == E_PHYSICALREASON_SWERVING,
              "G5 phase 1 under 4.0 s (flt_820BA8DC): zero pedals and steering, no hand-back");
    }

    GivenUp(1, 4.0f, 0.0f, 0.6f, 0.6f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(Car().mfManoeuvreTime == 0.0f && Untouched(l) && !HandedBack()
              && Car().miManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP,
              "G6 at 4.0 s still touching at both ends (> 0.5, KF_VEHICLE_IS_STUCK_TIME): the wait restarts");
    }

    GivenUp(1, 4.0f, 0.0f, 0.6f, 0.5f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(HandedBack() && Untouched(l) && Car().miManoeuvre == Vehicle::E_MANOEUVRE_GIVE_UP
              && Car().miManoeuvrePhase == 1,
              "G7 back exactly 0.5 is not stuck (strict): indicators off, time-not-driving 0, reason NORMAL (5); "
              "the manoeuvre byte stays GIVE_UP");
    }

    GivenUp(1, 4.5f, 0.0f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(HandedBack() && Car().mfManoeuvreTime == 4.5f, "G8 free at 4.5 s: handed back, the clock not reset");
    }

    // A nose still pinned past 3.2 s: the stuck test takes it (NONE after a high roll) -- no longer GIVE_UP,
    // so it is NOT "still stuck" and is handed back too.
    GivenUp(1, 4.0f, 0.0f, 3.3f, 0.0f);
    {
        CgsNumeric::Random& lrRand = F().mEffectRand;
        lrRand.mafFloatBuffer[lrRand.muOldestBufferIndex] = 1.9f;   // roll 90 > 0.4
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(Car().miManoeuvre == Vehicle::E_MANOEUVRE_NONE && HandedBack(),
              "G9 nose pinned: CheckIfPhysicalVehicleIsStuck makes it NONE, so it is handed back");
    }

    // The junction-FUP override: the stuck test zeroes the timers and says no -> handed back.
    GivenUp(1, 4.0f, 0.0f, 0.6f, 0.0f);
    F().mbDEBUGOverrideJunctionFUP = true;
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(HandedBack() && Info().mfStuckTimeFront == 0.0f, "G10 the stuck test runs in phase 1 (FUP reset seen)");
    }

    Check(gAsserts == 0, "A1 no assert fired on any valid path");

    GivenUp(2, 4.0f, 3.0f);
    {
        Controls l = Sentinel();
        F().UpdateGiveUpManoeuvre(KU_CAR, &l);
        Check(gAsserts == 1 && Untouched(l) && !HandedBack(),
              "G11 an invalid phase fires the one \"Invalid phase\" assert (.cpp 16973) and writes nothing");
    }

    std::printf("FxTraffic3GiveUp: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
