// FX-GS follow-on (source-only fixture): the three production Road Rage HUD producers.
// The runner extracts their bodies from BrnHUDMessageLogic.cpp and rehomes them onto the
// small HUDFixture and ScoringFixture below, following run_fxgs_online_team.py's pattern.
// ARTIST witnesses: GenerateOnlineBlueTeamEscapingMessage @0x82395218,
// GenerateOnlineBlueTeamAreBehindYouMessage @0x823953B0, and
// GenerateOnlineLeaderMilestoneMessage @0x82395548. No test in this file is runtime evidence.
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { StrStreamBase* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnGameState
{
// Harness-only constructor for the real inline-accessor score record used by the scoring double.
GameStateModuleIO::CarScoreData::CarScoreData() { std::memset(this, 0, sizeof(*this)); }

struct CarFixture
{
    GameStateModuleIO::CarScoreData mScore;
    GameStateModuleIO::EPlayerTeam  meTeam;
    f32                             mfDistanceToFinish;
    bool                            mbPresent;

    GameStateModuleIO::CarScoreData*       GetScoreData()       { return &mScore; }
    const GameStateModuleIO::CarScoreData* GetScoreData() const { return &mScore; }
};

// Scoring double follows the existing FxGsOnlineTeam runner pattern and records team-query count
// so the ARTIST short-circuit (second GetPlayerTeam call only when the first result is team 1)
// is visible to an assertion.
struct ScoringFixture
{
    CarFixture maCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    u32     muActiveCars;
    GameStateModuleIO::EPlayerTeam maeTeamReplies[E_ACTIVE_RACE_CAR_INDEX_COUNT][2];
    s32     maiTeamReplyCount[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    s32     maiTeamQueries[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    CarFixture* GetCarData(EActiveRaceCarIndex leIndex)
    {
        if (leIndex < E_ACTIVE_RACE_CAR_INDEX_0 || leIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
            return nullptr;
        return maCars[leIndex].mbPresent ? &maCars[leIndex] : nullptr;
    }
    const CarFixture* GetCarData(EActiveRaceCarIndex leIndex) const
    {
        if (leIndex < E_ACTIVE_RACE_CAR_INDEX_0 || leIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
            return nullptr;
        return maCars[leIndex].mbPresent ? &maCars[leIndex] : nullptr;
    }
    u32 GetNumberOfActiveCars() const { return muActiveCars; }
    f32 GetRaceCarDistanceToFinish(EActiveRaceCarIndex leIndex) const
    {
        return maCars[leIndex].mfDistanceToFinish;
    }
    GameStateModuleIO::EPlayerTeam GetPlayerTeam(EActiveRaceCarIndex leIndex)
    {
        const s32 liQuery = maiTeamQueries[leIndex]++;
        if (maiTeamReplyCount[leIndex] > 0)
        {
            const s32 liReply = liQuery < maiTeamReplyCount[leIndex]
                ? liQuery : maiTeamReplyCount[leIndex] - 1;
            return maeTeamReplies[leIndex][liReply];
        }
        return maCars[leIndex].meTeam;
    }
    void SetTeamReplies(EActiveRaceCarIndex leIndex,
                        GameStateModuleIO::EPlayerTeam leFirst,
                        GameStateModuleIO::EPlayerTeam leSecond,
                        s32 liCount)
    {
        maiTeamReplyCount[leIndex] = liCount;
        maeTeamReplies[leIndex][0] = leFirst;
        maeTeamReplies[leIndex][1] = leSecond;
    }
};

struct PostedAction
{
    s32 miType;
    s32 miSize;
    alignas(16) u8 maBytes[16];
};

// The production bodies call the event queue's typed AddEvent overload. This recorder offers
// both that two-argument form and the size-explicit form used in earlier snapshots.
struct ActionRecorder
{
    PostedAction maPosted[16];
    s32 miCount;

    void Reset()
    {
        miCount = 0;
        std::memset(maPosted, 0, sizeof(maPosted));
    }
    void AddEvent(const void* lpEvent, s32 liType, s32 liSize)
    {
        if (miCount < 16)
        {
            PostedAction& lrPosted = maPosted[miCount];
            lrPosted.miType = liType;
            lrPosted.miSize = liSize;
            const s32 liCopy = liSize < static_cast<s32>(sizeof(lrPosted.maBytes))
                ? liSize : static_cast<s32>(sizeof(lrPosted.maBytes));
            std::memcpy(lrPosted.maBytes, lpEvent, liCopy);
        }
        ++miCount;
    }
    template<class T> void AddEvent(const T* lpEvent, s32 liType)
    {
        AddEvent(static_cast<const void*>(lpEvent), liType, static_cast<s32>(sizeof(T)));
    }
};

// Only fields read or written by the three extracted production bodies are represented here.
struct HUDFixture
{
    f32 mfNextMileBoundaryForMessage;
    f32 mfNextKMBoundaryForMessage;
    f32 mfBlueTeamIsAheadTime;
    f32 mfBlueTeamIsBehindTime;
    ActionRecorder mActionQueue;

    void GenerateOnlineBlueTeamEscapingMessage(ScoringFixture*, f32, EActiveRaceCarIndex);
    void GenerateOnlineBlueTeamAreBehindYouMessage(ScoringFixture*, f32, EActiveRaceCarIndex);
    void GenerateOnlineLeaderMilestoneMessage(ScoringFixture*, f32, EActiveRaceCarIndex, bool);
};
}

#include "online_roadrage_hud_methods.inc"

using namespace BrnGameState;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static ScoringFixture gScoring;
static HUDFixture gHud;

static void Fresh()
{
    std::memset(&gScoring, 0, sizeof(gScoring));
    gScoring.muActiveCars = E_ACTIVE_RACE_CAR_INDEX_COUNT;
    for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
    {
        CarFixture& lrCar = gScoring.maCars[li];
        lrCar.mbPresent = true;
        lrCar.meTeam = GameStateModuleIO::E_PLAYER_TEAM_NONE;
        lrCar.mfDistanceToFinish = 10000.0f;
        lrCar.mScore.SetEliminated(true);
        lrCar.mScore.SetRacePosition(7);
    }
    gHud.mActionQueue.Reset();
    gHud.mfNextMileBoundaryForMessage = 5.0f;
    gHud.mfNextKMBoundaryForMessage = 5.0f;
    gHud.mfBlueTeamIsAheadTime = 0.0f;
    gHud.mfBlueTeamIsBehindTime = 0.0f;
    gAsserts = 0;
}

static void SetCar(EActiveRaceCarIndex leIndex,
                   GameStateModuleIO::EPlayerTeam leTeam,
                   f32 lfDistanceToFinish,
                   s32 liRacePosition,
                   bool lbEliminated = false)
{
    CarFixture& lrCar = gScoring.maCars[leIndex];
    lrCar.mbPresent = true;
    lrCar.meTeam = leTeam;
    lrCar.mfDistanceToFinish = lfDistanceToFinish;
    lrCar.mScore.SetEliminated(lbEliminated);
    lrCar.mScore.SetRacePosition(liRacePosition);
}

static const GameStateModuleIO::LeaderReachedBoundaryAction& BoundaryAction(s32 liEvent)
{
    return *reinterpret_cast<const GameStateModuleIO::LeaderReachedBoundaryAction*>(
        gHud.mActionQueue.maPosted[liEvent].maBytes);
}

int main()
{
    const EActiveRaceCarIndex KE_PLAYER = E_ACTIVE_RACE_CAR_INDEX_0;

    // ---- action 162: exact 400 m is not "more than 400 m ahead" -------------------------------
    Fresh();
    SetCar(KE_PLAYER, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 5000.0f, 0);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4600.0f, 4);
    gHud.mfBlueTeamIsAheadTime = 10.0f;
    gHud.GenerateOnlineBlueTeamEscapingMessage(&gScoring, 0.5f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfBlueTeamIsAheadTime == 0.0f,
          "162 exact 400 m difference resets the accumulator; comparison is strict  @0x82395300");

    // ---- action 162: timer threshold is strict, then the one-byte producer resets it ----------
    Fresh();
    SetCar(KE_PLAYER, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 5000.0f, 0);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4599.0f, 4);
    gHud.mfBlueTeamIsAheadTime = 59.0f;
    gHud.GenerateOnlineBlueTeamEscapingMessage(&gScoring, 1.0f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfBlueTeamIsAheadTime == 60.0f,
          "162 exactly 60 s does not post (ble skips at equality)  @0x8239537C..0x82395380");
    gHud.GenerateOnlineBlueTeamEscapingMessage(&gScoring, 0.25f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 1 && gHud.mActionQueue.maPosted[0].miType == 162
              && gHud.mActionQueue.maPosted[0].miSize == 1 && gHud.mfBlueTeamIsAheadTime == 0.0f,
          "162 60.25 s posts one byte, then clears the timer  @0x82395380..0x8239539C");

    // ---- unordered distance/timer comparisons preserve ARTIST fcmpu branch polarity -----------
    Fresh();
    SetCar(KE_PLAYER, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 5000.0f, 0);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM,
           std::numeric_limits<f32>::quiet_NaN(), 4);
    gHud.mfBlueTeamIsAheadTime = 20.0f;
    gHud.GenerateOnlineBlueTeamEscapingMessage(&gScoring, 1.0f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfBlueTeamIsAheadTime == 0.0f,
          "162 unordered candidate gap is treated as not-more-than-400 m and resets  @0x82395300..0x82395338");

    // ---- action 163: exactly 15 m behind qualifies, but exactly 8 s still does not post -------
    Fresh();
    SetCar(KE_PLAYER, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 5000.0f, 0);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 5015.0f, 4);
    gHud.mfBlueTeamIsBehindTime = 7.0f;
    gHud.GenerateOnlineBlueTeamAreBehindYouMessage(&gScoring, 1.0f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfBlueTeamIsBehindTime == 8.0f,
          "163 exact 15 m and exact 8 s remain at the inclusive timer boundary  @0x82395498..0x82395518");
    gHud.GenerateOnlineBlueTeamAreBehindYouMessage(&gScoring, 0.25f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 1 && gHud.mActionQueue.maPosted[0].miType == 163
              && gHud.mActionQueue.maPosted[0].miSize == 1 && gHud.mfBlueTeamIsBehindTime == 0.0f,
          "163 8.25 s posts one byte, then clears the timer  @0x82395518..0x82395534");

    Fresh();
    SetCar(KE_PLAYER, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 5000.0f, 0);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 5014.0f, 4);
    gHud.mfBlueTeamIsBehindTime = 7.0f;
    gHud.GenerateOnlineBlueTeamAreBehindYouMessage(&gScoring, 1.0f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfBlueTeamIsBehindTime == 0.0f,
          "163 less than 15 m behind resets the timer  @0x82395498..0x823954A0");

    Fresh();
    SetCar(KE_PLAYER, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 5000.0f, 0);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM,
           std::numeric_limits<f32>::quiet_NaN(), 4);
    gHud.mfBlueTeamIsBehindTime = 8.0f;
    gHud.GenerateOnlineBlueTeamAreBehindYouMessage(&gScoring, 0.25f, KE_PLAYER);
    Check(gHud.mActionQueue.miCount == 1 && gHud.mActionQueue.maPosted[0].miType == 163
              && gHud.mActionQueue.maPosted[0].miSize == 1 && gHud.mfBlueTeamIsBehindTime == 0.0f,
          "163 unordered distance falls through blt, accumulates, and posts over 8 s  @0x82395498..0x82395518");

    // ---- leader eligibility: first team != 1 short-circuits the second team query ------------
    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4000.0f, 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gScoring.maiTeamQueries[E_ACTIVE_RACE_CAR_INDEX_1] == 1,
          "164 first team != 1 skips the second GetPlayerTeam call  @0x8239562C..0x82395634");
    Check(gHud.mActionQueue.miCount == 2 && gHud.mActionQueue.maPosted[0].miType == 164
              && gHud.mActionQueue.maPosted[0].miSize == 12
              && BoundaryAction(0).meRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_1,
          "164 first team != 1 remains eligible and selects the lower race-position car  @0x82395634..0x82395658");

    // ---- first team == 1 makes the second call; second team == 0 skips -------------------------
    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 4000.0f, 1);
    gScoring.SetTeamReplies(E_ACTIVE_RACE_CAR_INDEX_1,
                            GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM,
                            GameStateModuleIO::E_PLAYER_TEAM_NONE, 2);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gScoring.maiTeamQueries[E_ACTIVE_RACE_CAR_INDEX_1] == 2 && gHud.mActionQueue.miCount == 0,
          "164 first team == 1 queries again and second team == 0 skips  @0x82395638..0x82395648");

    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM, 4000.0f, 1);
    gScoring.SetTeamReplies(E_ACTIVE_RACE_CAR_INDEX_1,
                            GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM,
                            GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 2);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gScoring.maiTeamQueries[E_ACTIVE_RACE_CAR_INDEX_1] == 2 && gHud.mActionQueue.miCount == 2
              && BoundaryAction(0).meRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_1,
          "164 first team == 1 plus second team != 0 proceeds to race-position comparison  @0x82395638..0x82395658");

    // ---- eliminated cars do not get a team query; strict position ties keep the first car ------
    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4000.0f, 1, true);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gScoring.maiTeamQueries[E_ACTIVE_RACE_CAR_INDEX_1] == 0 && gHud.mActionQueue.miCount == 0,
          "164 eliminated candidates are filtered before GetPlayerTeam  @0x8239561C..0x8239562C");

    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_0, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4000.0f, 1);
    SetCar(E_ACTIVE_RACE_CAR_INDEX_1, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4000.0f, 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gHud.mActionQueue.miCount == 2
              && BoundaryAction(0).meRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_0
              && BoundaryAction(1).meRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_0,
          "164 equal race positions retain the first car because the comparison is strict  @0x8239564C..0x82395650");

    // ---- mode-progress suppresses posts after consuming crossed mile/km boundaries ------------
    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_0, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4000.0f, 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, false);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfNextMileBoundaryForMessage == 4.0f
              && gHud.mfNextKMBoundaryForMessage == 4.0f,
          "164 out-of-progress suppresses both events but still decrements both boundaries");

    // ---- mile posts first, then kilometre; strict threshold and unordered behavior ------------
    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_0, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 4000.0f, 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gHud.mActionQueue.miCount == 2
              && BoundaryAction(0).mbIsBoundaryInMiles
              && BoundaryAction(0).mfDistanceInMetres == 5.0f * 1609.344f
              && !BoundaryAction(1).mbIsBoundaryInMiles
              && BoundaryAction(1).mfDistanceInMetres == 5000.0f,
          "164 posts the mile boundary before the kilometre boundary with their typed fields");

    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_0, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM, 5000.0f, 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gHud.mActionQueue.miCount == 1 && BoundaryAction(0).mbIsBoundaryInMiles
              && gHud.mfNextMileBoundaryForMessage == 4.0f
              && gHud.mfNextKMBoundaryForMessage == 5.0f,
          "164 equality at 5 km does not consume km, but distance remains below 5 miles");

    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_0, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM,
           5.0f * 1609.344f, 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfNextMileBoundaryForMessage == 5.0f
              && gHud.mfNextKMBoundaryForMessage == 5.0f,
          "164 equality at the 5-mile threshold does not consume either strict boundary");

    Fresh();
    SetCar(E_ACTIVE_RACE_CAR_INDEX_0, GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM,
           std::numeric_limits<f32>::quiet_NaN(), 1);
    gHud.GenerateOnlineLeaderMilestoneMessage(&gScoring, 0.0f, KE_PLAYER, true);
    Check(gHud.mActionQueue.miCount == 0 && gHud.mfNextMileBoundaryForMessage == 5.0f
              && gHud.mfNextKMBoundaryForMessage == 5.0f,
          "164 unordered leader distance follows fcmpu+bge skip and consumes no boundary  @0x823956B8..0x8239571C");

    std::printf("FxGsOnlineRoadRageHud: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
