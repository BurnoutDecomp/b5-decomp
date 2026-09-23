// FX-GS (crash parity 2026-09-23, G11-D1/D2/D3): the PRODUCTION BrnGameState::HUDMessageLogic
// crash-message bodies -- Construct / Prepare / PostWorldUpdate / GenerateRaceModeMessages /
// GeneratePlayerCheckpointMessage / DetectCrashes / DetectOnlineCrashes /
// RemoveCrashingMessagesForTakendownPlayers, extracted from
// src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp by run_fxgs_hud_crashes.py --
// driven through the REAL HUDMessageLogic, RCEntityActiveRaceCarOutputInterface (its accessor
// bodies extracted from BrnRCEntityActiveRaceCarOutputInterface.cpp), RaceCarCrashEvent and
// TakedownEvent queues, and checked against the ARTIST asm:
//   PostWorldUpdate @0x8239D998       jump table 0x8239DA68: 0/10 -> 0x8239DAB0 GenerateRaceModeMessages
//                                     (r4 iface, r5 scoring, r6 crash queue, r7 takedown queue,
//                                     r8 [sp+0x5C] player, f1 delta); 15 -> 0x8239DB40 DetectOnlineCrashes
//                                     (iface, crash queue, f1) then RemoveCrashing...(takedown queue)
//   GenerateRaceModeMessages @0x82399C78  checkpoint record (0x201 gate; 249, 24 bytes:
//                                     ld 0x1F0 / ld 0x1F8 / lbz 0x200) BEFORE the crash split;
//                                     latched mode 0 -> DetectCrashes, else DetectOnlineCrashes +
//                                     RemoveCrashing...; stb 0 0x201 last
//   DetectCrashes @0x82394418         every non-player event -> {maRivalIds[idx], idx} 250 / 16;
//                                     no +0x38 primary-crash test
//   DetectOnlineCrashes @0x82394528   buffer {id, 1.5 (flt_82029F18), idx}; each slot: inactive ->
//                                     free; timer -= step; `bge` vs 0.0 keeps; Showtime (0x0100) ->
//                                     free without a post; else post 250 / 16 then free; a full pool
//                                     asserts "liAllocatedIndex >= 0" (:804) and drops the crash
//   RemoveCrashing... @0x82366590     free every buffered slot whose +0xC index is a victim
//   Construct @0x8236F530 / Prepare @0x82366478  the pool's Clear image (queue 7..0, count 8, bits 0)
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0, gCriticalCalls = 0, gStuntCalls = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [hud-xcrash] witness stays silent
namespace Message { u64 gxMessageFilterFlags = 0; }      // VariableEventQueue::OutputQueueContents
}

// Harness-only: RaceCarState's default ctor calls Clear() (BrnVehicleEvents.cpp, not under test).
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(this, 0, sizeof(*this)); }

// Harness-only: the two PostWorldUpdate arms that are not under test, counted.
void BrnGameState::HUDMessageLogic::GenerateCriticalDamageMessage(
    const StuntModeScoring::ActiveRaceCarOutputInterface*, ScoringSystem*) { ++gCriticalCalls; }
void BrnGameState::HUDMessageLogic::GenerateStuntMessage(ScoringSystem*) { ++gStuntCalls; }

// The production bodies under test.
#include "hud_crashes_methods.inc"

using namespace BrnGameState;
typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface Iface;
typedef VehicleManagerOutputInterface::RaceCarCrashEventQueue CrashQueue;
typedef InputBuffer::TakedownEventQueue TakedownQueue;
typedef GameStateModuleIO::HUDMessageXCrashesAction XCrashes;
typedef GameStateModuleIO::HUDMessagePlayerReachesCheckpointAction Checkpoint;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// Big fixtures live in static storage.
static Iface          gIface;
static CrashQueue     gCrashes;
static TakedownQueue  gTakedowns;
static HUDMessageLogic gHud;
static u8             gScoringStandIn[16];   // passed through only; the stand-ins never read it
static ScoringSystem* const gpScoring = reinterpret_cast<ScoringSystem*>(gScoringStandIn);

static const EActiveRaceCarIndex KE_PLAYER = E_ACTIVE_RACE_CAR_INDEX_2;
static CgsID RivalId(s32 liSlot) { return 0x5100000000000000ull + static_cast<CgsID>(liSlot) * 0x101ull; }

struct Posted
{
    s32 miType;
    s32 miSize;
    u8  maBytes[32];
};

static s32 Drain(Posted* lpOut, s32 liMax)
{
    s32 liCount = 0;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    for (s32 liType = gHud.mActionQueue.GetFirstEvent(&lpEvent, &liSize); liType != -1;
         liType = gHud.mActionQueue.GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liCount < liMax)
        {
            lpOut[liCount].miType = liType;
            lpOut[liCount].miSize = liSize;
            std::memset(lpOut[liCount].maBytes, 0, sizeof(lpOut[liCount].maBytes));
            std::memcpy(lpOut[liCount].maBytes, lpEvent, liSize < 32 ? liSize : 32);
        }
        ++liCount;
    }
    return liCount;
}

static XCrashes AsCrash(const Posted& lr) { XCrashes l; std::memcpy(&l, lr.maBytes, sizeof(l)); return l; }
static Checkpoint AsCheckpoint(const Posted& lr) { Checkpoint l; std::memcpy(&l, lr.maBytes, sizeof(l)); return l; }

static void AddCrash(s32 liSlot, bool lbPrimary = true)
{
    BrnPhysics::Vehicle::RaceCarCrashEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.mRaceCarVolumeInstanceID.muId = static_cast<u64>(0x1000000u | (static_cast<u32>(liSlot) << 10)) << 32;
    lEvent.mbIsPrimaryCrash = lbPrimary;
    gCrashes.AddEvent(lEvent);
}

static void AddTakedown(s32 liAggressor, s32 liVictim)
{
    TakedownEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.meAggressorIndex = static_cast<EActiveRaceCarIndex>(liAggressor);
    lEvent.meVictimIndex    = static_cast<EActiveRaceCarIndex>(liVictim);
    gTakedowns.AddEvent(lEvent);
}

static s32 BufferedCount()
{
    s32 liCount = 0;
    for (s32 li = 0; li < 8; ++li)
    {
        if (gHud.mBufferedCrashingCars.IsObjectAllocated(li))
        {
            ++liCount;
        }
    }
    return liCount;
}

static bool PoolIsClearImage()
{
    if (gHud.mBufferedCrashingCars.miNumObjectsFree != 8)
    {
        return false;
    }
    for (s32 li = 0; li < 8; ++li)
    {
        if (gHud.mBufferedCrashingCars.maiObjectFreeQueue[li] != 7 - li || gHud.mBufferedCrashingCars.IsObjectAllocated(li))
        {
            return false;
        }
    }
    return true;
}

// A fresh frame: HUD object Constructed, the queues empty, every car in use, the player in slot 2.
static void Fresh(GameStateModuleIO::EGameModeType leLatchedMode)
{
    std::memset(&gHud, 0xCD, sizeof(gHud));   // prove Construct seeds the pool, not the harness
    gHud.Construct();
    gHud.meCurrentGameModeType = leLatchedMode;
    gCrashes.Construct();
    gTakedowns.Construct();
    gIface.mePlayerActiveRaceCarIndex = KE_PLAYER;
    for (s32 li = 0; li < 8; ++li)
    {
        gIface.maRivalIds[li]      = RivalId(li);
        gIface.maxRaceCarFlags[li] = BrnWorld::RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_IN_USE;
    }
    gAsserts = 0;
    gCriticalCalls = 0;
    gStuntCalls = 0;
}

static void PostWorld(GameStateModuleIO::EGameModeType leMode, f32 lfDelta)
{
    gHud.PostWorldUpdate(&gIface, leMode, gpScoring, &gCrashes, &gTakedowns, lfDelta, KE_PLAYER);
}

int main()
{
    Posted laPosted[16];

    // ---- Construct / Prepare: the pool's Clear image ----------------------------------------
    std::memset(&gHud, 0xCD, sizeof(gHud));
    gHud.Construct();
    Check(PoolIsClearImage(), "Construct: pool free queue 7..0, count 8, no bits  @0x8236F574..0x8236F5D4");
    Check(gHud.meCurrentGameModeType == GameStateModuleIO::E_MODE_NONE && gHud.mActionQueue.GetLength() == 0,
          "Construct: nothing posted, mode latched E_MODE_NONE  @0x8236F5D8");
    gHud.mBufferedCrashingCars.AllocateObject();
    gHud.mBufferedCrashingCars.AllocateObject();
    gHud.Prepare();
    Check(PoolIsClearImage(), "Prepare: the pool's Clear image again (0x82366528..0x82366578)");

    // ---- DetectCrashes (offline race, through PostWorldUpdate case 0) -------------------------
    Fresh(GameStateModuleIO::E_MODE_OFFLINE_RACE);
    AddCrash(4);
    AddCrash(KE_PLAYER);
    AddCrash(6, false);                     // not a primary crash: still reported (+0x38 never read)
    PostWorld(GameStateModuleIO::E_MODE_OFFLINE_RACE, 0.5f);
    s32 liCount = Drain(laPosted, 16);
    Check(liCount == 2, "D1 mode 0: two records for the two non-player crashes (the player's is skipped)  @0x823944A4");
    Check(liCount >= 1 && laPosted[0].miType == 250 && laPosted[0].miSize == 16,
          "D1 record type 250 (0xFA), size 16 (0x10)  @0x823944F8/0x823944F0");
    Check(liCount >= 1 && AsCrash(laPosted[0]).mRivalID == RivalId(4) && AsCrash(laPosted[0]).meRivalRaceCarIndex == 4,
          "D1 first record {maRivalIds[4], 4}  @0x82394504/0x823944EC");
    Check(liCount >= 2 && AsCrash(laPosted[1]).mRivalID == RivalId(6) && AsCrash(laPosted[1]).meRivalRaceCarIndex == 6,
          "D1 a non-primary crash is reported too: {maRivalIds[6], 6}");
    Check(BufferedCount() == 0, "D1 mode 0 buffers nothing (DetectOnlineCrashes is the mode!=0 arm)");
    Check(gAsserts == 0, "D1 no assert on the offline path");

    Fresh(GameStateModuleIO::E_MODE_OFFLINE_RACE);
    PostWorld(GameStateModuleIO::E_MODE_OFFLINE_RACE, 0.5f);
    Check(Drain(laPosted, 16) == 0, "D1 an empty crash queue posts nothing");

    // ---- the checkpoint record and the latch -------------------------------------------------
    Fresh(GameStateModuleIO::E_MODE_OFFLINE_RACE);
    gHud.mbPlayerHasJustTriggeredCheckpoint = true;
    gHud.mCurrentPlayerCheckpointID = 0x1111222233334444ull;
    gHud.mNextPlayerCheckpointID    = 0x5555666677778888ull;
    gHud.mbIsLastCheckpoint         = true;
    AddCrash(5);
    PostWorld(GameStateModuleIO::E_MODE_OFFLINE_RACE, 0.5f);
    liCount = Drain(laPosted, 16);
    Check(liCount == 2 && laPosted[0].miType == 249 && laPosted[0].miSize == 24,
          "D1 the checkpoint record goes first: type 249 (0xF9), size 24 (0x18)  @0x82399CC8/0x82399CC4");
    Check(liCount >= 1 && AsCheckpoint(laPosted[0]).mThisLandmarkID == 0x1111222233334444ull &&
          AsCheckpoint(laPosted[0]).mNextLandmarkID == 0x5555666677778888ull &&
          AsCheckpoint(laPosted[0]).mbIsPenultimatedLandmark,
          "D1 checkpoint record {ld 0x1F0, ld 0x1F8, lbz 0x200}  @0x82399CC0..0x82399CE4");
    Check(liCount == 2 && laPosted[1].miType == 250 && AsCrash(laPosted[1]).meRivalRaceCarIndex == 5,
          "D1 ...then the crash record (DetectCrashes follows the checkpoint post)");
    Check(!gHud.mbPlayerHasJustTriggeredCheckpoint, "D1 the latch is dropped last  @0x82399D5C `stb 0, 0x201`");

    Fresh(GameStateModuleIO::E_MODE_OFFLINE_RACE);
    gHud.mbPlayerHasJustTriggeredCheckpoint = false;
    AddCrash(5);
    PostWorld(GameStateModuleIO::E_MODE_OFFLINE_RACE, 0.5f);
    liCount = Drain(laPosted, 16);
    Check(liCount == 1 && laPosted[0].miType == 250, "D1 no checkpoint record while the latch is clear  @0x82399CBC `beq`");

    // ---- DetectOnlineCrashes (online race, mode 10) -----------------------------------------
    Fresh(GameStateModuleIO::E_MODE_ONLINE_RACE);
    AddCrash(3);
    AddCrash(KE_PLAYER);
    AddCrash(5);
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.5f);
    Check(Drain(laPosted, 16) == 0, "D2 mode 10: nothing posted on the crash frame (buffered 1.5 s)");
    Check(BufferedCount() == 2, "D2 two non-player crashes buffered, the player's is not  @0x823945D4");
    {
        bool lbSeen3 = false, lbSeen5 = false, lbTimers = true;
        for (s32 li = 0; li < 8; ++li)
        {
            if (!gHud.mBufferedCrashingCars.IsObjectAllocated(li))
            {
                continue;
            }
            const HUDMessageLogic::BufferedCrashingCar& lr = gHud.mBufferedCrashingCars[li];
            lbSeen3 = lbSeen3 || (lr.meActiveRaceCarIndex == 3 && lr.mRivalID == RivalId(3));
            lbSeen5 = lbSeen5 || (lr.meActiveRaceCarIndex == 5 && lr.mRivalID == RivalId(5));
            lbTimers = lbTimers && (lr.mfTimeUntilUnbuffered == 1.5f - 0.5f);
        }
        Check(lbSeen3 && lbSeen5, "D2 buffered {maRivalIds[idx] +0, idx +0xC}  @0x82394670/0x82394684");
        Check(lbTimers, "D2 timer seeded 1.5 (flt_82029F18) and aged by the same frame's second loop  @0x82394690/0x8239474C");
    }
    gCrashes.Construct();
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.5f);   // 0.5 left
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.5f);   // exactly 0.0: `bge` keeps it
    Check(Drain(laPosted, 16) == 0 && BufferedCount() == 2, "D2 a timer of exactly 0.0 is kept (fcmpu/bge vs flt_82001CC0)");
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.5f);   // -0.5: post and free
    liCount = Drain(laPosted, 16);
    Check(liCount == 2 && laPosted[0].miType == 250 && laPosted[0].miSize == 16 && laPosted[1].miType == 250,
          "D2 both posted as 250 / 16 once the timer goes negative  @0x8239480C");
    Check(liCount == 2 && ((AsCrash(laPosted[0]).meRivalRaceCarIndex == 3 && AsCrash(laPosted[0]).mRivalID == RivalId(3)) ||
                           (AsCrash(laPosted[0]).meRivalRaceCarIndex == 5 && AsCrash(laPosted[0]).mRivalID == RivalId(5))),
          "D2 the posted record is the buffered {id, idx}");
    Check(BufferedCount() == 0, "D2 posted slots are freed  @0x82394818");

    // Showtime and inactive cars.
    Fresh(GameStateModuleIO::E_MODE_ONLINE_RACE);
    AddCrash(4);
    AddCrash(6);
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 1.0f);
    gCrashes.Construct();
    gIface.maxRaceCarFlags[4] = BrnWorld::RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_IN_USE |
                                BrnWorld::RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_IN_SHOWTIME;
    gIface.maxRaceCarFlags[6] = 0;   // left the race: IsRaceCarActive false
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.1f);    // car 6 freed now, car 4 still 0.4 s to go
    Check(BufferedCount() == 1, "D2 an inactive car's buffered crash is freed without a post, before its timer runs out  @0x82394734");
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 1.0f);
    Check(Drain(laPosted, 16) == 0 && BufferedCount() == 0,
          "D2 a car in Showtime (0x0100) expires silently and is freed  @0x823947C8");

    // A full pool.
    Fresh(GameStateModuleIO::E_MODE_ONLINE_RACE);
    for (s32 li = 0; li < 8; ++li)
    {
        gHud.mBufferedCrashingCars.AllocateObject();
        gHud.mBufferedCrashingCars[li].meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(li);
        gHud.mBufferedCrashingCars[li].mfTimeUntilUnbuffered = 10.0f;
    }
    AddCrash(3);
    gAsserts = 0;
    gHud.DetectOnlineCrashes(&gIface, &gCrashes, 0.0f);
    Check(gAsserts == 1 && BufferedCount() == 8, "D2 a full pool asserts once (\"liAllocatedIndex >= 0\", :804) and drops the crash");

    // ---- RemoveCrashingMessagesForTakendownPlayers --------------------------------------------
    Fresh(GameStateModuleIO::E_MODE_ONLINE_RACE);
    AddCrash(3);
    AddCrash(5);
    AddCrash(5);
    gHud.DetectOnlineCrashes(&gIface, &gCrashes, 0.0f);
    AddTakedown(3, 5);                      // aggressor 3 (buffered too), victim 5
    gHud.RemoveCrashingMessagesForTakendownPlayers(&gTakedowns);
    Check(BufferedCount() == 1, "D3 both buffered crashes of the victim (5) are freed, car 3's stays  @0x82366600");
    {
        bool lbKept3 = false;
        for (s32 li = 0; li < 8; ++li)
        {
            lbKept3 = lbKept3 || (gHud.mBufferedCrashingCars.IsObjectAllocated(li) &&
                                  gHud.mBufferedCrashingCars[li].meActiveRaceCarIndex == 3);
        }
        Check(lbKept3, "D3 the victim test reads TakedownEvent +4 (meVictimIndex), not the aggressor  @0x823665C0");
    }

    // Mode 10 runs the removal after DetectOnlineCrashes in the same frame.
    Fresh(GameStateModuleIO::E_MODE_ONLINE_RACE);
    AddCrash(3);
    AddCrash(4);
    AddTakedown(KE_PLAYER, 4);
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.0f);
    Check(BufferedCount() == 1, "D3 mode 10: a car taken down this frame is never buffered past it  @0x82399D2C");

    // ---- PostWorldUpdate case 15 (the free-burn lobby) ----------------------------------------
    Fresh(GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY);
    AddCrash(1);
    AddCrash(7);
    AddTakedown(KE_PLAYER, 7);
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY, 0.25f);
    Check(BufferedCount() == 1 && Drain(laPosted, 16) == 0,
          "D2/D3 case 15: DetectOnlineCrashes then RemoveCrashing... (car 1 buffered, victim 7 dropped)  @0x8239DB50/0x8239DB5C");

    // ---- the other arms are untouched --------------------------------------------------------
    Fresh(GameStateModuleIO::E_MODE_ROAD_RAGE);
    AddCrash(4);
    PostWorld(GameStateModuleIO::E_MODE_ROAD_RAGE, 0.5f);
    Check(gCriticalCalls == 1 && Drain(laPosted, 16) == 0 && BufferedCount() == 0,
          "case 3 still runs GenerateCriticalDamageMessage only (no crash message)");
    Fresh(GameStateModuleIO::E_MODE_STUNT_ATTACK);
    AddCrash(4);
    PostWorld(GameStateModuleIO::E_MODE_STUNT_ATTACK, 0.5f);
    Check(gStuntCalls == 1 && Drain(laPosted, 16) == 0, "case 7 still runs GenerateStuntMessage only");

    // A mode change re-runs Prepare (the latched member switches): buffered crashes are dropped.
    Fresh(GameStateModuleIO::E_MODE_ONLINE_RACE);
    AddCrash(3);
    PostWorld(GameStateModuleIO::E_MODE_ONLINE_RACE, 0.0f);
    gCrashes.Construct();
    PostWorld(GameStateModuleIO::E_MODE_OFFLINE_RACE, 0.0f);
    Check(BufferedCount() == 0 && gHud.meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_RACE,
          "a mode change runs Prepare first (pool cleared) and latches the new mode  @0x8239DA2C/0x8239DA30");

    std::printf("FxGsHudCrashes: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
