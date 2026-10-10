#include "types.hpp"

#include <stdlib.h> // qsort

#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"     // Array<E*RivalryCompare,N> (the Add* candidate lists)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf (the message parameter)
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"          // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"   // CgsSystem::DateAndTime (the recent-activity test)
#include "GameSource/GameState/BrnGameStateModule.h"                     // GameStateModule::GetLocalPlayerNetworkID / GetPlayerActiveRaceCarIndex
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem::GetPointsLeader / GetNumberOfNetworkPlayersStillConnected

// ===========================================================================
// BrnGameState::OnlineFlybyManager -- online flyby / rival-rating subsystem.
//
// Twelve functions recovered from the X360 ARTIST build. Every per-player record lookup below is
// the interface's GetPlayerStatusDataByPlayerID linear search, which the console inlines at each
// site: walk the first miNumPlayers records comparing mNetworkPlayerID against the wanted id; on a
// miss the record pointer is null and the caller fires its own "lpPlayerStatusData" /
// "lpRelationship" assert.
// ===========================================================================

namespace BrnGameState
{

// ---- weighting constants (BrnGameStateOnlineFlybyManager.cpp:29..) ----
static const s32 KI_WEIGHTING_MARK_TARGET            = 40;
static const s32 KI_WEIGHTING_MARK_MARKED_ME         = 30; // +0x1E added when *this* player is the local player's target
static const s32 KI_WEIGHTING_POINTS_LEADER          = 50;
static const s32 KI_WEIGHTING_NEW_RELATIONSHIP       = 9;  // E_NEW (no live activity yet)
static const s32 KI_WEIGHTING_LIVE_RELATIONSHIP      = 8;  // E_ONGOING (has live activity)
static const s32 KI_WEIGHTING_RECENT                 = 3;
static const s32 KI_RECENT_ACTIVITY_SECONDS          = 300;

// ---- the pre-race rivalry message tables, as the image holds them. Rows the console leaves
// short end in null pairs; CountAvailable* stop at the first null plural id. ----
const CombinedStringID KA_NEW_RIVALRY_MESSAGES[4][OnlineFlybyManager::E_MESSAGE_STYLE_COUNT][KI_MAX_MESSAGES] =
{
    {   // E_NEW_RIVALRY_CURRENT_STATUS
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_NEW_RIVALRY_YOUR_UP_BY_N_TAKEDOWN", "PRERACE_NEW_RIVALRY_YOUR_UP_BY_N_TAKEDOWN_S" },
            { "PRERACE_NEW_RIVALRY_TAKEDOWN_BEHIND", "PRERACE_NEW_RIVALRY_TAKEDOWN_BEHIND" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_NEW_RIVALRY_TAKEDOWN_AHEAD", "PRERACE_NEW_RIVALRY_TAKEDOWN_AHEAD_S" },
            { "PRERACE_NEW_RIVALRY_LEADING", "PRERACE_NEW_RIVALRY_LEADING_S" },
            { "PRERACE_NEW_RIVALRY_INFRONT", "PRERACE_NEW_RIVALRY_INFRONT_S" },
            { "PRERACE_NEW_RIVALRY_AHEAD", "PRERACE_NEW_RIVALRY_AHEAD_S" },
            { NULL, NULL },
        },
    },
    {   // E_NEW_RIVALRY_SCORE_SETTLED
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_NEW_RIVALRY_SCORE_SETTLED", "PRERACE_NEW_RIVALRY_SCORE_SETTLED_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_NEW_RIVALRY_THEY_SCORE_SETTLED", "PRERACE_NEW_RIVALRY_THEY_SCORE_SETTLED_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_NEW_RIVALRY_EVENTS
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_NEW_RIVALRY_BATTLED_N_TIMES", "PRERACE_NEW_RIVALRY_BATTLED_N_TIMES_S" },
            { "PRERACE_NEW_RIVALRY_RAGED_N_TIMES", "PRERACE_NEW_RIVALRY_RAGED_N_TIMES_S" },
            { "PRERACE_NEW_RIVALRY_CLASHED_N_TIMES", "PRERACE_NEW_RIVALRY_CLASHED_N_TIMES_S" },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_NEW_RIVALRY_BATTLED_N_TIMES", "PRERACE_NEW_RIVALRY_BATTLED_N_TIMES_S" },
            { "PRERACE_NEW_RIVALRY_RAGED_N_TIMES", "PRERACE_NEW_RIVALRY_RAGED_N_TIMES_S" },
            { "PRERACE_NEW_RIVALRY_CLASHED_N_TIMES", "PRERACE_NEW_RIVALRY_CLASHED_N_TIMES_S" },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_NEW_RIVALRY_LONGEST_STREAK
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_NEW_YOUR_ON_STREAK", "PRERACE_NEW_YOUR_ON_STREAK" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_NEW_STREAK_ON_YOU", "PRERACE_NEW_STREAK_ON_YOU" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
};

const CombinedStringID KA_ONGOING_RIVALRY_MESSAGES[6][OnlineFlybyManager::E_MESSAGE_STYLE_COUNT][KI_MAX_MESSAGES] =
{
    {   // E_ONGOING_RIVALRY_MUGSHOTS_COLLECTED
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_ONGOING_RIVALRY_COLLECTED_YOUR_MUGSHOT", "PRERACE_ONGOING_RIVALRY_COLLECTED_YOUR_MUGSHOT_S" },
            { "PRERACE_ONGOING_RIVALRY_YOU_APPEAR_IN_GALLERY", "PRERACE_ONGOING_RIVALRY_YOU_APPEAR_IN_GALLERY_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_ONGOING_RIVALRY_COLLECTED_THEIR_MUGSHOT", "PRERACE_ONGOING_RIVALRY_COLLECTED_THEIR_MUGSHOT_S" },
            { "PRERACE_ONGOING_RIVALRY_THEY_APPEAR_IN_GALLERY", "PRERACE_ONGOING_RIVALRY_THEY_APPEAR_IN_GALLERY_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_ONGOING_RIVALRY_MARKS
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_ONGOING_RIVALRY_MARKED_YOU_N_TIME", "PRERACE_ONGOING_RIVALRY_MARKED_YOU_N_TIME_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_ONGOING_RIVALRY_MARKED_THEM_N_TIME", "PRERACE_ONGOING_RIVALRY_MARKED_THEM_N_TIME_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_ONGOING_RIVALRY_LONGEST_STREAK
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_ONGOING_RIVALRY_LONGEST_STREAK_ON_THEM", "PRERACE_ONGOING_RIVALRY_LONGEST_STREAK_ON_THEM_S" },
            { "PRERACE_ONGOING_RIVALRY_ROLL_AGAINST_THEM", "PRERACE_ONGOING_RIVALRY_ROLL_AGAINST_THEM_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_ONGOING_RIVALRY_LONGEST_STREAK_ON_YOU", "PRERACE_ONGOING_RIVALRY_LONGEST_STREAK_ON_YOU_S" },
            { "PRERACE_ONGOING_RIVALRY_ROLL_AGAINST_YOU", "PRERACE_ONGOING_RIVALRY_ROLL_AGAINST_YOU_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_ONGOING_RIVALRY_WINS
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_ONGOING_RIVALRY_BEATEN_THEM_EVENT", "PRERACE_ONGOING_RIVALRY_BEATEN_THEM_EVENT_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_ONGOING_RIVALRY_SCHOOLED_YOU_EVENT", "PRERACE_ONGOING_RIVALRY_SCHOOLED_YOU_EVENT_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_ONGOING_RIVALRY_PAYBACKS_SUCCESSFUL
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_ONGOING_RIVALRY_YOU_PAYBACK_SUCESSFUL", "PRERACE_ONGOING_RIVALRY_YOU_PAYBACK_SUCESSFUL_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_ONGOING_RIVALRY_THEY_PAYBACK_SUCESSFUL", "PRERACE_ONGOING_RIVALRY_THEY_PAYBACK_SUCESSFUL_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
    {   // E_ONGOING_RIVALRY_PAYBACKS_ESCAPED
        {   // E_MESSAGE_STYLE_POSITIVE
            { "PRERACE_ONGOING_RIVALRY_YOU_ESCAPED_PAYBACK", "PRERACE_ONGOING_RIVALRY_YOU_ESCAPED_PAYBACK_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
        {   // E_MESSAGE_STYLE_NEGATIVE
            { "PRERACE_ONGOING_RIVALRY_THEY_ESCAPED_PAYBACK", "PRERACE_ONGOING_RIVALRY_THEY_ESCAPED_PAYBACK_S" },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
            { NULL, NULL },
        },
    },
};

const CombinedStringID KA_NO_RIVALRY_MESSAGES[OnlineFlybyManager::E_NO_RIVALRY_COUNT] =
{
    { "PRERACE_THEY_TAKEDOWNS_AHEAD", "PRERACE_THEY_TAKEDOWNS_AHEAD_S" },   // E_NO_RIVALRY_TAKEDOWNS
    { "PRERACE_THEY_RIVALS_AHEAD", "PRERACE_THEY_RIVALS_AHEAD_S" },   // E_NO_RIVALRY_TOTAL_RIVALS
    { "PRERACE_NO_RIVALRY_RANK_IN_WORLD", "PRERACE_NO_RIVALRY_RANK_IN_WORLD" },   // E_NO_RIVALRY_RANK
    { "PRERACE_THEY_EVENTS_AHEAD", "PRERACE_THEY_EVENTS_AHEAD_S" },   // E_NO_RIVALRY_EVENTS
    { "PRERACE_THEY_WINS_AHEAD", "PRERACE_THEY_WINS_AHEAD_S" },   // E_NO_RIVALRY_WINS
};

// ---------------------------------------------------------------------------
// CalculateNumberOfCarsInFlyby  @ 0x82357F08  (virtual)
// Returns the number of *rival* cars to show: connected network players minus one (the local
// player), clamped to [0, 3] (FlybyRivalData::KI_MAX_CARS_IN_FLYBY).
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::CalculateNumberOfCarsInFlyby()
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
    CGS_ASSERT(mpScoringSystem, "GetScoringSystem()");
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    s32 liNumRivals = mpScoringSystem->GetNumberOfNetworkPlayersStillConnected() - 1;

    if (liNumRivals > 0)
    {
        if (liNumRivals >= 3)
            liNumRivals = 3;
        return liNumRivals;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// GetRivalName
// Looks up the player record for lPlayerID and returns the address of its PlayerName. On a miss
// the record pointer is null and the name's address is taken off it all the same, as the console
// does (the caller, CalculateOnlineRivals, asserts the result is non-null).
// ---------------------------------------------------------------------------
const CgsNetwork::PlayerName* OnlineFlybyManager::GetRivalName(BrnNetwork::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    return &mPlayerInputInterface.GetPlayerStatusDataByPlayerID(lPlayerID)->mPlayerName;
}

// ---------------------------------------------------------------------------
// CalculatePointsLeaderRating  @ 0x82358148
// +50 weighting if this rating's player is the current points leader.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculatePointsLeaderRating(FlybyManager::RivalRating* lpRating)
{
    CGS_ASSERT(mpScoringSystem, "mpScoringSystem");

    const BrnNetwork::NetworkPlayerID lPointsLeader = mpScoringSystem->GetPointsLeader();

    CGS_ASSERT(lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    if (lpRating->mPlayerID == lPointsLeader)
        lpRating->miRivalWeighting += KI_WEIGHTING_POINTS_LEADER;
}

// ---------------------------------------------------------------------------
// CalculateMarkedManRivalryRating  @ 0x823642F0
// Two passes:
//  (1) find the *local* player's record; if its marked-man target == this rating's player, the
//      local player is marking this rival -> +40.
//  (2) find this rating's player's record; if its marked-man target == the local player, the
//      rival is marking us -> +30.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateMarkedManRivalryRating(FlybyManager::RivalRating* lpRating)
{
    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");

    const BrnNetwork::NetworkPlayerID lLocalId = mpGameStateModule->GetLocalPlayerNetworkID();

    // (1) the local player's record
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpLocalStatusData =
        mPlayerInputInterface.GetPlayerStatusDataByPlayerID(lLocalId);
    CGS_ASSERT(lpLocalStatusData, "lpPlayerStatusData");

    if (lpLocalStatusData->mMarkedManPlayerID == lpRating->mPlayerID)
        lpRating->miRivalWeighting += KI_WEIGHTING_MARK_TARGET;

    // (2) the rival player's record
    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpRivalStatusData =
        mPlayerInputInterface.GetPlayerStatusDataByPlayerID(lpRating->mPlayerID);
    CGS_ASSERT(lpRivalStatusData, "lpPlayerStatusData");

    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");

    if (lpRivalStatusData->mMarkedManPlayerID == mpGameStateModule->GetLocalPlayerNetworkID())
        lpRating->miRivalWeighting += KI_WEIGHTING_MARK_MARKED_ME;
}

// ---------------------------------------------------------------------------
// CalculateNewOrOngoingRivalryRating  @ 0x82358028
// Adds +9 (new: the current relationship is level) or +8 (ongoing: someone is ahead) plus a +3
// "recent" bonus if the relationship last changed within KI_RECENT_ACTIVITY_SECONDS of now.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateNewOrOngoingRivalryRating(FlybyManager::RivalRating* lpRating)
{
    const BrnNetwork::LiveRevengeRelationship* lpRelationship =
        GetLiveRevengeRelationship(lpRating->mPlayerID);

    // The relationship's +112 word (miCurrentScoreForPlayersPointOfView) decides "new" (level ->
    // +9) vs "ongoing" (someone ahead -> +8).
    if (lpRelationship->IsCurrentRelationshipEqual())
        lpRating->miRivalWeighting += KI_WEIGHTING_NEW_RELATIONSHIP;
    else
        lpRating->miRivalWeighting += KI_WEIGHTING_LIVE_RELATIONSHIP;

    CgsSystem::DateAndTime lNow;
    lNow.Update();

    CGS_ASSERT(lpRelationship->Validate(), "lpRelationship->Validate()");

    const s32 liSecondsSinceActivity =
        lNow.GetDifferenceInSeconds(lpRelationship->GetLastChangedTime());

    if (liSecondsSinceActivity < KI_RECENT_ACTIVITY_SECONDS)
        lpRating->miRivalWeighting += KI_WEIGHTING_RECENT;
}

// ---------------------------------------------------------------------------
// CalculateRivalryRating  @ 0x8236DAE0
// The dispatcher: disconnect -> points-leader -> marked-man -> team ratings, then either the
// no-relationship random jitter (when total takedowns == 0) or the new/ongoing rivalry rating.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateRivalryRating(FlybyManager::RivalRating* lpRating)
{
    CGS_ASSERT(lpRating, "lpRating");

    CalculateDisconnectRating(lpRating);

    if (lpRating->miRivalWeighting != -1)
    {
        CalculatePointsLeaderRating(lpRating);
        CalculateMarkedManRivalryRating(lpRating);
        CalculateTeamRating(lpRating);

        CGS_ASSERT(lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        const BrnNetwork::LiveRevengeRelationship* lpRelationship =
            GetLiveRevengeRelationship(lpRating->mPlayerID);
        CGS_ASSERT(lpRelationship, "lpRelationship");

        const s32 liTotalTakedowns = lpRelationship->GetTotalTakedowns();
        lpRating->mbHasValidRelationship = (liTotalTakedowns > 0);

        if (liTotalTakedowns <= 0)
        {
            // No relationship: one inlined draw of the base random generator (the OLD 64-bit state's
            // top word, then the LCG step) masked to 3 bits, a small jitter so equal-weighted
            // strangers do not always sort identically.
            lpRating->miRivalWeighting += static_cast<s32>(mRandom.RandomUInt() & 7);
        }
        else
        {
            CalculateNewOrOngoingRivalryRating(lpRating);
        }

        CGS_ASSERT(lpRating->miRivalWeighting >= 0, "lpRating->miRivalWeighting >= 0");
    }
}

// ---------------------------------------------------------------------------
// GetOngoingStat  @ 0x823582A0
// Reads an ongoing-rivalry stat out of the relationship's overall stats: the local player's block
// for the positive style, the rival's block for the negative one.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::GetOngoingStat(FlybyManager::RivalRating* lpRating,
                                       EOngoingRivalryCompare leStatType, EMessageStyle leStyle)
{
    CGS_ASSERT(lpRating, "lpRating");

    const BrnNetwork::LiveRevengeRelationship* lpRelationship =
        GetLiveRevengeRelationship(lpRating->mPlayerID);

    const BrnNetwork::CommonRelationshipStats* lpCommonRelationship =
        (leStyle != E_MESSAGE_STYLE_POSITIVE) ? &lpRelationship->GetOverallStats()->mRivalStats
                                              : &lpRelationship->GetOverallStats()->mPlayerStats;

    CGS_ASSERT(lpCommonRelationship, "lpCommonRelationship");

    switch (leStatType)
    {
    case E_ONGOING_RIVALRY_MUGSHOTS_COLLECTED:
        return lpCommonRelationship->miScalps;
    case E_ONGOING_RIVALRY_MARKS:
        return lpCommonRelationship->miMarks;
    case E_ONGOING_RIVALRY_LONGEST_STREAK:
        return lpCommonRelationship->miLongestStreak;
    case E_ONGOING_RIVALRY_WINS:
        return lpCommonRelationship->miWins;
    case E_ONGOING_RIVALRY_PAYBACKS_SUCCESSFUL:
        return lpCommonRelationship->miPaybacksScored;
    case E_ONGOING_RIVALRY_PAYBACKS_ESCAPED:
        return lpCommonRelationship->miPaybacksDealt - lpCommonRelationship->miPaybacksScored;
    default:
        CGS_ASSERT(false, "Unknow stat type");
        return 0;
    }
}

// ---------------------------------------------------------------------------
// GetNewRivalryStat  @ 0x82358418
// Reads a new-rivalry stat. For the per-side stats (score settled, longest streak) leStyle selects
// the local player's block (positive) or the rival's block (negative); the current status is the
// score from that side's point of view and the event count is shared.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::GetNewRivalryStat(FlybyManager::RivalRating* lpRating,
                                          ENewRivalryCompare leStatType, EMessageStyle leStyle)
{
    CGS_ASSERT(lpRating, "lpRating");

    const BrnNetwork::LiveRevengeRelationship* lpRelationship =
        GetLiveRevengeRelationship(lpRating->mPlayerID);
    CGS_ASSERT(lpRelationship, "lpRelationship");

    const bool lbRivalSide = (leStyle != E_MESSAGE_STYLE_POSITIVE);
    const BrnNetwork::CommonRelationshipStats* lpSideStats =
        lbRivalSide ? &lpRelationship->GetOverallStats()->mRivalStats
                    : &lpRelationship->GetOverallStats()->mPlayerStats;

    switch (leStatType)
    {
    case E_NEW_RIVALRY_CURRENT_STATUS:
        return lbRivalSide ? lpRelationship->GetCurrentScoreForRival()
                           : lpRelationship->GetCurrentScoreForLocalPlayer();
    case E_NEW_RIVALRY_SCORE_SETTLED:
        return lpSideStats->miScoresSettled;
    case E_NEW_RIVALRY_EVENTS:
        return lpRelationship->GetTotalEvents();
    case E_NEW_RIVALRY_LONGEST_STREAK:
        return lpSideStats->miLongestStreak;
    default:
        CGS_ASSERT(false, "Unknow stat type");
        return 0;
    }
}

// ---------------------------------------------------------------------------
// GetNoRivalryStat  @ 0x82364488
// For the rating's player (or the local player when there is no rating) reads one of the
// player's network stats. On a record miss GetStatAsInt is called on the null record, as the
// console does.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::GetNoRivalryStat(FlybyManager::RivalRating* lpRating,
                                         ENoRivalryCompare leStatType)
{
    const BrnNetwork::NetworkPlayerID lPlayerID =
        lpRating ? lpRating->mPlayerID : GetLocalPlayerNetworkID();

    switch (leStatType)
    {
    case E_NO_RIVALRY_TAKEDOWNS:
        return GetPlayerStats(lPlayerID)->GetStatAsInt(BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_TAKEDOWNS);
    case E_NO_RIVALRY_TOTAL_RIVALS:
        return GetPlayerStats(lPlayerID)->GetStatAsInt(BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_NUMBER_OF_RIVALS);
    case E_NO_RIVALRY_RANK:
        return GetPlayerStats(lPlayerID)->GetStatAsInt(BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_RANK);
    case E_NO_RIVALRY_EVENTS:
        return GetPlayerStats(lPlayerID)->GetStatAsInt(BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_TOTAL_GAMES_COMPLETED);
    case E_NO_RIVALRY_WINS:
        return GetPlayerStats(lPlayerID)->GetStatAsInt(BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_WINS);
    default:
        CGS_ASSERT(false, "Unknown stat type");
        return 0;
    }
}

// ---------------------------------------------------------------------------
// GetLiveRevengeRelationship
// The live-revenge relationship of the player's status record (record +0x88); on a miss the
// address is taken off the null record, as the console does.
// ---------------------------------------------------------------------------
const BrnNetwork::LiveRevengeRelationship* OnlineFlybyManager::GetLiveRevengeRelationship(
    BrnNetwork::NetworkPlayerID lPlayerID) const
{
    return &mPlayerInputInterface.GetPlayerStatusDataByPlayerID(lPlayerID)->mLiveRevengeRelationship;
}

// ---------------------------------------------------------------------------
// CountAvailableOngoingRivalryMessages  @ 0x82358570
// Walks the KA_ONGOING_RIVALRY_MESSAGES[leStatType][leStyle][] string-id row counting entries with a
// plural id, capped at KI_MAX_MESSAGES (5). Hitting the cap means an unterminated table row -> the
// X360 fires the "haven't null terminated a table entry" assert and returns 0.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::CountAvailableOngoingRivalryMessages(EOngoingRivalryCompare leStatType,
                                                             EMessageStyle leStyle)
{
    s32 liCount = 0;
    while (KA_ONGOING_RIVALRY_MESSAGES[leStatType][leStyle][liCount].mpcPluralStringID != NULL)
    {
        ++liCount;
        if (liCount >= KI_MAX_MESSAGES)
        {
            CGS_ASSERT(false,
                       "Something has gone wrong, you probably haven't null terminated a table entry!");
            return 0;
        }
    }
    return liCount;
}

// ---------------------------------------------------------------------------
// CountAvailableNewRivalryMessages  @ 0x82358648
// As above against KA_NEW_RIVALRY_MESSAGES.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::CountAvailableNewRivalryMessages(ENewRivalryCompare leStatType,
                                                         EMessageStyle leStyle)
{
    s32 liCount = 0;
    while (KA_NEW_RIVALRY_MESSAGES[leStatType][leStyle][liCount].mpcPluralStringID != NULL)
    {
        ++liCount;
        if (liCount >= KI_MAX_MESSAGES)
        {
            CGS_ASSERT(false,
                       "Something has gone wrong, you probably haven't null terminated a table entry!");
            return 0;
        }
    }
    return liCount;
}

// ---------------------------------------------------------------------------
// CalculateOnlineRivals  @ 0x823861C0
// The driver. Prepares the FlybyData payload, builds up to 8 RivalRating slots (one per network
// player, with the local player's slot marked invalid), scores each via CalculateRivalryRating,
// sorts by weighting, then emits the top N cars + their rivalry/points-leader/no-rivalry messages.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateOnlineRivals()
{
    GameStateModuleIO::FlybyData* lpFlybyData = GetFlybyData();

    const s32 liNumCars = CalculateNumberOfCarsInFlyby();
    lpFlybyData->Prepare();

    if (liNumCars == 0)
        return;

    const BrnNetwork::NetworkPlayerID lLocalId = GetLocalPlayerNetworkID();
    if (lLocalId == CgsNetwork::K_INVALID_PLAYER_ID)
        return;

    GameStateModule* lpModule = GetGameStateModule();
    const s32 leLocalActiveRaceCarIndex = lpModule->GetPlayerActiveRaceCarIndex();

    // Build the 8 rival-rating slots.
    FlybyManager::RivalRating laRivalRatings[8];
    s32 liPlayer = 0;
    const s32 liNumPlayers = mPlayerInputInterface.GetNumPlayers();
    for (; liPlayer < liNumPlayers; ++liPlayer)
    {
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData =
            mPlayerInputInterface.GetPlayerStatusData(liPlayer);
        CGS_ASSERT(lpPlayerStatusData, "lpPlayerStatusData");

        const BrnNetwork::NetworkPlayerID lThisId = lpPlayerStatusData->mNetworkPlayerID;

        CGS_ASSERT(mpGameStateModule, "mpGameStateModule");

        // The X360 cross-references the player to its active-race-car index via the module's
        // player table (sub_8231DD88). When that index equals the local player's active-race-car
        // index this slot is the local player and is marked invalid (mPlayerID = -1).
        const s32 leActiveRaceCarIndex = ModulePlayerActiveRaceCarIndex(mpGameStateModule, lThisId);

        laRivalRatings[liPlayer].mbHasValidRelationship = false;
        laRivalRatings[liPlayer].miRivalWeighting = 0;
        if (leLocalActiveRaceCarIndex == leActiveRaceCarIndex)
            laRivalRatings[liPlayer].mPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        else
            laRivalRatings[liPlayer].mPlayerID = lThisId;
    }

    // Pad the remaining slots up to 8 as invalid.
    for (; liPlayer < 8; ++liPlayer)
    {
        laRivalRatings[liPlayer].mbHasValidRelationship = false;
        laRivalRatings[liPlayer].miRivalWeighting = 0;
        laRivalRatings[liPlayer].mPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
    }

    // Score every valid slot.
    for (s32 li = 0; li < 8; ++li)
    {
        if (laRivalRatings[li].mPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
            laRivalRatings[li].miRivalWeighting = 0;
        else
            CalculateRivalryRating(&laRivalRatings[li]);
    }

    // Sort highest-weighting first (invalid ids sink to the bottom -- see SortRivalsCallback).
    ::qsort(laRivalRatings, 8, sizeof(FlybyManager::RivalRating),
            &FlybyManager::RivalRating::SortRivalsCallback);

    // Emit the top liNumCars.
    for (s32 li = 0; li < liNumCars; ++li)
    {
        FlybyManager::RivalRating* lpRating = &laRivalRatings[li];

        CGS_ASSERT(mpGameStateModule, "mpGameStateModule");
        CGS_ASSERT(lpRating->mPlayerID != mpGameStateModule->GetLocalPlayerNetworkID(),
                   "laRivalRatings[liPlayerIndex].mPlayerID != GetLocalPlayerNetworkID()");
        CGS_ASSERT(lpRating->miRivalWeighting != -1,
                   "laRivalRatings[liPlayerIndex].miRivalWeighting != -1");

        CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
        const s32 leActiveRaceCarIndex =
            ModulePlayerActiveRaceCarIndex(mpScoringSystem, lpRating->mPlayerID);
        CGS_ASSERT(leActiveRaceCarIndex > -1, "leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leActiveRaceCarIndex < 8, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        const CgsNetwork::PlayerName* lpPlayerName = GetRivalName(lpRating->mPlayerID);
        CGS_ASSERT(lpPlayerName, "lpPlayerName");

        lpFlybyData->AddCar(static_cast<EActiveRaceCarIndex>(leActiveRaceCarIndex), lpPlayerName);
        lpFlybyData->AddMessage(KPC_GAMERTAG_STRING_ID, lpPlayerName->GetPlayerName());

        // Resolve this rival's relationship for the message-flavour decision.
        const BrnNetwork::LiveRevengeRelationship* lpRelationship =
            GetLiveRevengeRelationship(lpRating->mPlayerID);
        CGS_ASSERT(lpRelationship, "lpLiveRevengeRelationship");

        CGS_ASSERT(mpScoringSystem, "mpScoringSystem");
        if (lpRating->mPlayerID == mpScoringSystem->GetPointsLeader())
        {
            lpFlybyData->AddMessage("PRERACE_POINTS_LEADER", NULL);
            // The X360 then re-validates the car count and tags the just-added car as the
            // points-leader style; bounds asserts preserved verbatim.
            CGS_ASSERT(lpFlybyData->miNumberOfCars > 0, "miNumberOfCars > 0");
            CGS_ASSERT(lpFlybyData->miNumberOfCars <= 3,
                       "miNumberOfCars <= FlybyRivalData::KI_MAX_CARS_IN_FLYBY");
            FlybyData_TagLastCarStyle(lpFlybyData, 2 /*points-leader style*/);
        }
        else if (!lpRating->mbHasValidRelationship)
        {
            AddNoRivalryFlybyMessage(lpRating);
        }
        else
        {
            const s32 liTotalTakedowns = lpRelationship->GetTotalTakedowns();
            if (liTotalTakedowns >= 5)
            {
                if (liTotalTakedowns >= 10)
                    AddOngoingRivalryMessage(lpRating);
                else
                    AddNewRivalryMessage(lpRating);
            }
            else
            {
                AddNoRivalryFlybyMessage(lpRating);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// AddNoRivalryFlybyMessage
// A rival we have no rivalry with: pick one of his non-zero stats (takedowns, rivals, rank,
// events, wins) that has not been shown yet -- every one again once all have been -- and add its
// message with the stat as the parameter, then mark that stat shown. With no non-zero stat at all
// the "no rank" message is added and the rival's flyby entry takes the neutral style.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::AddNoRivalryFlybyMessage(FlybyManager::RivalRating* lpRating)
{
    CGS_ASSERT(lpRating, "lpRating");
    CGS_ASSERT(lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    CgsContainers::FastBitArray<E_NO_RIVALRY_COUNT> lNotZeroMessage;
    lNotZeroMessage.Construct();
    for (ENoRivalryCompare leStatType = E_NO_RIVALRY_START; leStatType < E_NO_RIVALRY_COUNT; leStatType++)
    {
        if (GetNoRivalryStat(lpRating, leStatType) > 0)
        {
            lNotZeroMessage.SetBit(static_cast<u32>(leStatType));
        }
    }

    if (lNotZeroMessage.IsZero())
    {
        GameStateModuleIO::FlybyData* lpFlybyData = GetFlybyData();
        lpFlybyData->AddMessage("PRERACE_NO_RIVALRY_RANK_NONE", NULL);
        CGS_ASSERT(lpFlybyData->miNumberOfCars > 0, "miNumberOfCars > 0");
        CGS_ASSERT(lpFlybyData->miNumberOfCars <= 3,
                   "miNumberOfCars <= FlybyRivalData::KI_MAX_CARS_IN_FLYBY");
        FlybyData_TagLastCarStyle(lpFlybyData, 1 /*neutral style*/);
        return;
    }

    CgsContainers::FastBitArray<E_NO_RIVALRY_COUNT> lNotZeroAndNonShownMessage;
    lNotZeroAndNonShownMessage.SetAnd(mAvailableNoRivalryMessages, lNotZeroMessage);
    if (lNotZeroAndNonShownMessage.IsZero())
    {
        mAvailableNoRivalryMessages.SetAll();
        lNotZeroAndNonShownMessage = lNotZeroMessage;
    }

    s32 liAvailableMessages = 0;
    for (CgsContainers::FastBitArray<E_NO_RIVALRY_COUNT>::Iterator lIt = lNotZeroAndNonShownMessage.Begin();
         lIt != lNotZeroAndNonShownMessage.End(); ++lIt)
    {
        ++liAvailableMessages;
    }

    s32 liRandomIndex = mRandom.RandomInt(0, liAvailableMessages - 1);
    CGS_ASSERT(liRandomIndex <= liAvailableMessages, "liRandomIndex <= liAvailableMessages");

    CgsContainers::FastBitArray<E_NO_RIVALRY_COUNT>::Iterator lStatIt = lNotZeroAndNonShownMessage.Begin();
    while (liRandomIndex > 0)
    {
        --liRandomIndex;
        ++lStatIt;
        CGS_ASSERT(lStatIt != lNotZeroAndNonShownMessage.End(), "lStatIt != lNotZeroAndNonShownMessage.End()");
    }
    CGS_ASSERT(lStatIt != lNotZeroAndNonShownMessage.End(), "lStatIt != lNotZeroAndNonShownMessage.End()");

    const ENoRivalryCompare leStatType = static_cast<ENoRivalryCompare>(lStatIt.GetIndex());
    const s32 liStat = GetNoRivalryStat(lpRating, leStatType);

    char lacParameter[16];
    const char* lpcMessage;
    if (liStat == 1)
    {
        lpcMessage = KA_NO_RIVALRY_MESSAGES[leStatType].mpcSingularStringID;
        CgsCore::SPrintf(lacParameter, sizeof(lacParameter), "%i", 1);
    }
    else
    {
        lpcMessage = KA_NO_RIVALRY_MESSAGES[leStatType].mpcPluralStringID;
        CgsCore::SPrintf(lacParameter, sizeof(lacParameter), "%i", liStat);
    }
    GetFlybyData()->AddMessage(lpcMessage, lacParameter);

    mAvailableNoRivalryMessages.UnSetBit(static_cast<u32>(leStatType));
}

// ---------------------------------------------------------------------------
// AddNewRivalryMessage
// A rival we have a new rivalry with. With the current relationship level, one of the stats either
// side has a non-zero value of is picked at random; otherwise the current status. The side with the
// larger value picks the style, a random message of that stat and style is added with the value
// (singular form for exactly 1) as the parameter.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::AddNewRivalryMessage(FlybyManager::RivalRating* lpRating)
{
    const BrnNetwork::LiveRevengeRelationship* lpRelationship =
        GetLiveRevengeRelationship(lpRating->mPlayerID);
    CGS_ASSERT(lpRelationship, "lpRelationship");

    ENewRivalryCompare leStatType = E_NEW_RIVALRY_START;
    if (lpRelationship->IsCurrentRelationshipEqual())
    {
        Array<ENewRivalryCompare, E_NEW_RIVALRY_COUNT> laAvaiableMessageIndex;
        laAvaiableMessageIndex.Construct();
        for (ENewRivalryCompare leStat = E_NEW_RIVALRY_START; leStat < E_NEW_RIVALRY_COUNT; leStat++)
        {
            if (GetNewRivalryStat(lpRating, leStat, E_MESSAGE_STYLE_POSITIVE) > 0 ||
                GetNewRivalryStat(lpRating, leStat, E_MESSAGE_STYLE_NEGATIVE) > 0)
            {
                laAvaiableMessageIndex.Append(leStat);
            }
        }

        CGS_ASSERT(laAvaiableMessageIndex.GetLength() > 0, "laAvaiableMessageIndex.GetLength() > 0");
        leStatType = laAvaiableMessageIndex[static_cast<u32>(
            mRandom.RandomInt(0, static_cast<s32>(laAvaiableMessageIndex.GetLength()) - 1))];
    }

    const s32 liPositiveStat = GetNewRivalryStat(lpRating, leStatType, E_MESSAGE_STYLE_POSITIVE);
    const s32 liNegativeStat = GetNewRivalryStat(lpRating, leStatType, E_MESSAGE_STYLE_NEGATIVE);
    EMessageStyle leStyle;
    s32           liStat;
    if (liPositiveStat > liNegativeStat)
    {
        leStyle = E_MESSAGE_STYLE_POSITIVE;
        liStat  = liPositiveStat;
    }
    else
    {
        leStyle = E_MESSAGE_STYLE_NEGATIVE;
        liStat  = liNegativeStat;
    }

    const s32 liMessage = mRandom.RandomInt(0, CountAvailableNewRivalryMessages(leStatType, leStyle) - 1);
    const CombinedStringID& lrMessage = KA_NEW_RIVALRY_MESSAGES[leStatType][leStyle][liMessage];

    char lacParameter[16];
    const char* lpcMessage;
    if (liStat == 1)
    {
        lpcMessage = lrMessage.mpcSingularStringID;
        CgsCore::SPrintf(lacParameter, sizeof(lacParameter), "%i", 1);
    }
    else
    {
        lpcMessage = lrMessage.mpcPluralStringID;
        CgsCore::SPrintf(lacParameter, sizeof(lacParameter), "%i", liStat);
    }
    GetFlybyData()->AddMessage(lpcMessage, lacParameter);
}

// ---------------------------------------------------------------------------
// AddOngoingRivalryMessage
// A rival we have an ongoing rivalry with: one of the stats either side has a non-zero value of is
// picked at random, the side with the larger value picks the style, and a random message of that
// stat and style is added with the value (singular form for exactly 1) as the parameter.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::AddOngoingRivalryMessage(FlybyManager::RivalRating* lpRating)
{
    Array<EOngoingRivalryCompare, E_ONGOING_RIVALRY_COUNT> laAvaiableMessageIndex;
    laAvaiableMessageIndex.Construct();
    for (EOngoingRivalryCompare leStat = E_ONGOING_RIVALRY_START; leStat < E_ONGOING_RIVALRY_COUNT; leStat++)
    {
        if (GetOngoingStat(lpRating, leStat, E_MESSAGE_STYLE_POSITIVE) > 0 ||
            GetOngoingStat(lpRating, leStat, E_MESSAGE_STYLE_NEGATIVE) > 0)
        {
            laAvaiableMessageIndex.Append(leStat);
        }
    }

    const EOngoingRivalryCompare leStatType = laAvaiableMessageIndex[static_cast<u32>(
        mRandom.RandomInt(0, static_cast<s32>(laAvaiableMessageIndex.GetLength()) - 1))];

    const s32 liPositiveStat = GetOngoingStat(lpRating, leStatType, E_MESSAGE_STYLE_POSITIVE);
    const s32 liNegativeStat = GetOngoingStat(lpRating, leStatType, E_MESSAGE_STYLE_NEGATIVE);
    EMessageStyle leStyle;
    s32           liStat;
    if (liPositiveStat > liNegativeStat)
    {
        leStyle = E_MESSAGE_STYLE_POSITIVE;
        liStat  = liPositiveStat;
    }
    else
    {
        leStyle = E_MESSAGE_STYLE_NEGATIVE;
        liStat  = liNegativeStat;
    }

    const s32 liMessage = mRandom.RandomInt(0, CountAvailableOngoingRivalryMessages(leStatType, leStyle) - 1);
    const CombinedStringID& lrMessage = KA_ONGOING_RIVALRY_MESSAGES[leStatType][leStyle][liMessage];

    char lacParameter[16];
    const char* lpcMessage;
    if (liStat == 1)
    {
        lpcMessage = lrMessage.mpcSingularStringID;
        CgsCore::SPrintf(lacParameter, sizeof(lacParameter), "%i", 1);
    }
    else
    {
        lpcMessage = lrMessage.mpcPluralStringID;
        CgsCore::SPrintf(lacParameter, sizeof(lacParameter), "%i", liStat);
    }
    GetFlybyData()->AddMessage(lpcMessage, lacParameter);
}

}
