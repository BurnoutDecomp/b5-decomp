// FX-FLOW (crash parity 2026-09-24, G11-D1 remainder, producer half): the PRODUCTION race-arm generators
// of BrnGameState::HUDMessageLogic -- GenerateLeaderMessages / GenerateFinisherMessage /
// GenerateRivalCheckpointMessage / GenerateFirstOrLastMessage / GenerateDistanceToFinishMessage, with
// Construct / Prepare / GenerateRaceModeMessages and the crash legs they sit between -- extracted from
// BrnHUDMessageLogic.cpp by run_fxflow_race_hud_messages.py and driven through the REAL HUDMessageLogic
// and RCEntityActiveRaceCarOutputInterface (its accessor bodies extracted from their TU). The two
// out-of-line ScoringSystem queries are scripted here; everything else the generators read off the
// scorer is its real inline accessors over its real members.
//
// ARTIST:
//   GenerateLeaderMessages @0x82394110   checkpoint frame, offline, not crashing, active, no finisher,
//       all cars ready -> (second - leader distance) / (0.44704 * 160) in [1, 1000] s -> 245 / 24
//       {GetRivalId(slot-0 index), split, slot-0 index @+0xC, player-leads @+0x10}
//   GenerateFinisherMessage @0x82394258  rival latch -> 247 / 8 {slot, place}, latch dropped; the
//       player's own latch neither posted nor dropped
//   GenerateRivalCheckpointMessage @0x82394338  latch, no finisher -> latch dropped; rival >= 500 m
//       nearer the finish than the player -> 248 / 24 {GetRivalId, landmark id, slot}
//   GenerateFirstOrLastMessage @0x82395760  new leader held 1.5 s -> 242 / 16; new last held 7.5 s ->
//       243 / 16; the lead half (clock included) skipped once any car finished
//   GenerateDistanceToFinishMessage @0x82395A88  first mark = (d - 100) - Modulo(d - 100, 500); below
//       the mark -> 244 / 8 {mark, place}, mark -= 500; 0.0 = spent; FLT_MAX distance = none
//   Prepare @0x82366478  the two clocks at 1.5 s / 7.5 s, the next mark at -1.0
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// Harness-only: RaceCarState's default ctor calls Clear() (BrnVehicleEvents.cpp, not under test).
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(this, 0, sizeof(*this)); }

// Harness-only: the PostWorldUpdate arms not under test.
void BrnGameState::HUDMessageLogic::GenerateCriticalDamageMessage(
    const StuntModeScoring::ActiveRaceCarOutputInterface*, ScoringSystem*) {}
void BrnGameState::HUDMessageLogic::GenerateStuntMessage(ScoringSystem*) {}

// Harness-only: the two out-of-line scorer queries, scripted per slot.
static u32 gauPosition[8];
static f32 gafDistance[8];
u32 BrnGameState::ScoringSystem::GetCarRacePosition(EActiveRaceCarIndex le) const { return gauPosition[le]; }
f32 BrnGameState::ScoringSystem::GetRaceCarDistanceToFinish(EActiveRaceCarIndex le) const { return gafDistance[le]; }

// The production bodies under test.
#include "race_hud_methods.inc"

using namespace BrnGameState;
typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface Iface;
typedef VehicleManagerOutputInterface::RaceCarCrashEventQueue CrashQueue;
typedef InputBuffer::TakedownEventQueue TakedownQueue;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static Iface           gIface;
static CrashQueue      gCrashes;
static TakedownQueue   gTakedowns;
static HUDMessageLogic gHud;
alignas(16) static u8  gScoringStorage[sizeof(ScoringSystem)];
static ScoringSystem* const gpScoring = reinterpret_cast<ScoringSystem*>(gScoringStorage);

static const EActiveRaceCarIndex KE_PLAYER = E_ACTIVE_RACE_CAR_INDEX_2;
static CgsID RivalId(s32 liSlot) { return 0x5100000000000000ull + static_cast<CgsID>(liSlot) * 0x101ull; }

struct Posted
{
    s32 miType;
    s32 miSize;
    alignas(8) u8 maBytes[32];
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
    gHud.mActionQueue.Clear();
    return liCount;
}

static u64 Get64(const Posted& lr, s32 li) { u64 lu; std::memcpy(&lu, lr.maBytes + li, 8); return lu; }
static s32 Get32(const Posted& lr, s32 li) { s32 l; std::memcpy(&l, lr.maBytes + li, 4); return l; }
static f32 GetF(const Posted& lr, s32 li) { f32 lf; std::memcpy(&lf, lr.maBytes + li, 4); return lf; }

// A fresh offline race: 4 cars in slots 0..3, the player in slot 2, all active and ready.
static void Reset()
{
    std::memset(gScoringStorage, 0, sizeof(gScoringStorage));
    gpScoring->mbACarHasFinishedTheRace = false;
    gpScoring->mbNewLeader    = false;
    gpScoring->mbNewLastPlace = false;
    gpScoring->meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    gpScoring->meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    for (s32 li = 0; li < 8; ++li)
    {
        gauPosition[li] = static_cast<u32>(li + 1);
        gafDistance[li] = 3000.0f;
        gpScoring->maRaceCarPositioningData[li].meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(li);
        gpScoring->maRaceCarPositioningData[li].mfDistanceToFinish = 3000.0f;
        gIface.maRivalIds[li] = RivalId(li);
        gIface.maRaceCarStates[li].mbCrashing = false;
    }
    gIface.mePlayerActiveRaceCarIndex = KE_PLAYER;
    gIface.mbIsPlayerCarActive  = true;
    gIface.SetAllActiveCarsReady(true);

    gHud.Construct();
    gHud.meCurrentGameModeType = GameStateModuleIO::E_MODE_OFFLINE_RACE;
    gCrashes.Construct();
    gTakedowns.Construct();
    Posted laDiscard[16];
    Drain(laDiscard, 16);
}

static s32 RunRaceArm(f32 lfStep, Posted* lpOut)
{
    gHud.GenerateRaceModeMessages(&gIface, gpScoring, &gCrashes, &gTakedowns, KE_PLAYER, lfStep);
    return Drain(lpOut, 16);
}

int main()
{
    Posted laPosted[16];

    // ---- Prepare's seeds (0x82366518 / 0x82366524 / 0x82366530) ----------------------------------
    Reset();
    Check(gHud.mTimeSinceNewLeader.GetFloatVal() == 1.5f && gHud.mTimeSinceNewLast.GetFloatVal() == 7.5f,
          "Prepare seeds the two clocks at flt_82029F18 (1.5 s) / flt_82029F1C (7.5 s)");
    Check(gHud.mfNextDistanceToFinishMessage == -1.0f, "Prepare seeds the next mark at flt_820037C8 (-1.0)");

    // ---- 245, the leader split (0x82394110) --------------------------------------------------------
    {
        const f32 lfSpeed = 0.44704f * 160.0f;
        u32 luBits;
        std::memcpy(&luBits, &lfSpeed, 4);
        Check(luBits == 0x428F0D84u, "the split speed is flt_82F31928 * flt_82020A70 == 0x428F0D84 (71.5264 m/s)");

        Reset();
        gauPosition[KE_PLAYER] = 2;
        gpScoring->maRaceCarPositioningData[0].meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
        gpScoring->maRaceCarPositioningData[0].mfDistanceToFinish = 1000.0f;
        gafDistance[KE_PLAYER] = 1000.0f + lfSpeed * 3.0f;
        gHud.mbPlayerHasJustTriggeredCheckpoint = true;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        s32 liCount = Drain(laPosted, 16);
        const f32 lfExpected = (gafDistance[KE_PLAYER] - 1000.0f) / lfSpeed;
        Check(liCount == 1 && laPosted[0].miType == 245 && laPosted[0].miSize == 24,
              "a checkpoint frame 3 s behind the leader posts ONE 245 of 24 bytes (@0x82394248)");
        Check(liCount == 1 && Get64(laPosted[0], 0) == RivalId(1) && GetF(laPosted[0], 8) == lfExpected
                  && Get32(laPosted[0], 0xC) == 1 && laPosted[0].maBytes[0x10] == 0,
              "245 = {GetRivalId(slot-0 index), split, slot-0 index @+0xC, leads 0 @+0x10}");

        gHud.mbPlayerHasJustTriggeredCheckpoint = false;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        Check(Drain(laPosted, 16) == 0, "no checkpoint this frame -> nothing (`lbz 0x201 ; beq`)");

        gHud.mbPlayerHasJustTriggeredCheckpoint = true;
        gHud.meCurrentGameModeType = GameStateModuleIO::E_MODE_ONLINE_RACE;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liOnline = Drain(laPosted, 16);
        gHud.meCurrentGameModeType = GameStateModuleIO::E_MODE_OFFLINE_RACE;
        gIface.maRaceCarStates[KE_PLAYER].mbCrashing = true;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liCrashing = Drain(laPosted, 16);
        gIface.maRaceCarStates[KE_PLAYER].mbCrashing = false;
        gIface.SetAllActiveCarsReady(false);
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liNotReady = Drain(laPosted, 16);
        gIface.SetAllActiveCarsReady(true);
        gpScoring->mbACarHasFinishedTheRace = true;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liFinished = Drain(laPosted, 16);
        gpScoring->mbACarHasFinishedTheRace = false;
        Check(liOnline == 0 && liCrashing == 0 && liNotReady == 0 && liFinished == 0,
              "online mode (10..17), the player crashing (+0x44A), cars not ready (+0x2861) or a car finished "
              "(+0x4EFA) -> nothing");

        gafDistance[KE_PLAYER] = 1000.0f + lfSpeed * 0.75f;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liUnderOne = Drain(laPosted, 16);
        gafDistance[KE_PLAYER] = 1000.0f + lfSpeed * 1001.0f;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liOverMax = Drain(laPosted, 16);
        gafDistance[KE_PLAYER] = 900.0f;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        const s32 liAhead = Drain(laPosted, 16);
        Check(liUnderOne == 0 && liOverMax == 0 && liAhead == 0,
              "a split under 1 s or over 1000 s, or a 'second' distance below the leader's -> nothing");

        gauPosition[KE_PLAYER] = 1;
        gpScoring->maRaceCarPositioningData[0].meActiveRaceCarIndex = KE_PLAYER;
        gpScoring->maRaceCarPositioningData[1].mfDistanceToFinish = 1000.0f + lfSpeed * 2.0f;
        gHud.GenerateLeaderMessages(&gIface, gpScoring);
        liCount = Drain(laPosted, 16);
        Check(liCount == 1 && Get32(laPosted[0], 0xC) == KE_PLAYER && laPosted[0].maBytes[0x10] == 1
                  && std::fabs(GetF(laPosted[0], 8) - 2.0f) < 1.0e-3f,
              "the leading player: second place's distance (lfs 0x59E0), leads 1, the index is slot 0's (the player)");
    }

    // ---- 247, a finisher (0x82394258) ----------------------------------------------------------------
    {
        Reset();
        gHud.meFinishedRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        gHud.miFinishPosition = 2;
        gHud.GenerateFinisherMessage(&gIface);
        const s32 liCount = Drain(laPosted, 16);
        Check(liCount == 1 && laPosted[0].miType == 247 && laPosted[0].miSize == 8 && Get32(laPosted[0], 0) == 3
                  && Get32(laPosted[0], 4) == 2 && gHud.meFinishedRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
              "a rival's finish posts ONE 247 {slot, place} of 8 bytes and drops the latch (@0x8239432C)");
        gHud.meFinishedRaceCarIndex = KE_PLAYER;
        gHud.GenerateFinisherMessage(&gIface);
        Check(Drain(laPosted, 16) == 0 && gHud.meFinishedRaceCarIndex == KE_PLAYER,
              "the player's own finish: nothing posted and the latch KEPT (`beq` @0x82394304 skips the store)");
    }

    // ---- 248, a rival's checkpoint (0x82394338) ------------------------------------------------------
    {
        Reset();
        gHud.meCheckpointTriggeringRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        gHud.mRivalCheckpointID = 0x7777000011112222ull;
        gafDistance[0] = 1000.0f;
        gafDistance[KE_PLAYER] = 1600.0f;
        gHud.GenerateRivalCheckpointMessage(&gIface, gpScoring);
        s32 liCount = Drain(laPosted, 16);
        Check(liCount == 1 && laPosted[0].miType == 248 && laPosted[0].miSize == 24
                  && Get64(laPosted[0], 0) == RivalId(0) && Get64(laPosted[0], 8) == 0x7777000011112222ull
                  && Get32(laPosted[0], 0x10) == 0 && gHud.meCheckpointTriggeringRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
              "a rival 600 m ahead posts ONE 248 {GetRivalId, landmark id, slot} of 24 bytes; latch dropped");
        gHud.meCheckpointTriggeringRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        gafDistance[KE_PLAYER] = 1400.0f;
        gHud.GenerateRivalCheckpointMessage(&gIface, gpScoring);
        Check(Drain(laPosted, 16) == 0 && gHud.meCheckpointTriggeringRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
              "a rival only 400 m ahead: nothing, but the latch is still dropped (the store precedes the test)");
        gHud.meCheckpointTriggeringRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        gafDistance[KE_PLAYER] = 1600.0f;
        gpScoring->mbACarHasFinishedTheRace = true;
        gHud.GenerateRivalCheckpointMessage(&gIface, gpScoring);
        Check(Drain(laPosted, 16) == 0 && gHud.meCheckpointTriggeringRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_0,
              "once a car has finished: nothing, and the latch is KEPT (the 0x4EFA exit precedes the store)");
    }

    // ---- 242 / 243, took the lead / took last (0x82395760) ----------------------------------------------
    {
        Reset();
        gpScoring->mbNewLeader = true;
        gpScoring->meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
        s32 liLeadPosts = 0;
        s32 liFrameOfPost = -1;
        for (s32 liFrame = 0; liFrame < 6; ++liFrame)
        {
            gHud.GenerateFirstOrLastMessage(gpScoring, 0.5f, KE_PLAYER, &gIface);
            gpScoring->mbNewLeader = false;   // the change flag holds for one UpdateRacePositions
            const s32 liCount = Drain(laPosted, 16);
            for (s32 li = 0; li < liCount; ++li)
            {
                if (laPosted[li].miType == 242)
                {
                    ++liLeadPosts;
                    liFrameOfPost = liFrame;
                    Check(laPosted[li].miSize == 16 && Get64(laPosted[li], 0) == RivalId(1) && Get32(laPosted[li], 8) == 1,
                          "242 = {GetRivalId(lead), lead} of 16 bytes");
                }
            }
        }
        Check(liLeadPosts == 1 && liFrameOfPost == 2,
              "a new leader is announced ONCE, on the frame its clock crosses 1.5 s (0 -> 0.5 -> 1.0 -> +0.5)");

        Reset();
        gpScoring->mbNewLeader = true;
        gpScoring->meLeadRaceCarIndex = KE_PLAYER;
        s32 liAny = 0;
        for (s32 liFrame = 0; liFrame < 6; ++liFrame)
        {
            gHud.GenerateFirstOrLastMessage(gpScoring, 0.5f, KE_PLAYER, &gIface);
            gpScoring->mbNewLeader = false;
            liAny += Drain(laPosted, 16);
        }
        Check(liAny == 0, "the player taking the lead is not announced");

        Reset();
        gpScoring->mbNewLastPlace = true;
        gpScoring->meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        s32 liLastPosts = 0;
        s32 liLastFrame = -1;
        for (s32 liFrame = 0; liFrame < 20; ++liFrame)
        {
            gHud.GenerateFirstOrLastMessage(gpScoring, 0.5f, KE_PLAYER, &gIface);
            gpScoring->mbNewLastPlace = false;
            const s32 liCount = Drain(laPosted, 16);
            for (s32 li = 0; li < liCount; ++li)
            {
                if (laPosted[li].miType == 243 && laPosted[li].miSize == 16 && Get32(laPosted[li], 8) == 3)
                {
                    ++liLastPosts;
                    liLastFrame = liFrame;
                }
            }
        }
        Check(liLastPosts == 1 && liLastFrame == 14, "a new last place is announced ONCE, after 7.5 s (frame 14 at 0.5 s)");

        Reset();
        gpScoring->mbACarHasFinishedTheRace = true;
        gpScoring->mbNewLeader = true;
        gpScoring->meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
        gpScoring->mbNewLastPlace = true;
        gpScoring->meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        gHud.GenerateFirstOrLastMessage(gpScoring, 0.5f, KE_PLAYER, &gIface);
        Drain(laPosted, 16);
        Check(gHud.mTimeSinceNewLeader.GetFloatVal() == 1.5f && gHud.mTimeSinceNewLast.GetFloatVal() == 0.5f,
              "once a car finished: the lead half is skipped, clock included; the last half still runs");
    }

    // ---- 244, the distance-to-finish marks (0x82395A88) ------------------------------------------------
    {
        Reset();
        gafDistance[KE_PLAYER] = 1050.0f;
        gHud.GenerateDistanceToFinishMessage(gpScoring, KE_PLAYER);
        Check(Drain(laPosted, 16) == 0 && gHud.mfNextDistanceToFinishMessage == 500.0f,
              "the first frame seeds the mark: (1050 - 100) - Modulo(950, 500) == 500, nothing posted");
        gafDistance[KE_PLAYER] = 3.4028234663852886e+38f;
        gHud.GenerateDistanceToFinishMessage(gpScoring, KE_PLAYER);
        const s32 liNoDistance = Drain(laPosted, 16);
        gafDistance[KE_PLAYER] = 499.0f;
        gauPosition[KE_PLAYER] = 3;
        gHud.GenerateDistanceToFinishMessage(gpScoring, KE_PLAYER);
        s32 liCount = Drain(laPosted, 16);
        Check(liNoDistance == 0 && liCount == 1 && laPosted[0].miType == 244 && laPosted[0].miSize == 8
                  && GetF(laPosted[0], 0) == 500.0f && Get32(laPosted[0], 4) == 3
                  && gHud.mfNextDistanceToFinishMessage == 0.0f,
              "FLT_MAX distance -> nothing; passing the mark posts ONE 244 {mark, place} of 8 bytes, mark -= 500");
        gafDistance[KE_PLAYER] = 10.0f;
        gHud.GenerateDistanceToFinishMessage(gpScoring, KE_PLAYER);
        Check(Drain(laPosted, 16) == 0 && gHud.mfNextDistanceToFinishMessage == 0.0f,
              "at 0.0 every mark is spent (`fcmpu 0x250, 0.0 ; beq`)");
    }

    // ---- GenerateRaceModeMessages order (0x82399CA4..0x82399D54) --------------------------------------
    {
        Reset();
        const f32 lfSpeed = 0.44704f * 160.0f;
        gauPosition[KE_PLAYER] = 2;
        gpScoring->maRaceCarPositioningData[0].meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
        gpScoring->maRaceCarPositioningData[0].mfDistanceToFinish = 1000.0f;
        gafDistance[KE_PLAYER] = 1000.0f + lfSpeed * 3.0f;
        gafDistance[0] = 500.0f;
        gHud.mbPlayerHasJustTriggeredCheckpoint = true;
        gHud.meFinishedRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        gHud.miFinishPosition = 1;
        gHud.meCheckpointTriggeringRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        const s32 liCount = RunRaceArm(0.5f, laPosted);
        const bool lbOrder = liCount >= 4 && laPosted[0].miType == 245 && laPosted[1].miType == 247
                             && laPosted[2].miType == 249 && laPosted[3].miType == 248;
        Check(lbOrder, "GenerateRaceModeMessages posts 245, 247, 249, 248 in the console's call order");
        Check(!gHud.mbPlayerHasJustTriggeredCheckpoint, "... and drops the checkpoint latch last (`stb 0, 0x201`)");
    }

    Check(gAsserts == 0, "no assert on the way");

    std::printf("FxFlowRaceHudMessages: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
