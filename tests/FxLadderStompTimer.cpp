// FX-LADDER NaN commit (crash parity 2026-09-25): CrashPlayManager::HandlePlayerToVehicleImpact
// @0x822D5928, the traffic-stomp detector. run_fxladder_stomp_timer.py extracts the production body and
// its two constants from BrnCrashPlayManager.cpp; this fixture runs it on the REAL CrashPlayManager /
// ActiveRaceCar structs. Two doubles: ActiveRaceCar::IsCrashing (the fixture's flag) and
// CrashPlayManager::OnCarCrash (records its calls). BrnTraffic::GetVehicleSpecies is the header's own
// inline range ladder: index < 400 is STANDARD, 400..598 is STATIC (species 1).
//
// ARTIST, read off the asm (0x822D5928..0x822D5AA8):
//   0x822D5928..   !mbIsCrashPlayActive (+0x14C) -> out; ActiveRaceCar crashing (+0x52A) == 0 -> out
//   0x822D59F4/F8  fcmpu |mNormalStress|, 0.08 (flt_82014A5C) ; blt -> out   (NaN: NOT taken, goes on)
//   0x822D5A08     OnCarCrash(id, 1)
//   0x822D5A18/1C  fcmpu mfTimeSinceLastTrafficStomp (+0x130), 1.0 (flt_82001C98) ; blt -> out
//                  raw 41980088 = bc 12,24: taken only when LT is set, so an UNORDERED (NaN) timer is
//                  NOT stopped here and goes on to the owner / species / normal tests.
//   0x822D5A20..38 owner(id) != 2 -> out ; GetVehicleSpecies(index) == 1 -> out
//   0x822D5A3C..94 vcmpgefp. y*y >= fma(x, x, z*z) on mNormal (+0x30) -> +0x130 = 0.0 (0x822D5AA0)
// The pre-fix body gated on `mfTimeSinceLastTrafficStomp >= 1.0`, which stops a NaN timer: the NaN
// case below is RED on it.
#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        std::printf("  [assert] %s\n", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

static bool gbCrashing = true;
static int  giCrashCalls = 0;
bool BrnWorld::ActiveRaceCar::IsCrashing() const { return gbCrashing; }

namespace BrnWorld
{
void CrashPlayManager::OnCarCrash(CgsSceneManager::EntityId, bool) { ++giCrashCalls; }

// The production constants and body, extracted verbatim.
#include "restored_methods.inc"
}

using BrnWorld::CrashPlayManager;
using BrnWorld::ActiveRaceCar;

namespace
{
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    alignas(16) unsigned char gManagerStorage[sizeof(CrashPlayManager)];
    alignas(16) unsigned char gCarStorage[sizeof(ActiveRaceCar)];

    void Check(bool lbPass, const char* lpcName)
    {
        ++gChecks;
        if (!lbPass) ++gFailures;
        std::printf("  %s %s\n", lbPass ? "ok  " : "FAIL", lpcName);
    }

    // Owner byte 2 (traffic) and a global traffic index, as EntityId packs them (index << 10).
    CgsSceneManager::EntityId Traffic(u32 luIndex) { return CgsSceneManager::EntityId{ 0x02000000u | (luIndex << 10) }; }

    struct Case
    {
        bool mbActive;
        bool mbCrashing;
        Vector3 mStress;
        Vector3 mNormal;
        f32 mfTimer;
        CgsSceneManager::EntityId mId;
    };

    // Runs the production body; returns the timer it leaves and how many OnCarCrash calls it made.
    f32 Run(const Case& lrCase, int& liCalls)
    {
        std::memset(gManagerStorage, 0, sizeof(gManagerStorage));
        std::memset(gCarStorage, 0, sizeof(gCarStorage));
        CrashPlayManager& lrManager = *reinterpret_cast<CrashPlayManager*>(gManagerStorage);
        lrManager.mbIsCrashPlayActive = lrCase.mbActive;
        lrManager.mfTimeSinceLastTrafficStomp = lrCase.mfTimer;
        gbCrashing = lrCase.mbCrashing;
        giCrashCalls = 0;
        BrnPhysics::ContactSpy::RaceCarContact lContact;
        std::memset(&lContact, 0, sizeof(lContact));
        lContact.mNormalStress = lrCase.mStress;
        lContact.mNormal = lrCase.mNormal;
        lrManager.HandlePlayerToVehicleImpact(reinterpret_cast<ActiveRaceCar*>(gCarStorage), lrCase.mId, &lContact);
        liCalls = giCrashCalls;
        return lrManager.mfTimeSinceLastTrafficStomp;
    }
}

int main()
{
    const Vector3 lUp        = { 0.0f, 1.0f, 0.0f, 0.0f };
    const Vector3 lSide      = { 1.0f, 0.0f, 0.0f, 0.0f };
    const Vector3 lStress    = { 0.0f, 0.5f, 0.0f, 0.0f };
    const f32     lfDiag     = 0.70710677f;
    const Vector3 lTie       = { lfDiag, lfDiag, 0.0f, 0.0f };   // y*y == x*x + z*z exactly
    int liCalls = 0;
    f32 lfTimer = 0.0f;

    std::printf("the stomp-timer gate @0x822D5A18/1C (blt: NOT taken on unordered):\n");
    lfTimer = Run(Case{ true, true, lStress, lUp, KF_NAN, Traffic(5) }, liCalls);
    Check(lfTimer == 0.0f, "NaN timer, standard traffic, stomp normal -> goes on -> timer reset to 0.0");
    Check(liCalls == 1, "... OnCarCrash called once");
    lfTimer = Run(Case{ true, true, lStress, lUp, 2.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 0.0f, "timer 2.0 >= 1.0 -> stomp -> reset");
    lfTimer = Run(Case{ true, true, lStress, lUp, 1.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 0.0f, "timer exactly 1.0 -> blt not taken -> reset");
    lfTimer = Run(Case{ true, true, lStress, lUp, 0.5f, Traffic(5) }, liCalls);
    Check(lfTimer == 0.5f, "timer 0.5 < 1.0 -> out, timer kept");
    Check(liCalls == 1, "... but OnCarCrash ran first (0x822D5A08)");

    std::printf("the tests a NaN timer now reaches decide it:\n");
    lfTimer = Run(Case{ true, true, lStress, lSide, KF_NAN, Traffic(5) }, liCalls);
    Check(std::isnan(lfTimer), "NaN timer, side normal -> y*y >= x*x+z*z fails -> timer kept");
    lfTimer = Run(Case{ true, true, lStress, lUp, KF_NAN, Traffic(450) }, liCalls);
    Check(std::isnan(lfTimer), "NaN timer, STATIC traffic (species 1) -> out, timer kept");
    lfTimer = Run(Case{ true, true, lStress, lUp, KF_NAN, CgsSceneManager::EntityId{ 0x01000000u } }, liCalls);
    Check(std::isnan(lfTimer), "NaN timer, a race car (owner 1) -> out, timer kept");

    std::printf("the other gates (ordered controls):\n");
    lfTimer = Run(Case{ true, true, lStress, lTie, 2.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 0.0f, "y*y == x*x + z*z -> vcmpgefp. all-true -> reset");
    lfTimer = Run(Case{ true, false, lStress, lUp, 2.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 2.0f && liCalls == 0, "player not crashing -> out before OnCarCrash");
    lfTimer = Run(Case{ false, true, lStress, lUp, 2.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 2.0f && liCalls == 0, "crash play inactive -> out before OnCarCrash");
    lfTimer = Run(Case{ true, true, Vector3{ 0.0f, 0.05f, 0.0f, 0.0f }, lUp, 2.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 2.0f && liCalls == 0, "|stress| 0.05 < 0.08 -> out before OnCarCrash");
    lfTimer = Run(Case{ true, true, Vector3{ KF_NAN, 0.0f, 0.0f, 0.0f }, lUp, 2.0f, Traffic(5) }, liCalls);
    Check(lfTimer == 0.0f && liCalls == 1, "NaN stress -> blt not taken -> OnCarCrash, stomp -> reset");

    std::printf("FxLadderStompTimer: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
