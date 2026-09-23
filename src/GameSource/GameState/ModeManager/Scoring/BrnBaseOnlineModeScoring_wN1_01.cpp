// BaseOnlineModeScoring -- end-of-event network award rating (wave N1 partfile of
// BrnBaseOnlineModeScoring.cpp).
//
// AwardNetworkRatings gathers one NetworkAwardData row per active race car, then walks the awards in
// priority order: each award re-sorts the rows with its rating comparator and hands the leading row
// the award if that award's give test passes and the car has not already been awarded. At most
// KI_ONLINE_AWARD_MAX_AWARDS awards are handed out, one per car.

#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
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

    CGS_ASSERT(lpScoringSystem != NULL, "lpScoringSystem");

    const s32 liNumActiveRaceCars = static_cast<s32>(lpScoringSystem->GetNumberOfActiveCars());
    CGS_ASSERT(liNumActiveRaceCars > 0, "liNumActiveRaceCars > 0");

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
            CGS_ASSERT(static_cast<u32>(leRaceCarIndex) < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS),
                       "invalid index : ");
            if (!lAwardedPlayers.IsBitSet(static_cast<u32>(leRaceCarIndex)))
            {
                if (maGiveAwardFunctions[leAwardID](laNetworkAwardData, liNumActiveRaceCars))
                {
                    CGS_ASSERT(static_cast<u32>(leRaceCarIndex) < static_cast<u32>(KI_MAX_ACTIVE_RACE_CARS),
                               "Index: ");
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
    CGS_ASSERT(lpAwardData != NULL, "lpAwardData");

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
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miRaceCarPosition >= 0, "lpPlayer1->miRaceCarPosition >= 0");
    CGS_ASSERT(lpPlayer1->miRaceCarPosition < KI_MAX_ACTIVE_RACE_CARS,
               "lpPlayer1->miRaceCarPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(lpPlayer2->miRaceCarPosition >= 0, "lpPlayer2->miRaceCarPosition >= 0");
    CGS_ASSERT(lpPlayer2->miRaceCarPosition < KI_MAX_ACTIVE_RACE_CARS,
               "lpPlayer2->miRaceCarPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    if (lpPlayer1->miRaceCarPosition > lpPlayer2->miRaceCarPosition)
    {
        return 1;
    }
    if (lpPlayer1->miRaceCarPosition < lpPlayer2->miRaceCarPosition)
    {
        return -1;
    }

    CGS_ASSERT(lpPlayer1->miRaceCarPosition != lpPlayer2->miRaceCarPosition,
               "lpPlayer1->miRaceCarPosition != lpPlayer2->miRaceCarPosition");
    return 0;
}

// Most takedowns first.
int BaseOnlineModeScoring::_TakedownsForCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miTakedownsFor >= 0, "lpPlayer1->miTakedownsFor >= 0");
    CGS_ASSERT(lpPlayer2->miTakedownsFor >= 0, "lpPlayer2->miTakedownsFor >= 0");

    if (lpPlayer1->miTakedownsFor > lpPlayer2->miTakedownsFor)
    {
        return -1;
    }
    return (lpPlayer1->miTakedownsFor < lpPlayer2->miTakedownsFor) ? 1 : 0;
}

// Most takedowns suffered first.
int BaseOnlineModeScoring::_TakedownsAgainstCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miTakedownsAgainst >= 0, "lpPlayer1->miTakedownsAgainst >= 0");
    CGS_ASSERT(lpPlayer2->miTakedownsAgainst >= 0, "lpPlayer2->miTakedownsAgainst >= 0");

    if (lpPlayer1->miTakedownsAgainst > lpPlayer2->miTakedownsAgainst)
    {
        return -1;
    }
    return (lpPlayer1->miTakedownsAgainst < lpPlayer2->miTakedownsAgainst) ? 1 : 0;
}

// Most crashes first.
int BaseOnlineModeScoring::_MostCrashesCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

    const NetworkAwardData* lpPlayer1 = static_cast<const NetworkAwardData*>(lpData1);
    const NetworkAwardData* lpPlayer2 = static_cast<const NetworkAwardData*>(lpData2);

    CGS_ASSERT(lpPlayer1->miNumberOfCrashes >= 0, "lpPlayer1->miNumberOfCrashes >= 0");
    CGS_ASSERT(lpPlayer2->miNumberOfCrashes >= 0, "lpPlayer2->miNumberOfCrashes >= 0");

    if (lpPlayer1->miNumberOfCrashes > lpPlayer2->miNumberOfCrashes)
    {
        return -1;
    }
    return (lpPlayer1->miNumberOfCrashes < lpPlayer2->miNumberOfCrashes) ? 1 : 0;
}

// Shortest fastest-lap time first.
int BaseOnlineModeScoring::_FastestLapCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
               "lpPlayer1->mfDistanceDriven == lpPlayer2->mfDistanceDriven");
    return 0;
}

// Cars whose distance is valid first, then the longest distance driven.
int BaseOnlineModeScoring::_LongestDistanceLapCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
               "lpPlayer1->mfDistanceDriven == lpPlayer2->mfDistanceDriven");
    return 0;
}

// Longest time in first place first.
int BaseOnlineModeScoring::_TimeInFirstPlaceCompare(const void* lpData1, const void* lpData2)
{
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
    CGS_ASSERT(lpData1 != NULL, "lpData1");
    CGS_ASSERT(lpData2 != NULL, "lpData2");

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
               "lpaNetworkAwardData[0].miRaceCarPosition < lpaNetworkAwardData[1].miRaceCarPosition || liNumberOfRaceCars < 2");
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
