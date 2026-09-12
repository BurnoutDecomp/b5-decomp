// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/PS3/BrnGameStateAchievementManagerPS3.cpp
// ============================================================================
// Reconstruction of the eight BrnGameState::AchievementManagerBase gameplay-event
// hooks whose console-attested bodies live in the PS3 unit. As in the platform-neutral
// base TU, each hook fires its achievement id through the two protected virtuals the
// first time the id is NOT yet earnt and the event's threshold is met:
//
//   IsAchievementEarnt(id)  == the second virtual of the pair
//   AchievementEarnt(id)    == the first virtual of the pair
//
// The achievement ids are referenced through the console-attested E_RETAIL_ACHIEVEMENT_*
// constants below. As documented in the base home, the console SKU numbers its
// achievements DIFFERENTLY from the declaration record EAchievement enum, so the raw integer
// ids the console passes (49,47,48,43,41,42,36,35,38,37,40,39,...) cannot be spelled with
// the PS3 enumerator names/values. These file-local constants keep every id a NAMED
// value (no bare integer literals in the bodies) while preserving the console numbering.
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

namespace BrnGameState
{

namespace
{
    // ---- console-attested achievement ids (raw integers from the console .text). ----
    // FLAG: retail-SKU numbering; intentionally NOT the EAchievement enum names/values.
    const EAchievement E_RETAIL_ACHIEVEMENT_CAUGHT_FEVER             = static_cast<EAchievement>(49); // OnCaughtFever (0x31)
    // (id 47, OnFreeburnChallengeBlockComplete, MOVED with its body to
    //../BrnGameStateAchievementManagerBase_Freeburn.cpp)
    const EAchievement E_RETAIL_ACHIEVEMENT_FIND_ALL_EVENTS          = static_cast<EAchievement>(48); // OnFreeburnSkillzTotalChange (0x30)
    const EAchievement E_RETAIL_ACHIEVEMENT_MUGSHOT_ADDED            = static_cast<EAchievement>(43); // OnMugshotAdded (0x2B)
    const EAchievement E_RETAIL_ACHIEVEMENT_MUGSHOT_SENT_5           = static_cast<EAchievement>(41); // OnMugshotSent / OnOnlineRaceComplete-band (0x29)
    const EAchievement E_RETAIL_ACHIEVEMENT_MUGSHOT_SENT_50          = static_cast<EAchievement>(42); // OnMugshotSent (0x2A)
    const EAchievement E_RETAIL_ACHIEVEMENT_ONLINE_RACE_FLAGGED      = static_cast<EAchievement>(36); // OnOnlineRaceComplete (0x24)
    const EAchievement E_RETAIL_ACHIEVEMENT_ONLINE_RACE_COMPLETE     = static_cast<EAchievement>(35); // OnOnlineRaceComplete (0x23)
    const EAchievement E_RETAIL_ACHIEVEMENT_ONLINE_RACE_CHAIN_B      = static_cast<EAchievement>(38); // OnOnlineRaceComplete (0x26)
    const EAchievement E_RETAIL_ACHIEVEMENT_ONLINE_RACE_CHAIN_A      = static_cast<EAchievement>(37); // OnOnlineRaceComplete (0x25)
    const EAchievement E_RETAIL_ACHIEVEMENT_RIVAL_ADDED_1            = static_cast<EAchievement>(40); // OnRivalAdded (0x28)
    const EAchievement E_RETAIL_ACHIEVEMENT_RIVAL_ADDED_50           = static_cast<EAchievement>(39); // OnRivalAdded (0x27)

    // Thresholds (console-attested compare immediates).
    const s32 KI_FREEBURN_SKILLZ_NETWORK_PLAYERS = 8;     // OnFreeburnSkillzTotalChange
    const f32 KF_FREEBURN_SKILLZ_TOTAL           = 6.0f;  // OnFreeburnSkillzTotalChange (fcmpu >= 6.0)
    const u32 KU_MUGSHOT_ADDED_COUNT             = 50u;   // OnMugshotAdded (unsigned compare)
    const s32 KI_MUGSHOT_SENT_5                  = 5;     // OnMugshotSent (signed compare)
    const s32 KI_MUGSHOT_SENT_50                 = 50;    // OnMugshotSent (signed compare)
    const s32 KI_ONLINE_RACE_GAME_MODE           = 8;     // OnOnlineRaceComplete
    const s32 KI_ONLINE_RACE_CHAIN_A             = 10;    // OnOnlineRaceComplete (the chain-A argument)
    const s32 KI_ONLINE_RACE_CHAIN_B             = 20;    // OnOnlineRaceComplete (the chain-B argument)
    const s32 KI_RIVAL_ADDED_1                   = 1;     // OnRivalAdded
    const s32 KI_RIVAL_ADDED_50                  = 50;    // OnRivalAdded

    // (The OnFreeburnChallengeComplete achievement table -- its FLOOR/FLAG note, the
    //  FreeburnChallengeAchievementEntry struct and the unrecoverable-rowset placeholder -- MOVED
    //  with that body to ../BrnGameStateAchievementManagerBase_Freeburn.cpp.)
}

// ----------------------------------------------------------------------------
// OnCaughtFever
//   Fires the "caught fever" achievement (id 49) unconditionally, first time only.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnCaughtFever()
{
    if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_CAUGHT_FEVER))
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_CAUGHT_FEVER);
    }
}

// ----------------------------------------------------------------------------
// OnFreeburnChallengeBlockComplete
// OnFreeburnChallengeComplete
//   ⭐ MOVED 2026-09-07 (challenge-manager mount) to the sibling partfile
//   AchievementManager/BrnGameStateAchievementManagerBase_Freeburn.cpp -- MOVED, not copied, so
//   there is no ODR fork. Both hooks are on the freeburn-challenge path the ChallengeManager
//   mount lights up, and this PS3 unit is not on the build list; the partfile carries them (and
//   the unrecoverable-table FLOOR note, and the id-47 constant) for zero new externals. Fold them
//   back here if this unit is ever mounted whole.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// OnFreeburnSkillzTotalChange
//   Fires FIND_ALL_EVENTS (id 48) when a local player's free-burn skillz total
//   changes in an 8-network-player session and the new total reaches 6.0.
//   Signature pinned against BurnoutSkillzManager::UpdateBurnoutSkillzTotals
//   (the committed call-site passes (GetNumberOfNetworkPlayers, lfNewTotal)),
//   which matches the existing (s32, f32) declaration -- bodied as committed.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFreeburnSkillzTotalChange(s32 liNumberOfNetworkPlayers,
                                                         f32 lfNewSkillzTotal)
{
    if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_FIND_ALL_EVENTS)
        && liNumberOfNetworkPlayers == KI_FREEBURN_SKILLZ_NETWORK_PLAYERS
        && lfNewSkillzTotal >= KF_FREEBURN_SKILLZ_TOTAL)
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_FIND_ALL_EVENTS);
    }
}

// ----------------------------------------------------------------------------
// OnMugshotAdded
//   Fires id 43 when the (unsigned) mugshot count reaches 50.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnMugshotAdded(u32 luMugshotCount)
{
    if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_MUGSHOT_ADDED)
        && luMugshotCount >= KU_MUGSHOT_ADDED_COUNT)
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_MUGSHOT_ADDED);
    }
}

// ----------------------------------------------------------------------------
// OnMugshotSent
//   Two independent (signed) thresholds: id 41 at >= 5 sent, id 42 at >= 50 sent.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnMugshotSent(s32 liMugshotCount)
{
    if (liMugshotCount >= KI_MUGSHOT_SENT_5)
    {
        if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_MUGSHOT_SENT_5))
        {
            AchievementEarnt(E_RETAIL_ACHIEVEMENT_MUGSHOT_SENT_5);
        }
    }

    if (liMugshotCount >= KI_MUGSHOT_SENT_50)
    {
        if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_MUGSHOT_SENT_50))
        {
            AchievementEarnt(E_RETAIL_ACHIEVEMENT_MUGSHOT_SENT_50);
        }
    }
}

// ----------------------------------------------------------------------------
// OnOnlineRaceComplete
//   - id 36: game-mode type == 8 AND the flag byte is set.
//   - id 35: fired unconditionally (online race completed).
//   - id 38: the chain-B value reaches 20.
//   - id 37: the chain-A value reaches 10.
//   Argument mapping pinned from the console's own call setup: game mode, flag byte,
//   chain-A, chain-B in that order; every threshold is a signed compare.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnOnlineRaceComplete(s32 liGameModeType, bool lbFlag,
                                                  s32 liChainA, s32 liChainB)
{
    if (liGameModeType == KI_ONLINE_RACE_GAME_MODE
        && lbFlag
        && !IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_FLAGGED))
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_FLAGGED);
    }

    if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_COMPLETE))
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_COMPLETE);
    }

    if (liChainB >= KI_ONLINE_RACE_CHAIN_B
        && !IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_CHAIN_B))
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_CHAIN_B);
    }

    if (liChainA >= KI_ONLINE_RACE_CHAIN_A
        && !IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_CHAIN_A))
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_ONLINE_RACE_CHAIN_A);
    }
}

// ----------------------------------------------------------------------------
// OnRivalAdded
//   - id 40: rival count reaches 1.
//   - id 39: rival count reaches 50.
//   (Both are signed compares.)
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnRivalAdded(s32 liRivalCount)
{
    if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_RIVAL_ADDED_1)
        && liRivalCount >= KI_RIVAL_ADDED_1)
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_RIVAL_ADDED_1);
    }

    if (!IsAchievementEarnt(E_RETAIL_ACHIEVEMENT_RIVAL_ADDED_50)
        && liRivalCount >= KI_RIVAL_ADDED_50)
    {
        AchievementEarnt(E_RETAIL_ACHIEVEMENT_RIVAL_ADDED_50);
    }
}

}
