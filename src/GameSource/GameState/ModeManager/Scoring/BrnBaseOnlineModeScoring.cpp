#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
// X360 @ 0x823106F8. Finishing position recorded for active-race-car slot liRaceCarIndex. The two
// bounds checks fire as the build had them: two SEPARATE Begin/Fire/End assert sequences (lower-bound
// then upper-bound), each with its own baked-in message string and source line (343 / 344).
s32 BaseOnlineModeScoring::GetPlayerPosition(s32 liRaceCarIndex)
{
    CGS_ASSERT(liRaceCarIndex >= 0, "liRaceCarIndex >= 0");
    CGS_ASSERT(liRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS, "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    return maiPlayerPositions[liRaceCarIndex];
}

// X360 @ 0x82310770. Store the finishing position liPlayerPosition for active-race-car slot
// liRaceCarIndex. Four independent bounds asserts (the X360 build did NOT fold them into one combined
// check -- each fires its own distinct line number 356/357/359/360); the asserts never early-return,
// so the table write executes unconditionally even on a failed bound.
void BaseOnlineModeScoring::SetPlayerPosition(s32 liRaceCarIndex, s32 liPlayerPosition)
{
    CGS_ASSERT(liRaceCarIndex >= 0, "liRaceCarIndex >= 0");
    CGS_ASSERT(liRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(liPlayerPosition >= 0, "liPlayerPosition >= 0");
    CGS_ASSERT(liPlayerPosition < E_ACTIVE_RACE_CAR_INDEX_COUNT, "liPlayerPosition < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    maiPlayerPositions[liRaceCarIndex] = liPlayerPosition;
}

// X360 @ 0x82314638. Team currently assigned to active-race-car slot liRaceCarIndex (read from the
// per-slot maePlayerTeams[] array). Virtual. Two SEPARATE bounds asserts (lower bound at .cpp line
// 1010, upper bound at line 1011), each with its own verbatim message string and the exact baked-in
// .cpp path (note: this function's asserts use the .cpp path, not the .h path the sibling accessors use).
GameStateModuleIO::EPlayerTeam
BaseOnlineModeScoring::GetCurrentPlayerTeam(s32 liRaceCarIndex)
{
    CGS_ASSERT(liRaceCarIndex >= 0, "liRaceCarIndex >= 0");
    CGS_ASSERT(liRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS, "liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    return maePlayerTeams[liRaceCarIndex];
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
    CGS_ASSERT(lpiResult != NULL, "lpiResult != NULL");

    if (*lpiResult == 0)
    {
        if (lfDistance1 < lfDistance2)
        {
            *lpiResult = -1;
        }
        else if (lfDistance1 > lfDistance2)
        {
            *lpiResult = 1;
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
    CGS_ASSERT(lpiResult != NULL, "lpiResult != NULL");

    if (*lpiResult == 0)
    {
        if (lID1 < lID2)
        {
            *lpiResult = -1;
        }
        else if (lID1 > lID2)
        {
            *lpiResult = 1;
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
    CGS_ASSERT(lpiResult != NULL, "lpiResult != NULL");
    CGS_ASSERT(lpTime1->GetFloatVal() > 0.0f, "lpPlayer1FinishTime->GetFloatVal() > 0.0f");
    CGS_ASSERT(lpTime2->GetFloatVal() > 0.0f, "lpPlayer2FinishTime->GetFloatVal() > 0.0f");

    if (*lpiResult == 0)
    {
        const f32 lfTime1 = lpTime1->GetFloatVal();
        const f32 lfTime2 = lpTime2->GetFloatVal();

        if (lfTime1 < lfTime2)
        {
            *lpiResult = -1;
        }
        else if (lfTime1 > lfTime2)
        {
            *lpiResult = 1;
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
    CGS_ASSERT(lpiResult != NULL, "lpiResult != NULL");

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
    CGS_ASSERT(lpiResult != NULL, "lpiResult != NULL");

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
    CGS_ASSERT(lpiResult != NULL, "lpiResult != NULL");

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
}
