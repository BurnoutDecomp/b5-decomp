// FX-GS (crash parity 2026-09-23, G10-D8): the PRODUCTION takedown-scoring bodies, extracted by
// run_fxgs_scoring_takedowns.py:
//   ScoringSystem::UpdateTakedowns          (BrnScoringSystem_UpdateA.cpp)   ARTIST 0x8232AC88
//   StuntModeScoringOnline::DealWithTakedown (BrnStuntModeScoringOnline.cpp)  ARTIST 0x82321890
//   StuntModeScoringOnline::Construct / Prepare / Destruct                    ARTIST 0x8232D060 /
//                                                                              0x82338B50 / 0x8232D0F8
// UpdateTakedowns runs on a ScoringFixture (the text is re-homed from ScoringSystem:: -- it reads only
// GetCarData / GetPlayerTeam / mOnlineStuntModeScoring) against the REAL TakedownEventQueue and
// CarScoreData types. The online scorer's bodies run on the REAL StuntModeScoringOnline object; the
// base StuntModeScoring calls they make (Construct / RegisterStunt / UpdateScore / UpdateStuntRating)
// are counted by harness definitions here.
//
// Checked against the asm:
//   0x8232AE40..0x8232AE7C  if ((player == aggressor && team(victim) != team(aggressor)) || stunt
//                           challenge) DealWithTakedown(ss+0x2620) -- only inside the both-CarData block
//   0x823218A4              `lbz 0x28` = mbStuntModeActive gates DealWithTakedown; then RegisterStunt,
//                           UpdateScore(3000.0 flt_8202112C, 15, false), UpdateStuntRating(15, 0, 0, 0)
//   0x8232D060              base Construct(r4 passed through), the two array counts, the StoredLeapingData
//                           pool Clear image, std 0 +0x22D0, the three bools, +0x2510 = 0 -- no +0x24F8 store
//   0x82338B50              pool Clear image + stb 0 +0x2515 only (+0x22D0 untouched), returns 1
//   0x8232D0F8              the Construct stores minus the base call
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoringOnline.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "scoring_takedowns_config.inc"   // the revision's UpdateTakedowns shape (runner-generated)
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static unsigned gBaseConstructs = 0, gRegisters = 0, gScores = 0, gRatings = 0;
static void*    gpBaseConstructManager = nullptr;
static bool     gbRegisterResult = true;
static f32      gfScore = 0.0f;
static s32      giScoreType = -1, giRatingType = -1;
static bool     gbScoreFlag = true;
static f32      gafRating[3] = { -1.0f, -1.0f, -1.0f };

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// ---- harness definitions of the base-class bodies the extracted online bodies call ------------
namespace BrnGameState
{
void StuntModeScoring::Construct(AchievementManager* lpAchievementManager)
{
    ++gBaseConstructs;
    gpBaseConstructManager = lpAchievementManager;
}
bool StuntModeScoring::RegisterStunt() { ++gRegisters; return gbRegisterResult; }
void StuntModeScoring::UpdateScore(f32 lfScore, EStuntType leType, bool lbFlag)
{
    ++gScores; gfScore = lfScore; giScoreType = static_cast<s32>(leType); gbScoreFlag = lbFlag;
}
void StuntModeScoring::UpdateStuntRating(EStuntType leType, f32 lf0, f32 lf1, f32 lf2)
{
    ++gRatings; giRatingType = static_cast<s32>(leType); gafRating[0] = lf0; gafRating[1] = lf1; gafRating[2] = lf2;
}

// ---- the ScoringSystem fixture UpdateTakedowns is re-homed onto --------------------------------
struct CarData
{
    alignas(16) unsigned char maScoreStorage[sizeof(GameStateModuleIO::CarScoreData)];
    GameStateModuleIO::CarScoreData* GetScoreData() { return reinterpret_cast<GameStateModuleIO::CarScoreData*>(maScoreStorage); }
};
struct OnlineScorerFixture
{
    unsigned muCalls = 0;
    void DealWithTakedown() { ++muCalls; }
};
struct ScoringFixture
{
    CarData                        maCars[8];
    bool                           mabHasCar[8];
    GameStateModuleIO::EPlayerTeam maeTeams[8];
    OnlineScorerFixture            mOnlineStuntModeScoring;

    CarData* GetCarData(::EActiveRaceCarIndex leIndex) { return mabHasCar[leIndex] ? &maCars[leIndex] : nullptr; }
    GameStateModuleIO::EPlayerTeam GetPlayerTeam(::EActiveRaceCarIndex leIndex) { return maeTeams[leIndex]; }
    FXGS_UPDATE_TAKEDOWNS_DECLARATION;
};
}

#include "scoring_takedowns_methods.inc"

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
static InputBuffer::TakedownEventQueue gQueue;

static void ResetScoring()
{
    std::memset(&gScoring, 0, sizeof(gScoring));
    for (s32 li = 0; li < 8; ++li)
    {
        gScoring.mabHasCar[li] = true;
        gScoring.maeTeams[li]  = GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }
    gQueue.Construct();
}

static void Push(s32 liAggressor, s32 liVictim)
{
    TakedownEvent lEvent = {};
    lEvent.meAggressorIndex = static_cast<::EActiveRaceCarIndex>(liAggressor);
    lEvent.meVictimIndex    = static_cast<::EActiveRaceCarIndex>(liVictim);
    gQueue.AddEvent(lEvent);
}

// The takedown arm: one event, then the number of DealWithTakedown calls it produced.
static unsigned Arm(s32 liAggressor, s32 liVictim, s32 liAggressorTeam, s32 liVictimTeam,
                    s32 liPlayer, bool lbChallenge, bool lbBothCars = true)
{
    ResetScoring();
    gScoring.maeTeams[liAggressor] = static_cast<GameStateModuleIO::EPlayerTeam>(liAggressorTeam);
    gScoring.maeTeams[liVictim]    = static_cast<GameStateModuleIO::EPlayerTeam>(liVictimTeam);
    if (!lbBothCars) gScoring.mabHasCar[liVictim] = false;
    Push(liAggressor, liVictim);
    FXGS_CALL_UPDATE_TAKEDOWNS(gScoring, &gQueue, static_cast<::EActiveRaceCarIndex>(liPlayer), lbChallenge);
    return gScoring.mOnlineStuntModeScoring.muCalls;
}

alignas(16) static unsigned char gaOnlineStorage[sizeof(StuntModeScoringOnline)];

int main()
{
    // ================= UpdateTakedowns: the trailing online-stunt arm (0x8232AE40..0x8232AE7C) ===
    Check(Arm(2, 5, 1, 2, 2, false) == 1, "D8 player (2) takes down a car on another team -> DealWithTakedown once");
    Check(Arm(2, 5, 1, 1, 2, false) == 0, "D8 player takes down a car on its OWN team -> no call");
    Check(Arm(2, 5, 0, 0, 2, false) == 0, "D8 offline (both teams NONE) player takedown -> no call");
    Check(Arm(3, 5, 1, 2, 2, false) == 0, "D8 a rival's takedown on another team, no challenge -> no call");
    Check(Arm(3, 5, 1, 1, 2, true)  == 1, "D8 any takedown during a stunt challenge -> DealWithTakedown once");
    Check(Arm(2, 5, 1, 2, 2, true, false) == 0,
          "D8 the arm sits inside the both-CarData block (victim CarData null -> no call)");
    {
        ResetScoring();
        Push(2, 5);
        Push(4, 6);
        FXGS_CALL_UPDATE_TAKEDOWNS(gScoring, &gQueue, ::E_ACTIVE_RACE_CAR_INDEX_2, true);
        Check(gScoring.mOnlineStuntModeScoring.muCalls == 2, "D8 one call per qualifying event (two events, challenge on)");
        Check(gScoring.maCars[2].GetScoreData()->GetTakedowns() == 1 && gScoring.maCars[5].GetScoreData()->GetTakedownsAgainst() == 1
                  && gScoring.maCars[4].GetScoreData()->GetTakedowns() == 1,
              "the CarScoreData tallies are unchanged (+0x4C aggressor, +0x50 victim)");
    }

    // ================= DealWithTakedown's gate (0x823218A4 lbz 0x28) ================================
    StuntModeScoringOnline& lrOnline = *reinterpret_cast<StuntModeScoringOnline*>(gaOnlineStorage);
    for (int liCase = 0; liCase < 3; ++liCase)
    {
        std::memset(gaOnlineStorage, 0, sizeof(gaOnlineStorage));
        gRegisters = gScores = gRatings = 0;
        gbRegisterResult = (liCase != 2);
        lrOnline.mbStuntModeActive = (liCase != 1);   // case 1: active clear, in-progress set
        lrOnline.mbStuntInProgress = (liCase == 1);
        lrOnline.DealWithTakedown();
        if (liCase == 0)
        {
            Check(gRegisters == 1, "D8 mbStuntModeActive (+0x28) set -> RegisterStunt  @0x823218B0");
            Check(gScores == 1 && gfScore == 3000.0f && giScoreType == 15 && gbScoreFlag == false,
                  "D8 ...UpdateScore(3000.0 flt_8202112C, 15, false)  @0x823218D4");
            Check(gRatings == 1 && giRatingType == 15 && gafRating[0] == 0.0f && gafRating[1] == 0.0f && gafRating[2] == 0.0f,
                  "D8 ...UpdateStuntRating(15, 0, 0, 0)  @0x823218F0");
        }
        else if (liCase == 1)
        {
            Check(gRegisters == 0 && gScores == 0,
                  "D8 mbStuntModeActive clear -> nothing, even with mbStuntInProgress (+0x29) set");
        }
        else
        {
            Check(gRegisters == 1 && gScores == 0 && gRatings == 0, "D8 RegisterStunt false -> no score  @0x823218BC");
        }
    }

    // ================= the online scorer's lifecycle (it is now the real ss+0x2620 object) ==========
    {
        std::memset(gaOnlineStorage, 0xCD, sizeof(gaOnlineStorage));
        lrOnline.mStoredLeapingDataPool.Clear();
        lrOnline.mStoredLeapingDataPool.AllocateObject();
        lrOnline.mStoredLeapingDataPool.AllocateObject();
        lrOnline.maMultiplierData.Clear();
        lrOnline.maChainableMultiplierInfo.Clear();
        GameStateModuleIO::ChainableMultiplierInfo lInfo = {};
        lrOnline.maChainableMultiplierInfo.Append(lInfo);
        lrOnline.maPendingMultiplier[0] = 0x1234;
        gBaseConstructs = 0;
        void* lpManager = reinterpret_cast<void*>(static_cast<uintptr_t>(0x5150));
        lrOnline.Construct(reinterpret_cast<StuntModeScoring::AchievementManager*>(lpManager));
        Check(gBaseConstructs == 1 && gpBaseConstructManager == lpManager,
              "Construct passes the achievement manager straight to the base (r4 untouched)  @0x8232D074");
        Check(lrOnline.mStoredLeapingDataPool.GetNumFreeObjects() == 7
                  && lrOnline.mStoredLeapingDataPool.GetFirstObjectIndex() == -1
                  && lrOnline.mStoredLeapingDataPool.maiObjectFreeQueue[0] == 6
                  && lrOnline.mStoredLeapingDataPool.maiObjectFreeQueue[6] == 0,
              "Construct: the pool's Clear image (free queue 6..0, count 7, no bits)  @0x8232D0A0..0x8232D0C8");
        Check(lrOnline.mCarsAroundPlayerAtTakeoff == 0, "Construct: std 0 +0x22D0 (not 0x600000000)  @0x8232D0CC");
        Check(lrOnline.maChainableMultiplierInfo.GetCount() == 0 && lrOnline.maMultiplierData.GetCount() == 0,
              "Construct: both array counts 0  @0x8232D090/0x8232D098");
        Check(!lrOnline.mbPendingMultiplierValid && !lrOnline.mbStuntModeEndedCached && !lrOnline.mbCarsAroundPlayerCaptured
                  && lrOnline.miOnlineDisplayScore == 0,
              "Construct: +0x2514/+0x2515/+0x2516 = 0, +0x2510 = 0");
        Check(lrOnline.maPendingMultiplier[0] == 0x1234, "Construct: no +0x24F8 store");
    }
    {
        std::memset(gaOnlineStorage, 0xCD, sizeof(gaOnlineStorage));
        lrOnline.mStoredLeapingDataPool.Clear();
        lrOnline.mStoredLeapingDataPool.AllocateObject();
        lrOnline.mCarsAroundPlayerAtTakeoff = 0x77;
        lrOnline.mbStuntModeEndedCached = true;
        const bool lbPrepared = lrOnline.Prepare();
        Check(lbPrepared && lrOnline.mStoredLeapingDataPool.GetNumFreeObjects() == 7
                  && lrOnline.mStoredLeapingDataPool.GetFirstObjectIndex() == -1,
              "Prepare: the pool's Clear image, returns 1  @0x82338B64..0x82338B84");
        Check(!lrOnline.mbStuntModeEndedCached, "Prepare: stb 0 +0x2515  @0x82338B88");
        Check(lrOnline.mCarsAroundPlayerAtTakeoff == 0x77, "Prepare: +0x22D0 is NOT stored");
    }
    {
        std::memset(gaOnlineStorage, 0xCD, sizeof(gaOnlineStorage));
        lrOnline.mStoredLeapingDataPool.Clear();
        lrOnline.mStoredLeapingDataPool.AllocateObject();
        gBaseConstructs = 0;
        lrOnline.Destruct();
        Check(lrOnline.mCarsAroundPlayerAtTakeoff == 0, "Destruct: std 0 +0x22D0  @0x8232D138");
        Check(lrOnline.mStoredLeapingDataPool.GetNumFreeObjects() == 7 && lrOnline.mStoredLeapingDataPool.GetFirstObjectIndex() == -1,
              "Destruct: the pool's Clear image  @0x8232D13C..0x8232D15C");
        Check(lrOnline.miOnlineDisplayScore == 0 && !lrOnline.mbPendingMultiplierValid && gBaseConstructs == 0,
              "Destruct: +0x2510/+0x2514 = 0 and no base call");
    }

    Check(gAsserts == 0, "valid fixtures fire no assert");
    std::printf("FxGsScoringTakedowns: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
