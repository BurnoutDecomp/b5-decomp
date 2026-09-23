// FX-GS (crash parity 2026-09-23, G11-D4): the PRODUCTION online team-mode bodies --
// ModeManager::HandleOnlineTeamModes / HandleOnlineTeamTakedowns / HandleOnlineTeamCheckForModeFinished /
// HandleOnlineBurningHomeRunCheckForModeFinished (BrnModeManager_UpdateMode.cpp) and
// GameStateModule::GetNetworkPlayerID (GameStateModule_GetActiveRaceCarIndexFromNetworkPlayer.cpp),
// extracted by run_fxgs_online_team.py and re-homed onto small fixtures (ModeFixture for the
// ModeManager, ScoringFixture for the ScoringSystem; the takedown queue, the player-status interface,
// the action records and CarScoreData are the REAL types). Checked against the ARTIST asm:
//   HandleOnlineTeamModes @0x8234C750      online && in progress && mode 11|13; takedowns first,
//                                          then mode 13 -> BHR finish check, else the team check
//   HandleOnlineTeamTakedowns @0x823440B8  blue players left = status records not eliminated
//                                          (+0xD9) on team 2; per event: eliminated victim ->
//                                          skip (finish test too); same team in 12/14/17 -> 166/4
//                                          {aggressor}; red on blue: mode 11 -> left-1,
//                                          SetPlayerEliminated(victim, aggressor), 165/8 {victim,
//                                          left==1, victim==player}; mode 13 -> 167/4
//                                          {GetNetworkPlayerID(aggressor)}; finish test INSIDE the
//                                          loop (left == 0 && mode 11 -> PlayerFinishedMode {0,0,0})
//   HandleOnlineTeamCheckForModeFinished @0x82328910  car count > 0; any connected, blue,
//                                          non-eliminated car short of the laps -> no finish
//   HandleOnlineBurningHomeRunCheckForModeFinished @0x82328A70  first global slot with no
//                                          checkpoint left -> GetActiveRaceCarIndex(slot as the id)
//                                          -> +0xBC = 1 -> PlayerFinishedMode {player team != 2, 0, 0}
//   GetNetworkPlayerID @0x823639C0         const GetCarData ? +0x148 : -1
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }      // VariableEventQueue::OutputQueueContents
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

// Harness-only: two out-of-line default constructors the fixtures' members name (not under test;
// every field the bodies read is set explicitly by each case).
BrnGameState::GameStateModuleIO::CarScoreData::CarScoreData() { std::memset(this, 0, sizeof(*this)); }
CgsSystem::DateAndTime::DateAndTime() {}

namespace BrnGameState
{
// ---- the ScoringSystem stand-in: the queries the bodies make, over eight car slots -------------
struct CarData
{
    GameStateModuleIO::CarScoreData mScore;
    GameStateModuleIO::EPlayerTeam  meTeam;
    BrnNetwork::NetworkPlayerID     mNetworkPlayerID;
    bool                            mbPresent;
    u32                             muCompletedLaps;

    GameStateModuleIO::CarScoreData*       GetScoreData()       { return &mScore; }
    const GameStateModuleIO::CarScoreData* GetScoreData() const { return &mScore; }
    BrnNetwork::NetworkPlayerID GetNetworkPlayerID() const { return mNetworkPlayerID; }
};

struct ScoringFixture
{
    CarData maCars[8];
    u32     muActiveCars;
    u32     muTotalLaps;
    s32     miEliminatedCalls;
    EActiveRaceCarIndex meEliminatedVictim, meEliminatedBy;

    CarData* GetCarData(EActiveRaceCarIndex le)
    {
        CGS_ASSERT(le >= 0 && le < 8, "index");
        return (le >= 0 && le < 8 && maCars[le].mbPresent) ? &maCars[le] : NULL;
    }
    const CarData* GetCarData(EActiveRaceCarIndex le) const
    {
        CGS_ASSERT(le >= 0 && le < 8, "index");
        return (le >= 0 && le < 8 && maCars[le].mbPresent) ? &maCars[le] : NULL;
    }
    GameStateModuleIO::EPlayerTeam GetPlayerTeam(EActiveRaceCarIndex le) const
    {
        const CarData* lp = GetCarData(le);
        return lp ? lp->meTeam : GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }
    void SetPlayerEliminated(EActiveRaceCarIndex leVictim, EActiveRaceCarIndex leEliminator)
    {
        ++miEliminatedCalls;
        meEliminatedVictim = leVictim;
        meEliminatedBy = leEliminator;
        maCars[leVictim].mScore.SetEliminated(true);
    }
    u32  GetNumberOfActiveCars() const { return muActiveCars; }
    bool GetPlayerDisconnected(EActiveRaceCarIndex le) const
    {
        const CarData* lp = GetCarData(le);
        return lp ? lp->mScore.GetDisconnected() : false;
    }
    u32 GetRaceCarNumCompletedLaps(EActiveRaceCarIndex le) const { return maCars[le].muCompletedLaps; }
    u32 GetTotalLaps() const { return muTotalLaps; }
};

struct ModeManagerRef
{
    ScoringFixture* mpScoring;
    ScoringFixture* GetScoringSystem() { return mpScoring; }
};

// ---- the GameStateModule stand-in (GetNetworkPlayerID is the production body) -----------------
class GameStateModule
{
public:
    ModeManagerRef      mModeManager;
    EActiveRaceCarIndex mePlayer;
    BrnNetwork::NetworkPlayerID mLastLookedUpID;
    EActiveRaceCarIndex meLookupAnswer;
    s32                 miLookups;

    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() { return mePlayer; }
    EActiveRaceCarIndex GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID lID)
    {
        ++miLookups;
        mLastLookedUpID = lID;
        return meLookupAnswer;
    }
    BrnNetwork::NetworkPlayerID GetNetworkPlayerID(::EActiveRaceCarIndex leActiveRaceCarIndex);
};

namespace GameStateModuleIO
{
struct PreWorldInputBuffer
{
    BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface mStatus;
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* GetPlayerStatusInterface() const { return &mStatus; }
};
struct OutputBuffer
{
    GameActionQueue mActions;
    GameActionQueue* GetGameActionQueue() { return &mActions; }
};
}

// ---- the ModeManager stand-in: the members and helpers the four bodies read ------------------
class ModeFixture
{
public:
    GameStateModuleIO::EGameModeType meCurrentGameModeType;
    bool             mbOnline, mbInProgress;
    ScoringFixture*  mpScoring;
    GameStateModule* mpGameStateModule;
    u32              mauCheckpointsRemaining[E_GLOBAL_RACE_CAR_INDEX_COUNT];
    s32              miFinishCalls;
    GameStateModuleIO::PlayerFinishedModeEvent mLastFinish;

    bool IsOnlineGameMode() const { return mbOnline; }
    bool IsInProgress() const { return mbInProgress; }
    ScoringFixture* GetScoringSystem() { return mpScoring; }
    u32 CountCheckpointsRemaining(EGlobalRaceCarIndex le) const { return mauCheckpointsRemaining[le]; }
    void PlayerFinishedMode(const GameStateModuleIO::PlayerFinishedModeEvent* lp) { ++miFinishCalls; mLastFinish = *lp; }

    void HandleOnlineTeamModes(const GameStateModuleIO::PreWorldInputBuffer*, GameStateModuleIO::OutputBuffer*,
                               InputBuffer::TakedownEventQueue*);
    void HandleOnlineTeamTakedowns(const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface*,
                                   const InputBuffer::TakedownEventQueue*, GameStateModuleIO::GameActionQueue*);
    void HandleOnlineTeamCheckForModeFinished();
    void HandleOnlineBurningHomeRunCheckForModeFinished();
};
}

// The production bodies (re-homed by the runner).
#include "online_team_methods.inc"

using namespace BrnGameState;
using namespace BrnGameState::GameStateModuleIO;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static ScoringFixture       gScoring;
static GameStateModule      gModule;
static ModeFixture          gMode;
static PreWorldInputBuffer  gInput;
static OutputBuffer         gOutput;
static InputBuffer::TakedownEventQueue gTakedowns;

struct Posted { s32 miType; s32 miSize; u8 maBytes[16]; };

static s32 Drain(Posted* lpOut, s32 liMax)
{
    s32 liCount = 0;
    const CgsModule::Event* lpEvent = nullptr;
    s32 liSize = 0;
    for (s32 liType = gOutput.mActions.GetFirstEvent(&lpEvent, &liSize); liType != -1;
         liType = gOutput.mActions.GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liCount < liMax)
        {
            lpOut[liCount].miType = liType;
            lpOut[liCount].miSize = liSize;
            std::memset(lpOut[liCount].maBytes, 0, sizeof(lpOut[liCount].maBytes));
            std::memcpy(lpOut[liCount].maBytes, lpEvent, liSize < 16 ? liSize : 16);
        }
        ++liCount;
    }
    return liCount;
}

// Eight present cars; teams given as a string of '0'/'1'/'2' per slot; the status interface lists
// the given number of players, player i driving car i.
static void Fresh(EGameModeType leMode, const char* lpcTeams, s32 liPlayers)
{
    std::memset(&gScoring, 0, sizeof(gScoring));
    for (s32 li = 0; li < 8; ++li)
    {
        gScoring.maCars[li].mbPresent = true;
        gScoring.maCars[li].meTeam = static_cast<EPlayerTeam>(lpcTeams[li] - '0');
        gScoring.maCars[li].mNetworkPlayerID = 100 + li;
    }
    gScoring.muActiveCars = 8;
    gScoring.muTotalLaps = 2;

    std::memset(&gModule, 0, sizeof(gModule));
    gModule.mModeManager.mpScoring = &gScoring;
    gModule.mePlayer = E_ACTIVE_RACE_CAR_INDEX_1;
    gModule.meLookupAnswer = E_ACTIVE_RACE_CAR_INDEX_INVALID;

    std::memset(&gMode, 0, sizeof(gMode));
    gMode.meCurrentGameModeType = leMode;
    gMode.mbOnline = true;
    gMode.mbInProgress = true;
    gMode.mpScoring = &gScoring;
    gMode.mpGameStateModule = &gModule;
    for (s32 li = 0; li < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++li)
    {
        gMode.mauCheckpointsRemaining[li] = 3;
    }

    std::memset(&gInput, 0, sizeof(gInput));
    gInput.mStatus.miNumPlayers = liPlayers;
    for (s32 li = 0; li < 8; ++li)
    {
        gInput.mStatus.maInGamePlayerData[li].meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(li);
    }
    gOutput.mActions.Construct();
    gTakedowns.Construct();
    gAsserts = 0;
}

static void Takedown(s32 liAggressor, s32 liVictim)
{
    TakedownEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.meAggressorIndex = static_cast<EActiveRaceCarIndex>(liAggressor);
    lEvent.meVictimIndex = static_cast<EActiveRaceCarIndex>(liVictim);
    gTakedowns.AddEvent(lEvent);
}

static void RunTakedowns() { gMode.HandleOnlineTeamTakedowns(&gInput.mStatus, &gTakedowns, &gOutput.mActions); }

int main()
{
    Posted laPosted[8];

    // ---- mode 11: red (1) takes down blue (2) ----------------------------------------------------
    //          slot: 0 1 2 3 4 5 6 7     three blue players (2, 3, 4) among the 5 listed
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "11222111", 5);
    Takedown(0, 2);
    RunTakedowns();
    s32 liCount = Drain(laPosted, 8);
    Check(liCount == 1 && laPosted[0].miType == 165 && laPosted[0].miSize == 8,
          "D4 mode 11 red-on-blue posts PLAYER_ELIMINATED 165 (0xA5), size 8  @0x823443B8/0x823443B0");
    {
        PlayerEliminatedAction lAction;
        std::memcpy(&lAction, laPosted[0].maBytes, sizeof(lAction));
        Check(liCount == 1 && lAction.meActiveRaceCarIndex == 2 && !lAction.mbLastBlueTeamMember && !lAction.mbLocalPlayerEliminated,
              "D4 record {victim 2, left==1 false (2 left), victim==player false}");
    }
    Check(gScoring.miEliminatedCalls == 1 && gScoring.meEliminatedVictim == 2 && gScoring.meEliminatedBy == 0,
          "D4 SetPlayerEliminated(victim, aggressor)  @0x8234438C");
    Check(gMode.miFinishCalls == 0, "D4 blue players left -> no finish");

    // the last-but-one and the last blue player; the local player is the victim once
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "12122111", 5);   // blue: 1 (the player), 3, 4
    Takedown(0, 3);
    Takedown(2, 1);
    Takedown(0, 4);
    RunTakedowns();
    liCount = Drain(laPosted, 8);
    {
        PlayerEliminatedAction la[3];
        for (s32 li = 0; li < 3 && li < liCount; ++li) std::memcpy(&la[li], laPosted[li].maBytes, sizeof(la[li]));
        Check(liCount == 3 && la[1].meActiveRaceCarIndex == 1 && la[1].mbLastBlueTeamMember && la[1].mbLocalPlayerEliminated,
              "D4 second elimination: one blue left (mbLastBlueTeamMember) and it was the local player  @0x823443A4/0x823443C4");
        Check(liCount == 3 && la[2].meActiveRaceCarIndex == 4 && !la[2].mbLastBlueTeamMember,
              "D4 third elimination: none left, so not 'last member' (left == 0, not 1)");
    }
    Check(gMode.miFinishCalls == 1 && !gMode.mLastFinish.mbTimedOut && !gMode.mLastFinish.mbCarDestroyed &&
          !gMode.mLastFinish.mbCrossedFinishLine,
          "D4 no blue player left in mode 11 -> PlayerFinishedMode {0,0,0}  @0x823443F8");

    // the finish test is INSIDE the loop: a later event with nothing to post still re-runs it
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "12111111", 2);   // one blue player (1)
    Takedown(0, 1);
    Takedown(5, 6);                                  // red on red in mode 11: nothing posted
    RunTakedowns();
    Check(gMode.miFinishCalls == 2, "D4 the finish test runs after every non-skipped event (twice here)  @0x823443D0");

    // an already-eliminated victim skips the event entirely (finish test included)
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "12111111", 2);
    gScoring.maCars[1].mScore.SetEliminated(true);   // the only blue player is already out
    Takedown(0, 1);
    RunTakedowns();
    Check(Drain(laPosted, 8) == 0 && gMode.miFinishCalls == 0 && gScoring.miEliminatedCalls == 0,
          "D4 eliminated victim (+0xD9) -> next event, no post, no finish test  @0x823442E8");

    // blue players are counted from the STATUS records only
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "11111222", 5);   // blue cars 5,6,7 are not in the 5 listed players
    Takedown(0, 1);
    RunTakedowns();
    Check(gMode.miFinishCalls == 1, "D4 blue count walks the player-status records (+0x9E4 players, +0x114 index), not every car");

    // ---- same team in 12/14/17 -> TRAITOROUS_TAKEDOWN -------------------------------------------
    Fresh(E_MODE_ONLINE_FREE_BURN, "11222111", 5);
    Takedown(3, 4);
    RunTakedowns();
    liCount = Drain(laPosted, 8);
    {
        TraitorousTakedownAction lAction;
        std::memcpy(&lAction, laPosted[0].maBytes, sizeof(lAction));
        Check(liCount == 1 && laPosted[0].miType == 166 && laPosted[0].miSize == 4 && lAction.meAggrActiveRaceCarIndex == 3,
              "D4 same team in mode 14 -> 166 (0xA6) / 4 {aggressor 3}  @0x8234432C");
    }
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "11222111", 5);
    Takedown(3, 4);
    RunTakedowns();
    Check(Drain(laPosted, 8) == 0 && gScoring.miEliminatedCalls == 0, "D4 same team in mode 11 posts nothing");
    Fresh(E_MODE_ONLINE_FUGITIVE, "11222111", 5);
    Takedown(0, 1);
    RunTakedowns();
    Check(Drain(laPosted, 8) == 1, "D4 mode 12 counts as a traitorous-takedown mode too");

    // ---- mode 13: red on blue -> SWITCH_BURNING_HOME_RUN_RUNNER {GetNetworkPlayerID(aggressor)} -
    Fresh(E_MODE_ONLINE_BURNING_HOME_RUN, "11222111", 5);
    Takedown(5, 2);
    RunTakedowns();
    liCount = Drain(laPosted, 8);
    {
        SwitchBurningHomeRunRunnerAction lAction;
        std::memcpy(&lAction, laPosted[0].maBytes, sizeof(lAction));
        Check(liCount == 1 && laPosted[0].miType == 167 && laPosted[0].miSize == 4 && lAction.mNewRunnerPlayerID == 105,
              "D4 mode 13 red-on-blue -> 167 (0xA7) / 4 {GetNetworkPlayerID(aggressor 5) == CarData +0x148}  @0x82344370");
    }
    Check(gScoring.miEliminatedCalls == 0 && gMode.miFinishCalls == 0, "D4 mode 13 eliminates nobody and never finishes here");

    // ---- GetNetworkPlayerID ---------------------------------------------------------------------
    Fresh(E_MODE_ONLINE_BURNING_HOME_RUN, "11222111", 5);
    gScoring.maCars[6].mbPresent = false;
    Check(gModule.GetNetworkPlayerID(E_ACTIVE_RACE_CAR_INDEX_3) == 103 && gModule.GetNetworkPlayerID(E_ACTIVE_RACE_CAR_INDEX_6) == -1,
          "D4 GetNetworkPlayerID: CarData +0x148, or -1 without CarData  @0x823639DC/0x823639F0");

    // ---- HandleOnlineTeamCheckForModeFinished ---------------------------------------------------
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "11222111", 5);
    gScoring.maCars[2].muCompletedLaps = 2;
    gScoring.maCars[3].muCompletedLaps = 2;
    gScoring.maCars[4].muCompletedLaps = 1;
    gMode.HandleOnlineTeamCheckForModeFinished();
    Check(gMode.miFinishCalls == 0, "D4 team check: a blue car short of the laps -> no finish  @0x82328A20");
    gScoring.maCars[4].mScore.SetDisconnected(true);
    gMode.HandleOnlineTeamCheckForModeFinished();
    Check(gMode.miFinishCalls == 1 && !gMode.mLastFinish.mbTimedOut, "D4 team check: a disconnected blue car is ignored -> finish {0,0,0}");
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "11222111", 5);
    gScoring.maCars[2].muCompletedLaps = 2;
    gScoring.maCars[3].muCompletedLaps = 2;
    gScoring.maCars[4].mScore.SetEliminated(true);
    gMode.HandleOnlineTeamCheckForModeFinished();
    Check(gMode.miFinishCalls == 1, "D4 team check: an eliminated blue car is ignored (+0xD9)");
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "11222111", 5);
    gScoring.muActiveCars = 0;
    gMode.HandleOnlineTeamCheckForModeFinished();
    Check(gMode.miFinishCalls == 0, "D4 team check: no cars in the mode (+0x4EE8 <= 0) -> nothing");

    // ---- HandleOnlineBurningHomeRunCheckForModeFinished -----------------------------------------
    Fresh(E_MODE_ONLINE_BURNING_HOME_RUN, "12111111", 5);
    gMode.HandleOnlineBurningHomeRunCheckForModeFinished();
    Check(gMode.miFinishCalls == 0 && gModule.miLookups == 0, "D4 BHR check: every slot has checkpoints left -> nothing");
    gMode.mauCheckpointsRemaining[9] = 0;
    gMode.mauCheckpointsRemaining[20] = 0;
    gModule.meLookupAnswer = E_ACTIVE_RACE_CAR_INDEX_4;
    gMode.HandleOnlineBurningHomeRunCheckForModeFinished();
    Check(gModule.miLookups == 1 && gModule.mLastLookedUpID == 9,
          "D4 BHR check: the FIRST done slot (9) is looked up, as the network player id  @0x82328B18/0x82328B20");
    Check(gScoring.maCars[4].mScore.GetCompletedBurningHomeRun(), "D4 BHR check: that car's +0xBC is set  @0x82328B3C");
    Check(gMode.miFinishCalls == 1 && !gMode.mLastFinish.mbTimedOut && !gMode.mLastFinish.mbCarDestroyed,
          "D4 BHR check: the local player (car 1) is blue -> PlayerFinishedMode {0,0,0}");
    Fresh(E_MODE_ONLINE_BURNING_HOME_RUN, "11111111", 5);
    gMode.mauCheckpointsRemaining[0] = 0;
    gModule.meLookupAnswer = E_ACTIVE_RACE_CAR_INDEX_2;
    gMode.HandleOnlineBurningHomeRunCheckForModeFinished();
    Check(gMode.miFinishCalls == 1 && gMode.mLastFinish.mbTimedOut, "D4 BHR check: a red local player finishes timed out {1,0,0}  @0x82328BA0");
    Fresh(E_MODE_ONLINE_BURNING_HOME_RUN, "11111111", 5);
    gMode.mauCheckpointsRemaining[0] = 0;
    gModule.meLookupAnswer = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    gMode.HandleOnlineBurningHomeRunCheckForModeFinished();
    Check(gMode.miFinishCalls == 0, "D4 BHR check: a done slot with no active car -> nothing  @0x82328B2C");

    // ---- HandleOnlineTeamModes: the gate and the order ------------------------------------------
    Fresh(E_MODE_ONLINE_ROAD_RAGE, "12111111", 2);
    Takedown(0, 1);
    gMode.mbOnline = false;
    gMode.HandleOnlineTeamModes(&gInput, &gOutput, &gTakedowns);
    Check(Drain(laPosted, 8) == 0 && gMode.miFinishCalls == 0, "D4 offline -> nothing  @0x8234C78C");
    gMode.mbOnline = true;
    gMode.mbInProgress = false;
    gMode.HandleOnlineTeamModes(&gInput, &gOutput, &gTakedowns);
    Check(Drain(laPosted, 8) == 0, "D4 not in progress -> nothing  @0x8234C7B8");
    gMode.mbInProgress = true;
    gMode.meCurrentGameModeType = E_MODE_ONLINE_FREE_BURN;
    gMode.HandleOnlineTeamModes(&gInput, &gOutput, &gTakedowns);
    Check(Drain(laPosted, 8) == 0, "D4 mode 14 is not a team mode here (only 11 and 13 pass)  @0x8234C7C0..C8");
    gMode.meCurrentGameModeType = E_MODE_ONLINE_ROAD_RAGE;
    gMode.HandleOnlineTeamModes(&gInput, &gOutput, &gTakedowns);
    liCount = Drain(laPosted, 8);
    Check(liCount == 1 && laPosted[0].miType == 165, "D4 mode 11 in progress online: the takedown posts 165  @0x8234C890");
    // the team finish check ran after the takedowns: blue car 1 eliminated, nobody else blue ->
    // the in-loop finish (1) and then the team check's finish (2)
    Check(gMode.miFinishCalls == 2, "D4 mode 11: HandleOnlineTeamCheckForModeFinished follows the takedowns  @0x8234C8B0");
    Fresh(E_MODE_ONLINE_BURNING_HOME_RUN, "12111111", 2);
    gMode.mauCheckpointsRemaining[3] = 0;
    gModule.meLookupAnswer = E_ACTIVE_RACE_CAR_INDEX_1;
    gMode.HandleOnlineTeamModes(&gInput, &gOutput, &gTakedowns);
    Check(gModule.miLookups == 1 && gMode.miFinishCalls == 1, "D4 mode 13: the BHR finish check follows  @0x8234C8A4");
    Check(gAsserts == 0, "D4 no assert on any of these paths");

    std::printf("FxGsOnlineTeam: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
