#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.cpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
// UpdatePlayerTeams (below) reads the scoring system's per-car team through
// ScoringSystem::GetPlayerTeam. The class header only forward-declares ScoringSystem
// (deliberately -- BrnScoringSystem.h embeds the online scorers BY VALUE, so the
// HEADERS may not include each other); this .cpp can and must complete the type.
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#define CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\modemanager\\scoring\\BrnBaseOnlineModeScoring.h"
namespace BrnGameState
{
// X360 @ 0x823106F8. Finishing position recorded for active-race-car slot liRaceCarIndex. The two
// bounds checks fire as the build had them: two SEPARATE Begin/Fire/End assert sequences (lower-bound
// then upper-bound), each with its own baked-in message string and source line (343 / 344).
s32 BaseOnlineModeScoring::GetPlayerPosition(s32 liRaceCarIndex)
{
    CGS_ASSERT(liRaceCarIndex >= 0,
        CGS_ASSERT_MESSAGE_AT("liRaceCarIndex >= 0", CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE, 343 /* ARTIST 0x82310724 */));
    CGS_ASSERT(liRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS,
        CGS_ASSERT_MESSAGE_AT(
            "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS", CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE, 344 /* ARTIST 0x82310748 */));
    return maiPlayerPositions[liRaceCarIndex];
}

// X360 @ 0x82310770. Store the finishing position liPlayerPosition for active-race-car slot
// liRaceCarIndex. Four independent bounds asserts (the X360 build did NOT fold them into one combined
// check -- each fires its own distinct line number 356/357/359/360); the asserts never early-return,
// so the table write executes unconditionally even on a failed bound.
void BaseOnlineModeScoring::SetPlayerPosition(s32 liRaceCarIndex, s32 liPlayerPosition)
{
    CGS_ASSERT(liRaceCarIndex >= 0,
        CGS_ASSERT_MESSAGE_AT("liRaceCarIndex >= 0", CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE, 356 /* ARTIST 0x823107A0 */));
    CGS_ASSERT(liRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE, 357 /* ARTIST 0x823107C4 */));
    CGS_ASSERT(liPlayerPosition >= 0,
        CGS_ASSERT_MESSAGE_AT("liPlayerPosition >= 0", CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE, 359 /* ARTIST 0x823107E8 */));
    CGS_ASSERT(liPlayerPosition < E_ACTIVE_RACE_CAR_INDEX_COUNT,
        CGS_ASSERT_MESSAGE_AT(
            "liPlayerPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE, 360 /* ARTIST 0x8231080C */));

    maiPlayerPositions[liRaceCarIndex] = liPlayerPosition;
}

// X360 @ 0x82314638. Team currently assigned to active-race-car slot liRaceCarIndex (read from the
// per-slot maePlayerTeams[] array). Virtual. Two SEPARATE bounds asserts (lower bound at .cpp line
// 1010, upper bound at line 1011), each with its own verbatim message string and the exact baked-in
// .cpp path (note: this function's asserts use the .cpp path, not the .h path the sibling accessors use).
GameStateModuleIO::EPlayerTeam
BaseOnlineModeScoring::GetCurrentPlayerTeam(s32 liRaceCarIndex)
{
    CGS_ASSERT(liRaceCarIndex >= 0, CGS_ASSERT_MESSAGE_AT("liRaceCarIndex >= 0", CGS_ARTIST_FILE, 1010));
    CGS_ASSERT(liRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS, CGS_ASSERT_MESSAGE_AT("liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS", CGS_ARTIST_FILE, 1011));
    return maePlayerTeams[liRaceCarIndex];
}

// ---- shared halves of the pure-virtual lifecycle ------------------------------------------------
// The derived overrides call these by qualified name. The console inlines each into its callers and
// folds the identical results, so every derived vtable slot that only forwards here points at one
// shared body.

// Every slot back to no team and to position 8 (one past the last of an eight-car field).
bool BaseOnlineModeScoring::Prepare()
{
    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        maiPlayerPositions[liSlot] = KI_MAX_ACTIVE_RACE_CARS;
        maePlayerTeams[liSlot]     = GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }
    return true;
}

bool BaseOnlineModeScoring::Release()
{
    return true;
}

// Awards back to INVALID, teams back to NONE. The award variables and the positions are left alone.
void BaseOnlineModeScoring::ClearData()
{
    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        maOnlineAwards[liSlot] = E_ONLINE_AWARD_INVALID;
    }
    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        maePlayerTeams[liSlot] = GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }
}

void BaseOnlineModeScoring::Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars)
{
    UpdatePlayerTeams(lpScoringSystem, liNumberOfCars);
}

// The award, award-variable and team tables into the output (+0x20 / +0x40 / +0x60). The eliminations
// block at +0x00 is the road-rage scorer's to write.
void BaseOnlineModeScoring::WriteDataToOutput(OnlineScoringOutputInterface* lpOutput)
{
    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        lpOutput->maOnlineAwards[liSlot] = maOnlineAwards[liSlot];
    }
    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        lpOutput->maiOnlineAwardVariables[liSlot] = maiOnlineAwardVariables[liSlot];
    }
    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        lpOutput->maePlayerTeam[liSlot] = maePlayerTeams[liSlot];
    }
}

// ---- per-field tie-break comparators ------------------------------------------------------------
// Each derived qsort comparator chains these helpers together and folds ONE decisive comparison into
// the running *lpiResult, but only while *lpiResult is still 0 (so the first non-zero comparison in
// the chain wins -- later fields cannot override an already-decided ordering). qsort sign convention
// here: a -1 means "lArg1 sorts before lArg2", +1 means "after", 0 means "tie, defer to next field".

// X360 @ 0x823146B0. Tie-break on distance-to-finish, ASCENDING (nearer the finish sorts first):
// d1 < d2 -> -1, d1 == d2 -> 0, d1 > d2 -> +1. Float compares (the X360 IEEE form lowers the
// equal/less branch to the nested compare below). The lpiResult NULL guard fires its own assert
// (.cpp line 1028) but never early-returns.
void BaseOnlineModeScoring::CompareDistanceToFinish(f32 lfDistance1, f32 lfDistance2, s32* lpiResult)
{
    CGS_ASSERT(lpiResult != NULL, CGS_ASSERT_MESSAGE_AT("lpiResult != NULL", CGS_ARTIST_FILE, 1028));

    if (*lpiResult == 0)
    {
        if (lfDistance1 > lfDistance2)
        {
            *lpiResult = 1;
        }
        else if (lfDistance1 < lfDistance2)
        {
            *lpiResult = -1;
        }
        else
        {
            *lpiResult = 0;
        }
    }
}

// X360 @ 0x82314748. Tie-break on network player id, ASCENDING (lower id sorts first):
// id1 < id2 -> -1, id1 == id2 -> 0, id1 > id2 -> +1. NULL guard at .cpp line 1063.
void BaseOnlineModeScoring::CompareNetworkPlayerID(BrnNetwork::NetworkPlayerID lID1,
                                                   BrnNetwork::NetworkPlayerID lID2,
                                                   s32* lpiResult)
{
    CGS_ASSERT(lpiResult != NULL, CGS_ASSERT_MESSAGE_AT("lpiResult != NULL", CGS_ARTIST_FILE, 1063));

    if (*lpiResult == 0)
    {
        if (lID1 > lID2)
        {
            *lpiResult = 1;
        }
        else if (lID1 < lID2)
        {
            *lpiResult = -1;
        }
        else
        {
            *lpiResult = 0;
        }
    }
}

// X360 @ 0x823147C8. Tie-break on finish time, ASCENDING (faster finish sorts first):
// t1 < t2 -> -1, t1 == t2 -> 0, t1 > t2 -> +1. The two times must be strictly positive (a finished
// car always has a recorded time); both guards fire their own assert (.cpp lines 1098/1099) and the
// NULL guard at .cpp line 1097. Comparisons go through CgsSystem::Time::GetFloatVal() exactly as the
// X360 binary does (it materialises (f32)miSeconds + mfFraction for each side before comparing).
void BaseOnlineModeScoring::CompareFinishTime(const CgsSystem::Time* lpTime1,
                                              const CgsSystem::Time* lpTime2,
                                              s32* lpiResult)
{
    CGS_ASSERT(lpiResult != NULL, CGS_ASSERT_MESSAGE_AT("lpiResult != NULL", CGS_ARTIST_FILE, 1097));
    CGS_ASSERT(lpTime1->GetFloatVal() > 0.0f, CGS_ASSERT_MESSAGE_AT("lpPlayer1FinishTime->GetFloatVal() > 0.0f", CGS_ARTIST_FILE, 1098));
    CGS_ASSERT(lpTime2->GetFloatVal() > 0.0f, CGS_ASSERT_MESSAGE_AT("lpPlayer2FinishTime->GetFloatVal() > 0.0f", CGS_ARTIST_FILE, 1099));

    if (*lpiResult == 0)
    {
        const f32 lfTime1 = lpTime1->GetFloatVal();
        const f32 lfTime2 = lpTime2->GetFloatVal();

        if (lfTime1 > lfTime2)
        {
            *lpiResult = 1;
        }
        else if (lfTime1 < lfTime2)
        {
            *lpiResult = -1;
        }
        else
        {
            *lpiResult = 0;
        }
    }
}

// X360 @ 0x82314928. Tie-break on checkpoints reached, DESCENDING (more checkpoints sorts first):
// c1 > c2 -> -1, c1 == c2 -> 0, c1 < c2 -> +1. NULL guard at .cpp line 1134. (The X360 binary folds
// the equal/less branch into `*lpiResult = c1 < c2`, which yields +1 for less and 0 for equal.)
void BaseOnlineModeScoring::CompareCheckpointsReached(s32 liCheckpoints1, s32 liCheckpoints2,
                                                      s32* lpiResult)
{
    CGS_ASSERT(lpiResult != NULL,
        CGS_ASSERT_MESSAGE_AT("lpiResult != NULL", CGS_ARTIST_FILE, 1134 /* ARTIST 0x82314950 */));

    if (*lpiResult == 0)
    {
        if (liCheckpoints1 > liCheckpoints2)
        {
            *lpiResult = -1;
        }
        else
        {
            *lpiResult = (liCheckpoints1 < liCheckpoints2) ? 1 : 0;
        }
    }
}

// X360 @ 0x823149A8. Tie-break on takedowns, DESCENDING (more takedowns sorts first):
// t1 > t2 -> -1, t1 == t2 -> 0, t1 < t2 -> +1. NULL guard at .cpp line 1168. Same folded
// less/equal branch as CompareCheckpointsReached.
void BaseOnlineModeScoring::CompareTakedowns(s32 liTakedowns1, s32 liTakedowns2, s32* lpiResult)
{
    CGS_ASSERT(lpiResult != NULL,
        CGS_ASSERT_MESSAGE_AT("lpiResult != NULL", CGS_ARTIST_FILE, 1168 /* ARTIST 0x823149D0 */));

    if (*lpiResult == 0)
    {
        if (liTakedowns1 > liTakedowns2)
        {
            *lpiResult = -1;
        }
        else
        {
            *lpiResult = (liTakedowns1 < liTakedowns2) ? 1 : 0;
        }
    }
}

// X360 @ 0x82314A28. Tie-break on time-as-runner, DESCENDING (longer time-as-runner sorts first):
// t1 > t2 -> -1, t1 == t2 -> 0, t1 < t2 -> +1. Times are taken by value (per DWARF). NULL guard at
// .cpp line 1236. The X360 binary inlines the seconds-then-fraction lexicographic compare (the same
// ordering CgsSystem::Time::operator>/operator< implement) and lowers the t1<t2 result through a
// cntlzw bit test that just normalises the bool to 0/1.
void BaseOnlineModeScoring::CompareTimeAsRunner(CgsSystem::Time lTime1, CgsSystem::Time lTime2,
                                                s32* lpiResult)
{
    CGS_ASSERT(lpiResult != NULL,
        CGS_ASSERT_MESSAGE_AT("lpiResult != NULL", CGS_ARTIST_FILE, 1236 /* ARTIST 0x82314A50 */));

    if (*lpiResult == 0)
    {
        if (lTime1 > lTime2)
        {
            *lpiResult = -1;
        }
        else
        {
            *lpiResult = (lTime1 < lTime2) ? 1 : 0;
        }
    }
}

// ----------------------------------------------------------------------------------------------
// UpdatePlayerTeams -- X360 @ 0x823219B8 (DWARF BrnBaseOnlineModeScoring.h:207 / .cpp:205).
// Refresh the whole per-slot team table from the scoring system. PUBLIC and NON-virtual: the
// race-mode Update (BrnOnlineRaceModeScoring.cpp:18) and the road-rage-teams Update
// (BrnOnlineRoadRageModeScoring.cpp:568) both forward to it verbatim.
//
// ⚠️ NOT a virtual: the header's own vtable map runs slots 0..9 and this is not one of them
// (BrnBaseOnlineModeScoring.h:69), and it has a full standalone console export. Never stub it:
// an inert body would leave maePlayerTeams[] frozen at whatever ClearData seeded for every
// online team mode.
//
// EVIDENCE (0x823219B8, dumped this session):
//   * the entry assert is "lpScoringSystem" @ BrnBaseOnlineModeScoring.cpp:212;
//   * the loop is a fixed 0..KI_MAX_ACTIVE_RACE_CARS walk whose write target is `a1 + 68` with a
//     4-byte post-increment -- and +0x44 == 68 is maePlayerTeams (header :195, DWARF h:195);
//   * the per-slot READ is ScoringSystem::GetPlayerTeam INLINED, not a raw poke: the inlined
//     fragment carries GetPlayerTeam's OWN baked assert -- the index message at
//     BrnScoringSystem.cpp line 1549, byte-identical to the one the standalone
//     ScoringSystem::GetPlayerTeam @0x8231FDB8 fires -- then calls GetCarData and reads
//     `*(result + 316)`. +316 == 0x13C is CarData::mePlayerTeam, cross-attested by
//     SetPlayerTeam @0x8231FE38 writing `result[79]` (79*4 == 0x13C) to the same field.
//     De-inlined here to the real GetPlayerTeam call, so its assert is not duplicated.
//   * the trailing "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert (BurnoutConstants.h:39)
//     is the enum-iterator operator++'s own, i.e. the console's loop variable is an
//     EActiveRaceCarIndex. Written as the plain s32 walk + cast, the convention every sibling
//     body in this TU and in BrnScoringSystem_Standings.cpp already uses.
//
// NOTE liNumberOfCars is UNUSED, exactly as on the console: the export never touches r5 and the
// walk is unconditionally the full 8 slots. Parameter kept for the DWARF signature.
// ----------------------------------------------------------------------------------------------
void BaseOnlineModeScoring::UpdatePlayerTeams(const ScoringSystem* lpScoringSystem,
                                              s32 /*liNumberOfCars*/)
{
    CGS_ASSERT(lpScoringSystem != NULL, CGS_ASSERT_MESSAGE_AT("lpScoringSystem", CGS_ARTIST_FILE, 212));

    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        maePlayerTeams[liSlot] =
            lpScoringSystem->GetPlayerTeam(static_cast<EActiveRaceCarIndex>(liSlot));
    }
}
}

// FOLDED FROM BrnBaseOnlineModeScoring_wN1_01.cpp
// BaseOnlineModeScoring -- end-of-event network award rating (wave N1 partfile of
// BrnBaseOnlineModeScoring.cpp).
//
// AwardNetworkRatings gathers one NetworkAwardData row per active race car, then walks the awards in
// priority order: each award re-sorts the rows with its rating comparator and hands the leading row
// the award if that award's give test passes and the car has not already been awarded. At most
// KI_ONLINE_AWARD_MAX_AWARDS awards are handed out, one per car.

#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include <stdlib.h> // qsort
#include <cstring>  // memset

namespace BrnGameState
{
// Awards handed out per event.
const s32 KI_ONLINE_AWARD_MAX_AWARDS = 4;

// ---- award tables (indexed by EOnlineAwardID) ---------------------------------------------------
int (* const BaseOnlineModeScoring::maAwardRatingFunctions[E_ONLINE_AWARD_COUNT])(const void* lpData1,
                                                                                  const void* lpData2) =
{
    &BaseOnlineModeScoring::_RaceWinnerCompare,             // E_ONLINE_AWARD_RACE_WINNER
    &BaseOnlineModeScoring::_TakedownsForCompare,           // E_ONLINE_AWARD_TAKEDOWNS_FOR
    &BaseOnlineModeScoring::_TakedownsAgainstCompare,       // E_ONLINE_AWARD_TAKEDOWNS_AGAINST
    &BaseOnlineModeScoring::_MostCrashesCompare,            // E_ONLINE_AWARD_MOST_CRASHES
    &BaseOnlineModeScoring::_FastestLapCompare,             // E_ONLINE_AWARD_FASTEST_LAP
    &BaseOnlineModeScoring::_ShortestDistanceLapCompare,    // E_ONLINE_AWARD_SHORTEST_DISTANCE
    &BaseOnlineModeScoring::_LongestDistanceLapCompare,     // E_ONLINE_AWARD_LONGEST_DISTANCE
    &BaseOnlineModeScoring::_TimeInFirstPlaceCompare,       // E_ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE
    &BaseOnlineModeScoring::_TimeInLastPlaceCompare,        // E_ONLINE_AWARD_LONGEST_TIME_IN_LAST_PLACE
    &BaseOnlineModeScoring::_MostTimeBoostingAwardCompare,  // E_ONLINE_AWARD_TIME_SPENT_BOOSTING
    &BaseOnlineModeScoring::_LongestDriftCompare,           // E_ONLINE_AWARD_LONGEST_DRIFT
};

bool (* const BaseOnlineModeScoring::maGiveAwardFunctions[E_ONLINE_AWARD_COUNT])(
    const NetworkAwardData* lpaNetworkAwardData, s32 liNumberOfRaceCars) =
{
    &BaseOnlineModeScoring::_GiveRaceWinnerAward,           // E_ONLINE_AWARD_RACE_WINNER
    &BaseOnlineModeScoring::_GiveTakedownsForAward,         // E_ONLINE_AWARD_TAKEDOWNS_FOR
    &BaseOnlineModeScoring::_GiveTakedownsAgainstAward,     // E_ONLINE_AWARD_TAKEDOWNS_AGAINST
    &BaseOnlineModeScoring::_GiveMostCrashesAward,          // E_ONLINE_AWARD_MOST_CRASHES
    &BaseOnlineModeScoring::_GiveFastestLapAward,           // E_ONLINE_AWARD_FASTEST_LAP
    &BaseOnlineModeScoring::_GiveShortestDistanceAward,     // E_ONLINE_AWARD_SHORTEST_DISTANCE
    &BaseOnlineModeScoring::_GiveLongestDistanceAward,      // E_ONLINE_AWARD_LONGEST_DISTANCE
    &BaseOnlineModeScoring::_GiveTimeInFirstPlaceAward,     // E_ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE
    &BaseOnlineModeScoring::_GiveTimeInLastPlaceAward,      // E_ONLINE_AWARD_LONGEST_TIME_IN_LAST_PLACE
    &BaseOnlineModeScoring::_GiveMostTimeBoostingAward,     // E_ONLINE_AWARD_TIME_SPENT_BOOSTING
    &BaseOnlineModeScoring::_GiveLongestDriftAward,         // E_ONLINE_AWARD_LONGEST_DRIFT
};

const EOnlineAwardID BaseOnlineModeScoring::maAwardPriorities[E_ONLINE_AWARD_COUNT] =
{
    E_ONLINE_AWARD_RACE_WINNER,
    E_ONLINE_AWARD_TAKEDOWNS_FOR,
    E_ONLINE_AWARD_TAKEDOWNS_AGAINST,
    E_ONLINE_AWARD_MOST_CRASHES,
    E_ONLINE_AWARD_FASTEST_LAP,
    E_ONLINE_AWARD_SHORTEST_DISTANCE,
    E_ONLINE_AWARD_LONGEST_DISTANCE,
    E_ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE,
    E_ONLINE_AWARD_LONGEST_TIME_IN_LAST_PLACE,
    E_ONLINE_AWARD_TIME_SPENT_BOOSTING,
    E_ONLINE_AWARD_LONGEST_DRIFT,
};

// ---- AwardNetworkRatings ------------------------------------------------------------------------
// Every slot's award is reset to INVALID first; the award variables are only written for the cars
// that win something. Nothing is awarded unless at least two cars are active. All eight rows are
// gathered, but only the first liNumActiveRaceCars take part in the sorts.
void BaseOnlineModeScoring::AwardNetworkRatings(const ScoringSystem* lpScoringSystem,
                                                u32 /*luNumActiveRaceCars*/)
{
    NetworkAwardData laNetworkAwardData[KI_MAX_ACTIVE_RACE_CARS];
    CgsContainers::BitArray<KI_MAX_ACTIVE_RACE_CARS> lAwardedPlayers;
    lAwardedPlayers.UnSetAll();

    memset(laNetworkAwardData, -1, sizeof(laNetworkAwardData));

    for (s32 liSlot = 0; liSlot < KI_MAX_ACTIVE_RACE_CARS; ++liSlot)
    {
        maOnlineAwards[liSlot] = E_ONLINE_AWARD_INVALID;
    }

    CGS_ASSERT(lpScoringSystem != NULL, CGS_ASSERT_MESSAGE_AT("lpScoringSystem", CGS_ARTIST_FILE, 263));

    const s32 liNumActiveRaceCars = static_cast<s32>(lpScoringSystem->GetNumberOfActiveCars());
    CGS_ASSERT(liNumActiveRaceCars > 0, CGS_ASSERT_MESSAGE_AT("liNumActiveRaceCars > 0", CGS_ARTIST_FILE, 268));

    if (liNumActiveRaceCars > 1)
    {
        for (EActiveRaceCarIndex leRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leRaceCarIndex++)
        {
            NetworkAwardData& lrAwardData = laNetworkAwardData[leRaceCarIndex];

            lrAwardData.meRaceCarIndex             = leRaceCarIndex;
            lrAwardData.miRaceCarPosition          = maiPlayerPositions[leRaceCarIndex];
            lrAwardData.miTakedownsFor             = lpScoringSystem->GetNumberOfTakedowns(leRaceCarIndex);
            lrAwardData.miTakedownsAgainst         = lpScoringSystem->GetNumberOfTakedownsAgainst(leRaceCarIndex);
            lrAwardData.miNumberOfCrashes          = lpScoringSystem->GetNumberOfCrashes(leRaceCarIndex);
            lrAwardData.mFastestLap                = lpScoringSystem->GetRaceCarFastestLapTime(leRaceCarIndex);
            lrAwardData.mfDistanceDriven           = lpScoringSystem->GetTotalDistanceDriven(leRaceCarIndex);
            lrAwardData.mbFinishedRace             = lpScoringSystem->IsNetworkCarsDistanceDrivenValid(leRaceCarIndex);
            lrAwardData.mTimeInFirstPlace          = lpScoringSystem->GetTimeSpentInFirstPlace(leRaceCarIndex);
            lrAwardData.mTimeInLastPlace           = lpScoringSystem->GetTimeSpentInLastPlace(leRaceCarIndex);
            lrAwardData.mTimeBoosting              = lpScoringSystem->GetTimeSpentBoosting(leRaceCarIndex);
            lrAwardData.mfLongestDrift             = lpScoringSystem->GetLongestDrift(leRaceCarIndex);
            lrAwardData.miOverallStandingsPosition = lpScoringSystem->GetRaceCarStandingsPosition(leRaceCarIndex);
        }

        s32 liAwardPriorityIndex  = 0;
        s32 liNumberOfAwardsGiven = 0;
        while ((liNumberOfAwardsGiven < KI_ONLINE_AWARD_MAX_AWARDS) &&
               (lAwardedPlayers.GetFirstZeroBit() != lAwardedPlayers.KI_INVALID_BITINDEX) &&
               (liAwardPriorityIndex < E_ONLINE_AWARD_COUNT))
        {
            CGS_ASSERT(liAwardPriorityIndex >= 0, "liAwardPriorityIndex >= 0");

            const EOnlineAwardID leAwardID = maAwardPriorities[liAwardPriorityIndex];
            CGS_ASSERT((leAwardID < E_ONLINE_AWARD_COUNT) && (leAwardID != E_ONLINE_AWARD_INVALID),
                       "( leAwardID < E_ONLINE_AWARD_COUNT ) && ( leAwardID != E_ONLINE_AWARD_INVALID )");

            qsort(laNetworkAwardData, liNumActiveRaceCars, sizeof(NetworkAwardData),
                  maAwardRatingFunctions[leAwardID]);

            const EActiveRaceCarIndex leRaceCarIndex = laNetworkAwardData[0].meRaceCarIndex;
            CGS_ASSERT(E_ACTIVE_RACE_CAR_INDEX_INVALID != leRaceCarIndex,
                       "E_ACTIVE_RACE_CAR_INDEX_INVALID != leRaceCarIndex");

            // The bit array's own index check (message streamed with the index on the console).
            if (!lAwardedPlayers.IsBitSet(static_cast<u32>(leRaceCarIndex)))
            {
                if (maGiveAwardFunctions[leAwardID](laNetworkAwardData, liNumActiveRaceCars))
                {
                    lAwardedPlayers.SetBit(static_cast<u32>(leRaceCarIndex));

                    maOnlineAwards[leRaceCarIndex]          = leAwardID;
                    maiOnlineAwardVariables[leRaceCarIndex] = GetAwardParameter(laNetworkAwardData, leAwardID);
                    ++liNumberOfAwardsGiven;
                }
            }

            ++liAwardPriorityIndex;
        }
    }
}

// The value shown next to an award: the standings position, a takedown / crash count, or the
// longest drift truncated to an integer. The other awards carry no parameter.
s32 BaseOnlineModeScoring::GetAwardParameter(const NetworkAwardData* lpAwardData, EOnlineAwardID leAwardID)
{
    CGS_ASSERT(lpAwardData != NULL, CGS_ASSERT_MESSAGE_AT("lpAwardData", CGS_ARTIST_FILE, 339));

    switch (leAwardID)
    {
    case E_ONLINE_AWARD_RACE_WINNER:
        return lpAwardData->miOverallStandingsPosition;
    case E_ONLINE_AWARD_TAKEDOWNS_FOR:
        return lpAwardData->miTakedownsFor;
    case E_ONLINE_AWARD_TAKEDOWNS_AGAINST:
        return lpAwardData->miTakedownsAgainst;
    case E_ONLINE_AWARD_MOST_CRASHES:
        return lpAwardData->miNumberOfCrashes;
    case E_ONLINE_AWARD_LONGEST_DRIFT:
        return static_cast<s32>(lpAwardData->mfLongestDrift);
    default:
        return 0;
    }
}

// ---- rating comparators (qsort; -1 sorts lpData1 first) -----------------------------------------

// Ascending finishing position. Two cars never share a position.
int BaseOnlineModeScoring::_RaceWinnerCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 392 /* ARTIST 0x823138AC */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 393 /* ARTIST 0x823138D0 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miRaceCarPosition >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->miRaceCarPosition >= 0",
            CGS_ARTIST_FILE, 399 /* ARTIST 0x823138F8 */));
    CGS_ASSERT(lpPlayer1->miRaceCarPosition < KI_MAX_ACTIVE_RACE_CARS,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->miRaceCarPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            CGS_ARTIST_FILE, 400 /* ARTIST 0x82313920 */));
    CGS_ASSERT(lpPlayer2->miRaceCarPosition >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer2->miRaceCarPosition >= 0",
            CGS_ARTIST_FILE, 401 /* ARTIST 0x82313948 */));
    CGS_ASSERT(lpPlayer2->miRaceCarPosition < KI_MAX_ACTIVE_RACE_CARS,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer2->miRaceCarPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS",
            CGS_ARTIST_FILE, 402 /* ARTIST 0x82313970 */));

    if (lpPlayer1->miRaceCarPosition > lpPlayer2->miRaceCarPosition)
    {
        return 1;
    }
    if (lpPlayer1->miRaceCarPosition < lpPlayer2->miRaceCarPosition)
    {
        return -1;
    }

    CGS_ASSERT(lpPlayer1->miRaceCarPosition != lpPlayer2->miRaceCarPosition,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->miRaceCarPosition != lpPlayer2->miRaceCarPosition",
            CGS_ARTIST_FILE, 415 /* ARTIST 0x823139C0 */));
    return 0;
}

// Most takedowns first.
int BaseOnlineModeScoring::_TakedownsForCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 433 /* ARTIST 0x82313A0C */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 434 /* ARTIST 0x82313A30 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miTakedownsFor >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->miTakedownsFor >= 0", CGS_ARTIST_FILE, 439 /* ARTIST 0x82313A58 */));
    CGS_ASSERT(lpPlayer2->miTakedownsFor >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer2->miTakedownsFor >= 0", CGS_ARTIST_FILE, 440 /* ARTIST 0x82313A80 */));

    if (lpPlayer1->miTakedownsFor > lpPlayer2->miTakedownsFor)
    {
        return -1;
    }
    return (lpPlayer1->miTakedownsFor < lpPlayer2->miTakedownsFor) ? 1 : 0;
}

// Most takedowns suffered first.
int BaseOnlineModeScoring::_TakedownsAgainstCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 468 /* ARTIST 0x82313AF4 */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 469 /* ARTIST 0x82313B18 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miTakedownsAgainst >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->miTakedownsAgainst >= 0", CGS_ARTIST_FILE, 474 /* ARTIST 0x82313B40 */));
    CGS_ASSERT(lpPlayer2->miTakedownsAgainst >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer2->miTakedownsAgainst >= 0", CGS_ARTIST_FILE, 475 /* ARTIST 0x82313B68 */));

    if (lpPlayer1->miTakedownsAgainst > lpPlayer2->miTakedownsAgainst)
    {
        return -1;
    }
    return (lpPlayer1->miTakedownsAgainst < lpPlayer2->miTakedownsAgainst) ? 1 : 0;
}

// Most crashes first.
int BaseOnlineModeScoring::_MostCrashesCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 503 /* ARTIST 0x82313BDC */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 504 /* ARTIST 0x82313C00 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miNumberOfCrashes >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->miNumberOfCrashes >= 0", CGS_ARTIST_FILE, 509 /* ARTIST 0x82313C28 */));
    CGS_ASSERT(lpPlayer2->miNumberOfCrashes >= 0,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer2->miNumberOfCrashes >= 0", CGS_ARTIST_FILE, 510 /* ARTIST 0x82313C50 */));

    if (lpPlayer1->miNumberOfCrashes > lpPlayer2->miNumberOfCrashes)
    {
        return -1;
    }
    return (lpPlayer1->miNumberOfCrashes < lpPlayer2->miNumberOfCrashes) ? 1 : 0;
}

// Shortest fastest-lap time first.
int BaseOnlineModeScoring::_FastestLapCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 538 /* ARTIST 0x82313CC4 */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 539 /* ARTIST 0x82313CE8 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (lpPlayer1->mFastestLap > lpPlayer2->mFastestLap)
    {
        return 1;
    }
    return (lpPlayer1->mFastestLap < lpPlayer2->mFastestLap) ? -1 : 0;
}

// Cars whose distance is valid first, then the shortest distance driven.
int BaseOnlineModeScoring::_ShortestDistanceLapCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 569 /* ARTIST 0x82313DF4 */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 570 /* ARTIST 0x82313E18 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (!lpPlayer1->mbFinishedRace)
    {
        return 1;
    }
    if (!lpPlayer2->mbFinishedRace)
    {
        return -1;
    }
    if (lpPlayer1->mfDistanceDriven < lpPlayer2->mfDistanceDriven)
    {
        return -1;
    }
    if (lpPlayer1->mfDistanceDriven > lpPlayer2->mfDistanceDriven)
    {
        return 1;
    }

    CGS_ASSERT(lpPlayer1->mfDistanceDriven == lpPlayer2->mfDistanceDriven,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->mfDistanceDriven == lpPlayer2->mfDistanceDriven",
            CGS_ARTIST_FILE, 595 /* ARTIST 0x82313E84 */));
    return 0;
}

// Cars whose distance is valid first, then the longest distance driven.
int BaseOnlineModeScoring::_LongestDistanceLapCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 612 /* ARTIST 0x82313ED4 */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 613 /* ARTIST 0x82313EF8 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (!lpPlayer1->mbFinishedRace)
    {
        return 1;
    }
    if (!lpPlayer2->mbFinishedRace)
    {
        return -1;
    }
    if (lpPlayer1->mfDistanceDriven > lpPlayer2->mfDistanceDriven)
    {
        return -1;
    }
    if (lpPlayer1->mfDistanceDriven < lpPlayer2->mfDistanceDriven)
    {
        return 1;
    }

    CGS_ASSERT(lpPlayer1->mfDistanceDriven == lpPlayer2->mfDistanceDriven,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayer1->mfDistanceDriven == lpPlayer2->mfDistanceDriven",
            CGS_ARTIST_FILE, 638 /* ARTIST 0x82313F64 */));
    return 0;
}

// Longest time in first place first.
int BaseOnlineModeScoring::_TimeInFirstPlaceCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 655 /* ARTIST 0x82313FB4 */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 656 /* ARTIST 0x82313FD8 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (lpPlayer1->mTimeInFirstPlace > lpPlayer2->mTimeInFirstPlace)
    {
        return -1;
    }
    return (lpPlayer1->mTimeInFirstPlace < lpPlayer2->mTimeInFirstPlace) ? 1 : 0;
}

// Longest time in last place first.
int BaseOnlineModeScoring::_TimeInLastPlaceCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 686 /* ARTIST 0x823140CC */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 687 /* ARTIST 0x823140F0 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (lpPlayer1->mTimeInLastPlace > lpPlayer2->mTimeInLastPlace)
    {
        return -1;
    }
    return (lpPlayer1->mTimeInLastPlace < lpPlayer2->mTimeInLastPlace) ? 1 : 0;
}

// Longest time boosting first.
int BaseOnlineModeScoring::_MostTimeBoostingAwardCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 717 /* ARTIST 0x823141E4 */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 718 /* ARTIST 0x82314208 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (lpPlayer1->mTimeBoosting > lpPlayer2->mTimeBoosting)
    {
        return -1;
    }
    return (lpPlayer1->mTimeBoosting < lpPlayer2->mTimeBoosting) ? 1 : 0;
}

// Descending, but on mfDistanceDriven, not mfLongestDrift: that is what the console reads (+0x1C).
// The matching give test and award parameter do use mfLongestDrift.
int BaseOnlineModeScoring::_LongestDriftCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData1", CGS_ARTIST_FILE, 748 /* ARTIST 0x823142FC */));
    CGS_ASSERT(lpData2 != NULL,
        CGS_ASSERT_MESSAGE_AT("lpData2", CGS_ARTIST_FILE, 749 /* ARTIST 0x82314320 */));

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    if (lpPlayer1->mfDistanceDriven > lpPlayer2->mfDistanceDriven)
    {
        return -1;
    }
    return (lpPlayer1->mfDistanceDriven < lpPlayer2->mfDistanceDriven) ? 1 : 0;
}

// ---- give-award tests (called on the sorted rows; row 0 is the candidate) -----------------------

// Always awarded; the leader must be strictly ahead of the runner-up.
bool BaseOnlineModeScoring::_GiveRaceWinnerAward(const NetworkAwardData* lpaNetworkAwardData,
                                                 s32 liNumberOfRaceCars)
{
    CGS_ASSERT(lpaNetworkAwardData[0].miRaceCarPosition < lpaNetworkAwardData[1].miRaceCarPosition ||
                   liNumberOfRaceCars < 2,
        CGS_ASSERT_MESSAGE_AT(
            "lpaNetworkAwardData[0].miRaceCarPosition < lpaNetworkAwardData[1].miRaceCarPosition || liNumberOfRaceCars < 2",
            CGS_ARTIST_FILE, 779 /* ARTIST 0x82314394 */));
    return true;
}

bool BaseOnlineModeScoring::_GiveTakedownsForAward(const NetworkAwardData* lpaNetworkAwardData,
                                                   s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].miTakedownsFor > lpaNetworkAwardData[1].miTakedownsFor;
}

bool BaseOnlineModeScoring::_GiveTakedownsAgainstAward(const NetworkAwardData* lpaNetworkAwardData,
                                                       s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].miTakedownsAgainst > lpaNetworkAwardData[1].miTakedownsAgainst;
}

bool BaseOnlineModeScoring::_GiveMostCrashesAward(const NetworkAwardData* lpaNetworkAwardData,
                                                  s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].miNumberOfCrashes > lpaNetworkAwardData[1].miNumberOfCrashes;
}

bool BaseOnlineModeScoring::_GiveFastestLapAward(const NetworkAwardData* lpaNetworkAwardData,
                                                 s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mFastestLap > lpaNetworkAwardData[1].mFastestLap;
}

// The shortest- and longest-distance tests are the same body (the console folds them into one).
bool BaseOnlineModeScoring::_GiveShortestDistanceAward(const NetworkAwardData* lpaNetworkAwardData,
                                                       s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mbFinishedRace;
}

bool BaseOnlineModeScoring::_GiveLongestDistanceAward(const NetworkAwardData* lpaNetworkAwardData,
                                                      s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mbFinishedRace;
}

bool BaseOnlineModeScoring::_GiveTimeInLastPlaceAward(const NetworkAwardData* lpaNetworkAwardData,
                                                      s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mTimeInLastPlace > lpaNetworkAwardData[1].mTimeInLastPlace;
}

bool BaseOnlineModeScoring::_GiveTimeInFirstPlaceAward(const NetworkAwardData* lpaNetworkAwardData,
                                                       s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mTimeInFirstPlace > lpaNetworkAwardData[1].mTimeInFirstPlace;
}

bool BaseOnlineModeScoring::_GiveMostTimeBoostingAward(const NetworkAwardData* lpaNetworkAwardData,
                                                       s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mTimeBoosting > lpaNetworkAwardData[1].mTimeBoosting;
}

bool BaseOnlineModeScoring::_GiveLongestDriftAward(const NetworkAwardData* lpaNetworkAwardData,
                                                   s32 /*liNumberOfRaceCars*/)
{
    return lpaNetworkAwardData[0].mfLongestDrift > lpaNetworkAwardData[1].mfLongestDrift;
}
}

#undef CGS_ARTIST_FILE
#undef CGS_ARTIST_BRNBASEONLINEMODESCORING_FILE
