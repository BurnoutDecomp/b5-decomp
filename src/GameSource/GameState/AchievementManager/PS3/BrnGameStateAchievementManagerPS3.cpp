// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/PS3/BrnGameStateAchievementManagerPS3.cpp
// ============================================================================
// Reconstruction of the eight BrnGameState::AchievementManagerBase gameplay-event
// hooks whose X360-attested bodies live in the PS3 unit. As in the platform-neutral
// base TU, each hook fires its achievement id through the two protected virtuals the
// first time the id is NOT yet earnt and the event's threshold is met:
//
//   IsAchievementEarnt(id)  == X360 vtable slot 1 ( (*(*this + 4))(this, id) )
//   AchievementEarnt(id)    == X360 vtable slot 0 ( (**this)(this, id)       )
//
// The achievement ids are referenced through the X360-asm-attested E_X360_ACHIEVEMENT_*
// constants below. As documented in the base home, the X360 SKU numbers its
// achievements DIFFERENTLY from the PS3 DWARF EAchievement enum, so the raw integer
// ids the X360 passes (49,47,48,43,41,42,36,35,38,37,40,39,...) cannot be spelled with
// the PS3 enumerator names/values. These file-local constants keep every id a NAMED
// value (no bare integer literals in the bodies) while preserving the X360 numbering.
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

namespace BrnGameState
{

namespace
{
    // ---- X360-attested achievement ids (raw integers from the X360 .text). ----
    // FLAG: X360-SKU numbering; intentionally NOT the PS3 EAchievement names/values.
    const EAchievement E_X360_ACHIEVEMENT_CAUGHT_FEVER             = static_cast<EAchievement>(49); // OnCaughtFever (0x31)
    // (id 47, OnFreeburnChallengeBlockComplete, MOVED with its body to
    //  ../BrnGameStateAchievementManagerBase_Freeburn.cpp)
    const EAchievement E_X360_ACHIEVEMENT_FIND_ALL_EVENTS          = static_cast<EAchievement>(48); // OnFreeburnSkillzTotalChange (0x30)
    const EAchievement E_X360_ACHIEVEMENT_MUGSHOT_ADDED            = static_cast<EAchievement>(43); // OnMugshotAdded (0x2B)
    const EAchievement E_X360_ACHIEVEMENT_MUGSHOT_SENT_5           = static_cast<EAchievement>(41); // OnMugshotSent / OnOnlineRaceComplete-band (0x29)
    const EAchievement E_X360_ACHIEVEMENT_MUGSHOT_SENT_50          = static_cast<EAchievement>(42); // OnMugshotSent (0x2A)
    const EAchievement E_X360_ACHIEVEMENT_ONLINE_RACE_FLAGGED      = static_cast<EAchievement>(36); // OnOnlineRaceComplete (0x24)
    const EAchievement E_X360_ACHIEVEMENT_ONLINE_RACE_COMPLETE     = static_cast<EAchievement>(35); // OnOnlineRaceComplete (0x23)
    const EAchievement E_X360_ACHIEVEMENT_ONLINE_RACE_CHAIN_B      = static_cast<EAchievement>(38); // OnOnlineRaceComplete (0x26)
    const EAchievement E_X360_ACHIEVEMENT_ONLINE_RACE_CHAIN_A      = static_cast<EAchievement>(37); // OnOnlineRaceComplete (0x25)
    const EAchievement E_X360_ACHIEVEMENT_RIVAL_ADDED_1            = static_cast<EAchievement>(40); // OnRivalAdded (0x28)
    const EAchievement E_X360_ACHIEVEMENT_RIVAL_ADDED_50           = static_cast<EAchievement>(39); // OnRivalAdded (0x27)

    // Thresholds (X360-attested compare immediates).
    const s32 KI_FREEBURN_SKILLZ_NETWORK_PLAYERS = 8;     // OnFreeburnSkillzTotalChange (cmpwi r30,8)
    const f32 KF_FREEBURN_SKILLZ_TOTAL           = 6.0f;  // OnFreeburnSkillzTotalChange (fcmpu >= 6.0)
    const u32 KU_MUGSHOT_ADDED_COUNT             = 50u;   // OnMugshotAdded (cmplwi r30,0x32 -- unsigned)
    const s32 KI_MUGSHOT_SENT_5                  = 5;     // OnMugshotSent (cmpwi r30,5 -- signed)
    const s32 KI_MUGSHOT_SENT_50                 = 50;    // OnMugshotSent (cmpwi r30,0x32 -- signed)
    const s32 KI_ONLINE_RACE_GAME_MODE           = 8;     // OnOnlineRaceComplete (cmpwi r4,8)
    const s32 KI_ONLINE_RACE_CHAIN_A             = 10;    // OnOnlineRaceComplete (cmpwi r29,0xA -- a4)
    const s32 KI_ONLINE_RACE_CHAIN_B             = 20;    // OnOnlineRaceComplete (cmpwi r30,0x14 -- a5)
    const s32 KI_RIVAL_ADDED_1                   = 1;     // OnRivalAdded (cmpwi r30,1)
    const s32 KI_RIVAL_ADDED_50                  = 50;    // OnRivalAdded (cmpwi r30,0x32)

    // (The OnFreeburnChallengeComplete achievement table -- its FLOOR/FLAG note, the
    //  FreeburnChallengeAchievementEntry struct and the unrecoverable-rowset placeholder -- MOVED
    //  with that body to ../BrnGameStateAchievementManagerBase_Freeburn.cpp.)
}

// ----------------------------------------------------------------------------
// OnCaughtFever  (X360 0x8235B590)
//   Fires the "caught fever" achievement (id 49) unconditionally, first time only.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnCaughtFever()
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_CAUGHT_FEVER))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_CAUGHT_FEVER);
    }
}

// ----------------------------------------------------------------------------
// OnFreeburnChallengeBlockComplete  (X360 0x8235B370)
// OnFreeburnChallengeComplete       (X360 0x8235B2D8)
//   ⭐ MOVED 2026-09-07 (challenge-manager mount) to the sibling partfile
//   AchievementManager/BrnGameStateAchievementManagerBase_Freeburn.cpp -- MOVED, not copied, so
//   there is no ODR fork. Both hooks are on the freeburn-challenge path the ChallengeManager
//   mount lights up, and this PS3 unit is not on the build list; the partfile carries them (and
//   the unrecoverable-table FLOOR note, and the id-47 constant) for zero new externals. Fold them
//   back here if this unit is ever mounted whole.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// OnFreeburnSkillzTotalChange  (X360 0x8235B500)
//   Fires FIND_ALL_EVENTS (id 48) when a local player's free-burn skillz total
//   changes in an 8-network-player session and the new total reaches 6.0.
//   Signature pinned against BurnoutSkillzManager::UpdateBurnoutSkillzTotals
//   (the committed call-site passes (GetNumberOfNetworkPlayers(), lfNewTotal)),
//   which matches the existing (s32, f32) declaration -- bodied as committed.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFreeburnSkillzTotalChange(s32 liNumberOfNetworkPlayers,
                                                         f32 lfNewSkillzTotal)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_EVENTS)
        && liNumberOfNetworkPlayers == KI_FREEBURN_SKILLZ_NETWORK_PLAYERS
        && lfNewSkillzTotal >= KF_FREEBURN_SKILLZ_TOTAL)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_FIND_ALL_EVENTS);
    }
}

// ----------------------------------------------------------------------------
// OnMugshotAdded  (X360 0x8235B3D0)
//   Fires id 43 when the (unsigned) mugshot count reaches 50.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnMugshotAdded(u32 luMugshotCount)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_MUGSHOT_ADDED)
        && luMugshotCount >= KU_MUGSHOT_ADDED_COUNT)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_MUGSHOT_ADDED);
    }
}

// ----------------------------------------------------------------------------
// OnMugshotSent  (X360 0x8235B5F0)
//   Two independent (signed) thresholds: id 41 at >= 5 sent, id 42 at >= 50 sent.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnMugshotSent(s32 liMugshotCount)
{
    if (liMugshotCount >= KI_MUGSHOT_SENT_5)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_MUGSHOT_SENT_5))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_MUGSHOT_SENT_5);
        }
    }

    if (liMugshotCount >= KI_MUGSHOT_SENT_50)
    {
        if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_MUGSHOT_SENT_50))
        {
            AchievementEarnt(E_X360_ACHIEVEMENT_MUGSHOT_SENT_50);
        }
    }
}

// ----------------------------------------------------------------------------
// OnOnlineRaceComplete  (X360 0x8235B6A8)
//   - id 36: game-mode type == 8 AND the flag byte is set.
//   - id 35: fired unconditionally (online race completed).
//   - id 38: chain-B value (a5/r7) reaches 20.
//   - id 37: chain-A value (a4/r6) reaches 10.
//   Arg mapping pinned from the asm register setup (r4=game mode, r5=flag byte,
//   r6=chain-A, r7=chain-B; all signed cmpwi compares).
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnOnlineRaceComplete(s32 liGameModeType, bool lbFlag,
                                                  s32 liChainA, s32 liChainB)
{
    if (liGameModeType == KI_ONLINE_RACE_GAME_MODE
        && lbFlag
        && !IsAchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_FLAGGED))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_FLAGGED);
    }

    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_COMPLETE))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_COMPLETE);
    }

    if (liChainB >= KI_ONLINE_RACE_CHAIN_B
        && !IsAchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_CHAIN_B))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_CHAIN_B);
    }

    if (liChainA >= KI_ONLINE_RACE_CHAIN_A
        && !IsAchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_CHAIN_A))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_ONLINE_RACE_CHAIN_A);
    }
}

// ----------------------------------------------------------------------------
// OnRivalAdded  (X360 0x8235B448)
//   - id 40: rival count reaches 1.
//   - id 39: rival count reaches 50.
//   (Both signed cmpwi compares.)
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnRivalAdded(s32 liRivalCount)
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_RIVAL_ADDED_1)
        && liRivalCount >= KI_RIVAL_ADDED_1)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_RIVAL_ADDED_1);
    }

    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_RIVAL_ADDED_50)
        && liRivalCount >= KI_RIVAL_ADDED_50)
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_RIVAL_ADDED_50);
    }
}

}
