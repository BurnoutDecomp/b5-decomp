// FX-TRAFFIC (crash parity 2026-09-23, G59-D1): the PRODUCTION TrafficEntityModule::
// HandleExternalRequests (with its drive-thru arm), ClearupCrashedTraffic and
// KillAllTrafficInCylinder, extracted from
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp by
// run_fxtraffic_drive_thru_clearup.py and hosted on a fixture with the real member types.
// RemoveVehicle is a RECORDER here (it is not under test).
//
// Checked against the ARTIST asm:
//   HandleExternalRequests @0x8274B660
//     0x8274B7CC  `addi r11, r3, -0xD` + a 232-entry table: cases 84..87 == action ids 97..100
//     0x8274BFA8  `lbzx r11, r31, r22` (r22 = 0x717DC, mbIsOnlineGameMode) ; bne -> skip
//     0x8274BFB8  ClearupCrashedTraffic()
//     0x8274BFD8  KillAllTrafficInCylinder(v1 = this+0x713D0, f1 = 250.0 (flt_82004A24),
//                 f2 = 1000.0 (flt_820BA604), r6 = 0)
//   ClearupCrashedTraffic @0x8273CBE0
//     0x8273CBF8  a stack snapshot of mPhysicalVehicles & mAliveVehicles, walked bit by bit
//     0x8273CEA4  mbIsFatallyCrashing -> remove ; else GetCurrentManoeuvre() == 3 -> remove
//   KillAllTrafficInCylinder @0x82741C58
//     alive (byte+5 & 1) ; unless the bool, species byte (+4 & 0xF) == STATIC is skipped ;
//     horizontal d^2 (y zeroed) > r^2 -> skip ; |dy| >= h -> skip ; else RemoveVehicle
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTrackWitness.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

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

// HandleExternalRequests' action-236 arm carries a [nettraf] witness since b5 ca4ac341. The real
// declaration is included above, so this silent definition must keep its signature. The drive-thru
// actions under test never reach that arm.
namespace BrnNetHarnessPC
{
    void WitnessTag(const char*, const char*, const char*, ...) {}
}

namespace BrnTraffic
{
    const char* gpcTrafficRemoveReason = "unknown";
    inline void LogMissingLeg_T6(bool&, const char*) {}
namespace
{
    // The network arms' capped [netcrash] witness (crash parity FX-NETCRASH), off here.
    CgsDev::Log::DebugPrint* NetCrashDiagStream() { return nullptr; }
    const s32 KI_NETCRASH_HULL_DIAG_MAX_LINES = 48;
}

    struct DriveThruFixture
    {
        typedef TrafficEntityModule M;

        // What HandleExternalRequests' network arms read (actions 47 SET_COUNTDOWN, 143
        // SHOWTIME_MODE_SWITCH, 225/226 local player gone, 236 RESTART_TRAFFIC; b5 b43b5c2b). The
        // drive-thru actions under test never reach them, so IsPaused / RestartTraffic are inert
        // stand-ins that count their calls.
        typedef M::EState        EState;
        typedef M::ERunningState ERunningState;
        static const EState        E_STATE_STARTING_UP   = M::E_STATE_STARTING_UP;
        static const EState        E_STATE_RUNNING       = M::E_STATE_RUNNING;
        static const ERunningState E_RUNNINGSTATE_NORMAL = M::E_RUNNINGSTATE_NORMAL;
        decltype(M::meState)                         meState;
        decltype(M::meRunningState)                  meRunningState;
        decltype(M::meRunningStateToUseAfterStartup) meRunningStateToUseAfterStartup;
        decltype(M::mbActivateOnlineHullsAfterReset) mbActivateOnlineHullsAfterReset;
        decltype(M::mau16HullsToActivateAfterReset)  mau16HullsToActivateAfterReset;
        decltype(M::muCurrentlyPredictedHull)        muCurrentlyPredictedHull;
        unsigned muNetworkArmCalls;
        bool IsPaused()       { ++muNetworkArmCalls; return false; }
        void RestartTraffic() { ++muNetworkArmCalls; }

        decltype(M::mbIsOnlineGameMode)             mbIsOnlineGameMode;
        decltype(M::mbAllowDivergentBehaviour)      mbAllowDivergentBehaviour;
        decltype(M::mfBaseDensityScale)             mfBaseDensityScale;
        decltype(M::mfGameModeDensityScale)         mfGameModeDensityScale;
        decltype(M::mLocalPlayerPosition)           mLocalPlayerPosition;
        decltype(M::maVehicles)                     maVehicles;
        decltype(M::maVehicleTransforms)            maVehicleTransforms;
        decltype(M::mVehicleSoaData)                mVehicleSoaData;
        decltype(M::maTrafficPhysicsInfoList)       maTrafficPhysicsInfoList;
        decltype(M::maTrafficPhysicsInfoListBits)   maTrafficPhysicsInfoListBits;

        Vehicle*            GetVehicle(u32 luIndex);
        Matrix44Affine      GetVehicleTransform(u32 luIndex) const;
        TrafficPhysicsInfo* GetTrafficPhysicsInfoForVehicl(u32 luVehicle);

        void HandleExternalRequests(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);
        void HandlePrepareForModeAction(const BrnTrafficIO::InputBuffer_PostPhysics*,
                                        const BrnGameState::GameStateModuleIO::PrepareForModeAction*) {}
        void HandleStopModeAction(const BrnTrafficIO::InputBuffer_PostPhysics*,
                                  const BrnGameState::GameStateModuleIO::StopModeAction*) {}
        void ClearupCrashedTraffic();
        void KillAllTrafficInCylinder(Vector3 lvCentre, f32 lfRadius, f32 lfHeight, bool lbIncludeStatic);
        void RemoveVehicle(u32 luVehicle);
    };

    // ---- RemoveVehicle recorder ----------------------------------------------------------
    static std::vector<u32> gRemoved;
    static u32 guMutateAfter = 0xFFFFFFFFu, guMutateVictim = 0xFFFFFFFFu;

    void DriveThruFixture::RemoveVehicle(u32 luVehicle)
    {
        gRemoved.push_back(luVehicle);
        // A mid-walk side effect on ANOTHER car (as RemoveVehicle's trailer arms have): the
        // console walks a stack snapshot, so the victim is still visited.
        if (luVehicle == guMutateAfter)
        {
            mVehicleSoaData.mPhysicalVehicles.UnSetBit(guMutateVictim);
            mVehicleSoaData.mAliveVehicles.UnSetBit(guMutateVictim);
        }
    }
}

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // The read-locked accessor (0x827117A8) without the lock bookkeeping.
    const InputBuffer_PostPhysics::GameActionQueueStorage* InputBuffer_PostPhysics::GetGameActionQueue() const
    {
        return &mGameActionQueue;
    }
}
}

// The production bodies under test (or the runner's labelled empty stand-ins).
#include "drive_thru_clearup.inc"

using namespace BrnTraffic;
typedef DriveThruFixture Fixture;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

alignas(64) static unsigned char gaStorage[sizeof(Fixture)];
alignas(64) static unsigned char gaInputStorage[sizeof(BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics)];

static const Vector3 KV_PLAYER = { 100.0f, 5.0f, 200.0f, 0.0f };

static Fixture& Fresh()
{
    std::memset(gaStorage, 0, sizeof(gaStorage));
    Fixture& lr = *reinterpret_cast<Fixture*>(gaStorage);
    lr.mbAllowDivergentBehaviour = true;
    lr.mLocalPlayerPosition = KV_PLAYER;
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        lr.maVehicles[luVehicle].muSpecies = static_cast<u8>(GetVehicleSpecies(luVehicle));
        lr.maVehicles[luVehicle].miPhysicalPartsIndex = -1;
        lr.maVehicleTransforms[luVehicle].SetIdentity();
        lr.maVehicleTransforms[luVehicle].wAxis = { 5000.0f, 5.0f, 5000.0f, 1.0f };   // far away
    }
    gRemoved.clear();
    guMutateAfter = guMutateVictim = 0xFFFFFFFFu;
    return lr;
}

static void Place(Fixture& lr, u32 luVehicle, f32 lfX, f32 lfY, f32 lfZ, bool lbAlive = true)
{
    lr.maVehicleTransforms[luVehicle].wAxis = { lfX, lfY, lfZ, 1.0f };
    if (lbAlive)
    {
        lr.maVehicles[luVehicle].mxFlags |= Vehicle::E_FLAG_ALIVE;
        lr.mVehicleSoaData.mAliveVehicles.SetBit(luVehicle);
    }
}

static void MakePhysical(Fixture& lr, u32 luVehicle, s8 liParts, bool lbFatal, s8 liManoeuvre)
{
    lr.maVehicles[luVehicle].mxFlags |= Vehicle::E_FLAG_PHYSICAL;
    lr.maVehicles[luVehicle].miPhysicalPartsIndex = liParts;
    lr.maVehicles[luVehicle].miManoeuvre = liManoeuvre;
    lr.mVehicleSoaData.mPhysicalVehicles.SetBit(luVehicle);
    lr.maTrafficPhysicsInfoListBits.SetBit(static_cast<u32>(liParts));
    lr.maTrafficPhysicsInfoList[liParts].muOwningVehicleIndex = static_cast<u16>(luVehicle);
    lr.maTrafficPhysicsInfoList[liParts].mbIsFatallyCrashing = lbFatal;
}

// The scene every drive-thru case uses. Player at (100, 5, 200).
static Fixture& Scene()
{
    Fixture& lr = Fresh();
    Place(lr, 1, 100.0f, 5.0f, 600.0f);   MakePhysical(lr, 1, 0, true, 0);   // fatal, 400 m away
    Place(lr, 2, 1000.0f, 5.0f, 1000.0f); MakePhysical(lr, 2, 1, false, 3);  // gave up, far
    Place(lr, 3, 1000.0f, 5.0f, 900.0f);  MakePhysical(lr, 3, 2, false, 0);  // physical, fine, far
    Place(lr, 4, 900.0f, 5.0f, 900.0f);                                       // alive, far
    Place(lr, 5, 349.9f, 5.0f, 200.0f);                                       // d = 249.9
    Place(lr, 6, 350.5f, 5.0f, 200.0f);                                       // d = 250.5
    Place(lr, 7, 350.0f, 5.0f, 200.0f);                                       // d = 250 exactly
    Place(lr, 8, 110.0f, 1004.9f, 200.0f);                                    // dy = 999.9
    Place(lr, 9, 110.0f, 1005.0f, 200.0f);                                    // dy = 1000 exactly
    Place(lr, 10, 100.0f, 505.0f, 200.0f);                                    // straight above, dy 500
    Place(lr, 11, 120.0f, 5.0f, 200.0f, false);                               // dead, inside
    Place(lr, 12, 2000.0f, 5.0f, 2000.0f); MakePhysical(lr, 12, 3, true, 0); // fatal, far
    Place(lr, 13, 2100.0f, 5.0f, 2000.0f); MakePhysical(lr, 13, 4, true, 0); // fatal, far
    Place(lr, 14, 130.0f, 5.0f, 200.0f);   MakePhysical(lr, 14, 5, true, 0); // fatal, INSIDE
    Place(lr, 401, 105.0f, 5.0f, 205.0f);                                     // parked (STATIC), inside
    guMutateAfter  = 12;   // removing 12 knocks 13 out of the live SoA sets mid-walk
    guMutateVictim = 13;
    return lr;
}

static BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics& InputWith(s32 liAction)
{
    std::memset(gaInputStorage, 0, sizeof(gaInputStorage));
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics& lrInput =
        *reinterpret_cast<BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics*>(gaInputStorage);
    lrInput.mGameActionQueue.Construct();
    alignas(16) u8 lauPayload[144] = {};
    lrInput.mGameActionQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lauPayload), liAction, 144);
    return lrInput;
}

static bool RemovedAre(std::initializer_list<u32> lExpected)
{
    return gRemoved == std::vector<u32>(lExpected);
}

static void PrintRemoved(const char* lpcTag)
{
    std::fprintf(stderr, "  %s removed:", lpcTag);
    for (u32 luVehicle : gRemoved) std::fprintf(stderr, " %u", luVehicle);
    std::fprintf(stderr, "\n");
}

int main()
{
    BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* const lpOutput =
        reinterpret_cast<BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics*>(gaInputStorage);   // never read by these arms

    // ---- the four drive-thru actions, offline ----
    const s32 laiActions[] = { BrnGameState::GameStateModuleIO::E_ACTION_BODY_SHOP_DRIVE_THRU,
                               BrnGameState::GameStateModuleIO::E_ACTION_PAINT_SHOP_DRIVE_THRU,
                               BrnGameState::GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD,
                               BrnGameState::GameStateModuleIO::E_ACTION_GAS_STATION_DRIVE_THRU };
    Check(laiActions[0] == 97 && laiActions[1] == 98 && laiActions[2] == 99 && laiActions[3] == 100,
          "the four drive-thru action ids are 97..100 (jump-table cases 84..87 + 13)");
    for (s32 liAction : laiActions)
    {
        Fixture& lr = Scene();
        lr.HandleExternalRequests(&InputWith(liAction), lpOutput);
        char lacName[160];
        std::snprintf(lacName, sizeof(lacName),
                      "action %d offline: clear-up {1 fatal, 2 gave-up, 12, 13 (snapshot), 14} then the "
                      "250 m / 1000 m cylinder {5, 7, 8, 10, 14}  @0x8274BFB8/0x8274BFD8", liAction);
        const bool lbPass = RemovedAre({ 1, 2, 12, 13, 14, 5, 7, 8, 10, 14 });
        Check(lbPass, lacName);
        if (!lbPass) PrintRemoved("got");
    }

    // ---- what the arm must NOT do ----
    {
        Fixture& lr = Scene();
        lr.mbIsOnlineGameMode = true;
        lr.HandleExternalRequests(&InputWith(97), lpOutput);
        Check(gRemoved.empty(), "online: the drive-thru arm removes nothing (lbzx +0x717DC ; bne)");
    }
    {
        Fixture& lr = Scene();
        lr.HandleExternalRequests(&InputWith(96), lpOutput);
        Check(gRemoved.empty(), "action 96 is not a drive-thru");
        Scene();
        lr.HandleExternalRequests(&InputWith(101), lpOutput);
        Check(gRemoved.empty(), "action 101 is not a drive-thru");
    }

    // ---- ClearupCrashedTraffic alone ----
    {
        Fixture& lr = Scene();
        lr.ClearupCrashedTraffic();
        Check(RemovedAre({ 1, 2, 12, 13, 14 }), "ClearupCrashedTraffic: fatal or given-up PHYSICAL cars only, in index order");
        if (!RemovedAre({ 1, 2, 12, 13, 14 })) PrintRemoved("clear-up");
    }
    {
        Fixture& lr = Scene();
        guMutateAfter = 0xFFFFFFFFu;
        lr.maVehicles[3].miManoeuvre = 2;   // 3-point turn: not given up
        lr.maTrafficPhysicsInfoList[1].mbIsFatallyCrashing = false;
        lr.maVehicles[2].miManoeuvre = 4;   // stuck-reverse: not given up
        lr.ClearupCrashedTraffic();
        Check(RemovedAre({ 1, 12, 13, 14 }), "ClearupCrashedTraffic: only E_MANOEUVRE_GIVE_UP (3) counts as given up");
    }

    // ---- KillAllTrafficInCylinder alone ----
    {
        Fixture& lr = Scene();
        lr.KillAllTrafficInCylinder(KV_PLAYER, 250.0f, 1000.0f, false);
        Check(RemovedAre({ 5, 7, 8, 10, 14 }),
              "cylinder: d <= r (250 on the rim is IN), |dy| < h (1000 exactly is OUT), dead and parked skipped");
        if (!RemovedAre({ 5, 7, 8, 10, 14 })) PrintRemoved("cylinder");
    }
    {
        Fixture& lr = Scene();
        lr.KillAllTrafficInCylinder(KV_PLAYER, 250.0f, 1000.0f, true);
        Check(RemovedAre({ 5, 7, 8, 10, 14, 401 }), "cylinder with the bool set: the parked car goes too");
    }
    {
        Fixture& lr = Scene();
        lr.KillAllTrafficInCylinder(KV_PLAYER, 10.5f, 600.0f, false);
        Check(RemovedAre({ 10 }), "cylinder: the Y lane is ignored for the radius (straight above at dy 500 is inside r 10.5)");
    }
    {
        // The parked test reads the vehicle's OWN species byte, not the index range.
        Fixture& lr = Scene();
        lr.maVehicles[5].muSpecies = Vehicle::E_SPECIES_STATIC;
        lr.maVehicles[401].muSpecies = Vehicle::E_SPECIES_STANDARD;
        lr.KillAllTrafficInCylinder(KV_PLAYER, 250.0f, 1000.0f, false);
        Check(RemovedAre({ 7, 8, 10, 14, 401 }), "cylinder: STATIC is the vehicle's species byte (+4 & 0xF), not GetVehicleSpecies(index)");
    }

    Check(gAsserts == 0, "no assert fires");
    std::printf("FxTrafficDriveThruClearup: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
