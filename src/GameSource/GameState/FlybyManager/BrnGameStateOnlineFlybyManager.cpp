#include "types.hpp"

#include <stdlib.h> // qsort

#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"

// ===========================================================================
// BrnGameState::OnlineFlybyManager -- online flyby / rival-rating subsystem.
//
// Twelve functions recovered from the X360 ARTIST build. Every per-player record lookup below is
// the linear search the X360 inlines at each site: walk maPlayerStats[0..miNumPlayers) comparing
// mNetworkPlayerID against the wanted id; on a miss the record pointer is left null (matching the
// `v6 = 0` / `v11 = 0` paths) and the caller fires its own "lpPlayerStatusData"/"lpRelationship"
// assert. Members are addressed BY NAME; the named-struct layout in the header is what makes the
// X360 this-relative offset math resolve.
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

// ---------------------------------------------------------------------------
// CalculateNumberOfCarsInFlyby  @ 0x82357F08  (virtual)
// Returns the number of *rival* cars to show: connected network players minus one (the local
// player), clamped to [0, 3] (FlybyRivalData::KI_MAX_CARS_IN_FLYBY).
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::CalculateNumberOfCarsInFlyby()
{
    CGS_ASSERT(mpScoringSystem.Get(), "mpScoringSystem");
    CGS_ASSERT(mpScoringSystem.Get(), "GetScoringSystem()");
    CGS_ASSERT(mpScoringSystem.Get(), "mpScoringSystem");

    s32 liNumRivals =
        ScoringSystem_GetNumberOfNetworkPlayersStillConnected(mpScoringSystem.Get()) - 1;

    if (liNumRivals > 0)
    {
        if (liNumRivals >= 3)
            liNumRivals = 3;
        return liNumRivals;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// GetRivalName  @ 0x823581F8
// Looks up the player record for lPlayerID and returns the address of its PlayerName. On a miss
// the record pointer is null and (null + 256) is returned -- the X360 behaviour exactly (the
// caller, CalculateOnlineRivals, asserts the result is non-null).
// ---------------------------------------------------------------------------
const u8* OnlineFlybyManager::GetRivalName(BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    NetworkPlayerStats* lpStats = 0;
    const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
    for (s32 li = 0; li < liNumPlayers; ++li)
    {
        if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lPlayerID)
        {
            lpStats = &mPlayerStatusInterface.maPlayerStats[li];
            break;
        }
    }

    // (null + 0x100) when not found -- byte-exact with the X360 `return v7 + 256`.
    return reinterpret_cast<const u8*>(lpStats) + 0x100;
}

// ---------------------------------------------------------------------------
// CalculatePointsLeaderRating  @ 0x82358148
// +50 weighting if this rating's player is the current points leader.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculatePointsLeaderRating(FlybyManager::RivalRating* lpRating)
{
    CGS_ASSERT(mpScoringSystem.Get(), "mpScoringSystem");

    BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPointsLeader =
        ScoringSystem_GetPointsLeader(mpScoringSystem.Get());

    CGS_ASSERT(lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lpRating->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    if (lpRating->mPlayerID == lPointsLeader)
        lpRating->miRivalWeighting += KI_WEIGHTING_POINTS_LEADER;
}

// ---------------------------------------------------------------------------
// CalculateMarkedManRivalryRating  @ 0x823642F0
// Two passes:
//  (1) find the *local* player's record; if its mMarkedManTargetID == this rating's player, the
//      local player is marking this rival -> +40.
//  (2) find this rating's player's record; if its mMarkedManTargetID == the local player, the
//      rival is marking us -> +30.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateMarkedManRivalryRating(FlybyManager::RivalRating* lpRating)
{
    CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");

    const BrnNetwork::RoadRulesRecvData::NetworkPlayerID lLocalId =
        mpGameStateModule.Get()->GetLocalPlayerNetworkID();

    // (1) the local player's record
    NetworkPlayerStats* lpLocalStats = 0;
    {
        const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
        for (s32 li = 0; li < liNumPlayers; ++li)
        {
            if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lLocalId)
            {
                lpLocalStats = &mPlayerStatusInterface.maPlayerStats[li];
                break;
            }
        }
    }
    CGS_ASSERT(lpLocalStats, "lpPlayerStatusData");

    if (lpLocalStats->mMarkedManTargetID == lpRating->mPlayerID)
        lpRating->miRivalWeighting += KI_WEIGHTING_MARK_TARGET;

    // (2) the rival player's record
    NetworkPlayerStats* lpRivalStats = 0;
    {
        const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
        for (s32 li = 0; li < liNumPlayers; ++li)
        {
            if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lpRating->mPlayerID)
            {
                lpRivalStats = &mPlayerStatusInterface.maPlayerStats[li];
                break;
            }
        }
    }
    CGS_ASSERT(lpRivalStats, "lpPlayerStatusData");

    CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");

    if (lpRivalStats->mMarkedManTargetID == mpGameStateModule.Get()->GetLocalPlayerNetworkID())
        lpRating->miRivalWeighting += KI_WEIGHTING_MARK_MARKED_ME;
}

// ---------------------------------------------------------------------------
// CalculateNewOrOngoingRivalryRating  @ 0x82358028
// Adds +9 (new: no live activity) or +8 (ongoing: live activity present) plus a +3 "recent"
// bonus if the relationship's last activity is within KI_RECENT_ACTIVITY_SECONDS of now.
// Returns the difference-in-seconds (the X360 returns r3 == GetDifferenceInSeconds's result).
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateNewOrOngoingRivalryRating(FlybyManager::RivalRating* lpRating)
{
    NetworkPlayerStats* lpStats = 0;
    {
        const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
        for (s32 li = 0; li < liNumPlayers; ++li)
        {
            if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lpRating->mPlayerID)
            {
                lpStats = &mPlayerStatusInterface.maPlayerStats[li];
                break;
            }
        }
    }

    BrnNetwork::LiveRevengeRelationship* lpRelationship = lpStats->GetLiveRelationship();

    // The X360 reads *(record + 248) == relationship + 112 == miNetTakedownDelta to decide "new"
    // (no net activity yet -> +9) vs "ongoing" (net activity recorded -> +8).
    const bool lbIsNew = (lpRelationship->miNetTakedownDelta == 0);

    if (lbIsNew)
        lpRating->miRivalWeighting += KI_WEIGHTING_NEW_RELATIONSHIP;
    else
        lpRating->miRivalWeighting += KI_WEIGHTING_LIVE_RELATIONSHIP;

    CgsSystem::DateAndTime lNow;
    lNow.Update();

    CGS_ASSERT(lpRelationship->Validate(), "lpRelationship->Validate()");

    const s32 liSecondsSinceActivity =
        lNow.GetDifferenceInSeconds(lpRelationship->mLastActivity);

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

        BrnNetwork::LiveRevengeRelationship* lpRelationship =
            GetLiveRevengeRelationship(lpRating->mPlayerID);
        CGS_ASSERT(lpRelationship, "lpRelationship");

        const s32 liTotalTakedowns = lpRelationship->GetTotalTakedowns();
        lpRating->mbHasValidRelationship = (liTotalTakedowns > 0);

        if (liTotalTakedowns <= 0)
        {
            // No relationship: advance the base CgsNumeric::Random 64-bit LCG (its state lives in
            // the mRandom blob at mRandom+0x20 == this+0x280) and add the OLD state's high-3-bits as
            // a small jitter so equal-weighted strangers do not always sort identically. The X360
            // inlines the Random step here (asm 0x8236DBF0-0x8236DC28): the 64-bit state is loaded
            // (ld), the new state = oldState * 0x5851F42D4C957F2D + 1 (the standard Knuth/PCG 64-bit
            // LCG multiplier, built via lis/ori/insrdi) is stored back (std), and the jitter is the
            // OLD state's top 32 bits masked to 3 bits ((oldState >> 32) & 7).
            u64* lpSeed = reinterpret_cast<u64*>(&mRandom[0x20]);
            const u64 luOldState = *lpSeed;
            *lpSeed = luOldState * 0x5851F42D4C957F2DULL + 1u;
            lpRating->miRivalWeighting += static_cast<s32>((luOldState >> 32) & 7);
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
// Reads an ongoing-rivalry stat out of the live (lbUseSquare==false) or square
// (lbUseSquare==true) relationship's common-stats block.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::GetOngoingStat(FlybyManager::RivalRating* lpRating,
                                       EOngoingRivalryCompare leStatType, EMessageStyle leStyle)
{
    CGS_ASSERT(lpRating, "lpRating");

    NetworkPlayerStats* lpStats = 0;
    {
        const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
        for (s32 li = 0; li < liNumPlayers; ++li)
        {
            if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lpRating->mPlayerID)
            {
                lpStats = &mPlayerStatusInterface.maPlayerStats[li];
                break;
            }
        }
    }

    const bool lbUseSquare = (leStyle != E_MESSAGE_STYLE_POSITIVE);
    BrnNetwork::LiveRevengeRelationship* lpCommonRelationship =
        lbUseSquare ? lpStats->GetSquareRelationship() : lpStats->GetLiveRelationship();

    CGS_ASSERT(lpCommonRelationship, "lpCommonRelationship");

    switch (leStatType)
    {
    case E_ONGOING_RIVALRY_MUGSHOTS_COLLECTED:
        return lpCommonRelationship->miMugshotsCollected;
    case E_ONGOING_RIVALRY_MARKS:
        return lpCommonRelationship->miMarks;
    case E_ONGOING_RIVALRY_LONGEST_STREAK:
        return lpCommonRelationship->miLongestStreak;
    case E_ONGOING_RIVALRY_WINS:
        return lpCommonRelationship->miWins;
    case E_ONGOING_RIVALRY_PAYBACKS_SUCCESSFUL:
        return lpCommonRelationship->miPaybacksSuccessful;
    case E_ONGOING_RIVALRY_PAYBACKS_ESCAPED:
        return lpCommonRelationship->miPaybacksTotal - lpCommonRelationship->miPaybacksSuccessful;
    default:
        CGS_ASSERT(false, "Unknow stat type");
        return 0;
    }
}

// ---------------------------------------------------------------------------
// GetNewRivalryStat  @ 0x82358418
// Reads a new-rivalry stat. For the per-side stats (events, longest-streak) leStyle selects the
// player-side (+0) vs rival-side (+36) sub-block; the "score settled" / "current status" stats
// read the overall block.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::GetNewRivalryStat(FlybyManager::RivalRating* lpRating,
                                          ENewRivalryCompare leStatType, EMessageStyle leStyle)
{
    CGS_ASSERT(lpRating, "lpRating");

    NetworkPlayerStats* lpStats = 0;
    {
        const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
        for (s32 li = 0; li < liNumPlayers; ++li)
        {
            if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lpRating->mPlayerID)
            {
                lpStats = &mPlayerStatusInterface.maPlayerStats[li];
                break;
            }
        }
    }

    BrnNetwork::LiveRevengeRelationship* lpRelationship = lpStats->GetLiveRelationship();
    CGS_ASSERT(lpRelationship, "lpRelationship");

    // leStyle selects the player-side (positive) vs rival-side (negative) CommonStats sub-block.
    // In the X360 this is `v13 = v12 + 36` -- the rival-side block of the SAME relationship.
    const bool lbRivalSide = (leStyle != E_MESSAGE_STYLE_POSITIVE);

    switch (leStatType)
    {
    case E_NEW_RIVALRY_CURRENT_STATUS:
        // *(v12 + 112): the net takedown delta (side-independent), negated for the rival-side view.
        return lbRivalSide ? -lpRelationship->miNetTakedownDelta : lpRelationship->miNetTakedownDelta;
    case E_NEW_RIVALRY_SCORE_SETTLED:
        // *(v13 + 20): per-side "score settled" (miScoreSettled).
        return lbRivalSide ? lpRelationship->miRivalScoreSettled : lpRelationship->miScoreSettled;
    case E_NEW_RIVALRY_EVENTS:
        // *(v12 + 116): events-played-together (side-independent).
        return lpRelationship->miEventsPlayedTogether;
    case E_NEW_RIVALRY_LONGEST_STREAK:
        // *(v13 + 8): per-side longest streak.
        return lbRivalSide ? lpRelationship->miRivalLongestStreak : lpRelationship->miLongestStreak;
    default:
        CGS_ASSERT(false, "Unknow stat type");
        return 0;
    }
}

// ---------------------------------------------------------------------------
// GetNoRivalryStat  @ 0x82364488
// For the local player (or the rating's player) reads a top-level NetworkPlayerStats value via
// GetStatAsInt with a per-enum stat index. On a record miss it calls GetStatAsInt(null, idx)
// exactly as the X360 does.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::GetNoRivalryStat(FlybyManager::RivalRating* lpRating,
                                         ENoRivalryCompare leStatType)
{
    BrnNetwork::RoadRulesRecvData::NetworkPlayerID lPlayerID =
        lpRating ? lpRating->mPlayerID : GetLocalPlayerNetworkID();

    s32 liStatIndex;
    switch (leStatType)
    {
    case E_NO_RIVALRY_TAKEDOWNS:    liStatIndex = 4;  break;
    case E_NO_RIVALRY_TOTAL_RIVALS: liStatIndex = 5;  break;
    case E_NO_RIVALRY_RANK:         liStatIndex = 8;  break;
    case E_NO_RIVALRY_EVENTS:       liStatIndex = 0;  break;
    case E_NO_RIVALRY_WINS:         liStatIndex = 11; break;
    default:
        CGS_ASSERT(false, "Unknown stat type");
        return 0;
    }

    NetworkPlayerStats* lpStats = 0;
    {
        const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
        for (s32 li = 0; li < liNumPlayers; ++li)
        {
            if (mPlayerStatusInterface.maPlayerStats[li].mNetworkPlayerID == lPlayerID)
            {
                lpStats = &mPlayerStatusInterface.maPlayerStats[li];
                break;
            }
        }
    }

    return NetworkPlayerStats::GetStatAsInt(lpStats, liStatIndex);
}

// ---------------------------------------------------------------------------
// CountAvailableOngoingRivalryMessages  @ 0x82358570
// Walks the KA_ONGOING_RIVALRY_MESSAGES[leStatType][leStyle][] string-id row counting non-null
// entries, capped at KI_MAX_MESSAGES (5). Hitting the cap means an unterminated table row -> the
// X360 fires the "haven't null terminated a table entry" assert and returns 0.
// ---------------------------------------------------------------------------
s32 OnlineFlybyManager::CountAvailableOngoingRivalryMessages(EOngoingRivalryCompare leStatType,
                                                             EMessageStyle leStyle)
{
    s32 liCount = 0;
    const void* const* lppRow = KA_ONGOING_RIVALRY_MESSAGES[leStatType][leStyle];
    while (lppRow[0])
    {
        ++liCount;
        lppRow += 2; // CombinedStringID is 8 bytes (two pointers wide in this row layout)
        if (liCount >= 5)
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
    const void* const* lppRow = KA_NEW_RIVALRY_MESSAGES[leStatType][leStyle];
    while (lppRow[0])
    {
        ++liCount;
        lppRow += 2;
        if (liCount >= 5)
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
//
// The FlybyData / GameStateModule / ScoringSystem payload calls (FlybyData::Prepare/AddCar/
// AddMessage, GetPlayerActiveRaceCarIndex, GetPointsLeader) live in other TUs and are reached
// through the typed accessors declared in the header; the rival-message Add* helpers are this
// class's own non-twelve members (declare-only here). The control flow is reproduced faithfully.
// ---------------------------------------------------------------------------
void OnlineFlybyManager::CalculateOnlineRivals()
{
    GameStateModuleIO::FlybyData* lpFlybyData = GetFlybyData();

    const s32 liNumCars = CalculateNumberOfCarsInFlyby();
    FlybyData_Prepare(lpFlybyData);

    if (liNumCars == 0)
        return;

    const BrnNetwork::RoadRulesRecvData::NetworkPlayerID lLocalId = GetLocalPlayerNetworkID();
    if (lLocalId == CgsNetwork::K_INVALID_PLAYER_ID)
        return;

    GameStateModule* lpModule = GetGameStateModule();
    const s32 leLocalActiveRaceCarIndex = lpModule->GetPlayerActiveRaceCarIndex();

    // Build the 8 rival-rating slots.
    FlybyManager::RivalRating laRivalRatings[8];
    s32 liPlayer = 0;
    const s32 liNumPlayers = mPlayerStatusInterface.miNumPlayers;
    for (; liPlayer < liNumPlayers; ++liPlayer)
    {
        CGS_ASSERT(liPlayer >= 0, "liIndex >= 0");
        CGS_ASSERT(liPlayer < mPlayerStatusInterface.miNumPlayers, "liIndex < miNumPlayers");

        NetworkPlayerStats* lpStatusData = &mPlayerStatusInterface.maPlayerStats[liPlayer];
        CGS_ASSERT(lpStatusData, "lpPlayerStatusData");

        const BrnNetwork::RoadRulesRecvData::NetworkPlayerID lThisId =
            lpStatusData->mNetworkPlayerID;

        CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");

        // The X360 cross-references the player to its active-race-car index via the module's
        // player table (sub_8231DD88). When that index equals the local player's active-race-car
        // index this slot is the local player and is marked invalid (mPlayerID = -1).
        const s32 leActiveRaceCarIndex = ModulePlayerActiveRaceCarIndex(mpGameStateModule.Get(), lThisId);

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

        CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");
        CGS_ASSERT(lpRating->mPlayerID != mpGameStateModule.Get()->GetLocalPlayerNetworkID(),
                   "laRivalRatings[liPlayerIndex].mPlayerID != GetLocalPlayerNetworkID()");
        CGS_ASSERT(lpRating->miRivalWeighting != -1,
                   "laRivalRatings[liPlayerIndex].miRivalWeighting != -1");

        CGS_ASSERT(mpScoringSystem.Get(), "mpScoringSystem");
        const s32 leActiveRaceCarIndex =
            ModulePlayerActiveRaceCarIndex(mpScoringSystem.Get(), lpRating->mPlayerID);
        CGS_ASSERT(leActiveRaceCarIndex > -1, "leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leActiveRaceCarIndex < 8, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        const u8* lpPlayerName = GetRivalName(lpRating->mPlayerID);
        CGS_ASSERT(lpPlayerName, "lpPlayerName");

        FlybyData_AddCar(lpFlybyData, leActiveRaceCarIndex, lpPlayerName);
        FlybyData_AddMessage(lpFlybyData, 0 /*off_82CDB924 name table*/, lpPlayerName);

        // Resolve this rival's relationship for the message-flavour decision.
        BrnNetwork::LiveRevengeRelationship* lpRelationship =
            GetLiveRevengeRelationship(lpRating->mPlayerID);
        CGS_ASSERT(lpRelationship, "lpLiveRevengeRelationship");

        CGS_ASSERT(mpScoringSystem.Get(), "mpScoringSystem");
        if (lpRating->mPlayerID == ScoringSystem_GetPointsLeader(mpScoringSystem.Get()))
        {
            FlybyData_AddMessage(lpFlybyData, "PRERACE_POINTS_LEADER", 0);
            // The X360 then re-validates the car count and tags the just-added car as the
            // points-leader style; bounds asserts preserved verbatim.
            CGS_ASSERT(FlybyData_GetNumberOfCars(lpFlybyData) > 0, "miNumberOfCars > 0");
            CGS_ASSERT(FlybyData_GetNumberOfCars(lpFlybyData) <= 3,
                       "miNumberOfCars <= FlybyRivalData::KI_MAX_CARS_IN_FLYBY");
            FlybyData_TagLastCarStyle(lpFlybyData, 2 /*points-leader style*/);
        }
        else if (!lpRating->mbHasValidRelationship)
        {
            AddNoRivalryFlybyMessage(lpRating);
        }
        else
        {
            const s32 liTotalTakedowns =
                lpRelationship->miTakedowns + lpRelationship->miRivalTakedowns;
            CGS_ASSERT(liTotalTakedowns >= 0,
                       "mOverallStats.mPlayerStats.miTakedowns + mOverallStats.mRivalStats.miTakedowns >= 0");
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

}
