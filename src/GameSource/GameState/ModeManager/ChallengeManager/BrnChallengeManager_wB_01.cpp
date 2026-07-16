// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_01.cpp
// ============================================================================
// Wave-B partfile 01 of BrnGameState::ChallengeManager -- the skill-score READ/eval side.
// Bodies reconstructed store-for-store from the X360 ARTIST asm onto the keystone-frozen
// named layout in BrnChallengeManager.h:
//
//   GetScaleFactor           @ 0x8233B070  (all-one/all-two-player debug score scaling)
//   GetCurrentSkillScore     @ 0x8233B2A0  (per-coop-type skill-score read; FLT_MAX sentinel)
//   UpdateCurrentActionScore @ 0x82316968  (writes the maafCurrentActionsScores[arci][action] grid)
//
// The recovered rodata floats are used as their attested literal values (BURNOUT_X360_ARTIST.XEX,
// big-endian): flt_82001C98 == 1.0f, flt_82001DA0 == 0.5f, flt_820037C8 == -1.0f,
// flt_82020AFC == 3.4028235e38f (FLT_MAX, the "uncapped" sentinel).
//
// BLOCKED in this group (reported in funcs_blocked, NOT bodied here):
//   IsSkillScoreCurrentlySuccessful @ 0x823167A0 -- the X360 build drifted this to a
//   SEVEN-parameter function (score f1 shadowing r4; action r5; then r6/r7/r8/r9/r10), whereas
//   the keystone-frozen private declaration in BrnChallengeManager.h has only FIVE params
//   (f32, const ChallengeListEntryAction*, bool, s32, bool). The asm consumes two register
//   inputs (r9 = the maafCumulativeContributions grid ROW index; r10 = the top-level
//   target-comparison branch selector) that have NO parameter to carry in the frozen decl,
//   and it uses r7/r8 as a GetTargetValue index / grid COLUMN index rather than the decl's
//   liNumPlayers / lbIsOnline. Bodying it against the 5-param decl would drop or fabricate
//   that logic; the header cannot be edited, so the function is skipped.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "SharedClasses/DataLists/ChallengeListEntry.h"   // BrnResource::ChallengeListEntry{,Action} accessors

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // GetScaleFactor @ 0x8233B070
    // ------------------------------------------------------------------------
    // When the "make every challenge N-player" debug toggles are set, the freeburn scores
    // are scaled by the challenge's ORIGINAL player count (the pre-hack count backed up in the
    // high nibble of ChallengeListEntry byte +0xD3 == GetOriginalNumPlayers()): x1 per original
    // player when all-one-player, xHALF that when all-two-player. The GetChallengeIndex call is
    // kept for its (debug-print/assert) side effects; its result is discarded in the asm.
    // Default scale is 1.0f (flt_82001C98).
    f32 ChallengeManager::GetScaleFactor()
    {
        f32 lfScaleFactor = 1.0f;                                   // flt_82001C98

        if ( mbChallengesAreAllOnePlayer )                          // byte_82FAD9C1
        {
            GetChallengeIndex( mpCurrentChallenge->GetChallengeID() );
            return static_cast<f32>( mpCurrentChallenge->GetOriginalNumPlayers() );
        }
        else if ( mbChallengesAreAllTwoPlayer )                     // byte_82FAD9C0
        {
            GetChallengeIndex( mpCurrentChallenge->GetChallengeID() );
            return static_cast<f32>( mpCurrentChallenge->GetOriginalNumPlayers() ) * 0.5f; // flt_82001DA0
        }

        return lfScaleFactor;
    }

    // ------------------------------------------------------------------------
    // GetCurrentSkillScore @ 0x8233B2A0
    // ------------------------------------------------------------------------
    // Reads the freeburn skill's current running value for one challenge action. When the
    // player is currently at the action's location (lbIsLocationOk), the score is the action's
    // cumulative running total scaled by GetScaleFactor() and the frame step. Otherwise the
    // per-skill banked/active values are combined per the action's BANK_FOR_SUCCESS modifier and
    // its coop type; if the skill has no active/banked value this frame the "uncapped" sentinel
    // is returned (-1.0f online, FLT_MAX offline).
    f32 ChallengeManager::GetCurrentSkillScore( s32 liActionIndex, bool lbIsLocationOk,
                                                EFreeburnSkill leFreeburnSkill,
                                                BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoopType,
                                                bool lbIsOnline, f32 lfTimeStep )
    {
        if ( lbIsLocationOk )
        {
            CGS_ASSERT( liActionIndex < mpCurrentChallenge->GetNumActions(),
                        "liActionIndex < mpCurrentChallenge->GetNumActions()" );

            const f32 lfScaleFactor = GetScaleFactor();
            return lfScaleFactor * mafCumulativeActionScores[ liActionIndex ] * lfTimeStep;
        }

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction( liActionIndex );

        if ( ( lpAction->GetModifier() & BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
                 == BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
        {
            if ( mabBankedSkillThisFrame[ leFreeburnSkill ] )
            {
                return mafBankedActionScores[ leFreeburnSkill ] * lfTimeStep;
            }
        }
        else if ( mabActiveSkillThisFrame[ leFreeburnSkill ] )
        {
            if ( leCoopType == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION )
            {
                if ( mabBankedSkillThisFrame[ leFreeburnSkill ] )
                {
                    return mafBankedActionScores[ leFreeburnSkill ] * lfTimeStep;
                }
                return ( mafActiveSkillValue[ leFreeburnSkill ] + mafBankedActionScores[ leFreeburnSkill ] ) * lfTimeStep;
            }
            return mafActiveSkillValue[ leFreeburnSkill ] * lfTimeStep;
        }

        if ( lbIsOnline )
        {
            return -1.0f;              // flt_820037C8
        }
        return 3.4028235e38f;         // flt_82020AFC (FLT_MAX -- the uncapped sentinel)
    }

    // ------------------------------------------------------------------------
    // UpdateCurrentActionScore @ 0x82316968
    // ------------------------------------------------------------------------
    // Snapshots the freeburn skill's live value into the local player's per-action score grid
    // (maafCurrentActionsScores[arci][action]) scaled by lfScore. Does nothing while the player
    // is at the action's location (lbIsLocationOk) -- the grid only tracks off-location running
    // totals. The banked-vs-active selection mirrors GetCurrentSkillScore: BANK_FOR_SUCCESS
    // actions read the banked score; other actions read the active value (plus banked, for
    // INDIVIDUAL_ACCUMULATION coop).
    void ChallengeManager::UpdateCurrentActionScore( s32 liActionIndex, bool lbIsLocationOk,
                                                     EFreeburnSkill leFreeburnSkill,
                                                     EActiveRaceCarIndex leActiveRaceCarIndex,
                                                     f32 lfScore )
    {
        CGS_ASSERT( ( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )
                        && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                    "( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )" );

        if ( lbIsLocationOk )
        {
            return;
        }

        CGS_ASSERT( mpCurrentChallenge, "mpCurrentChallenge" );
        CGS_ASSERT( liActionIndex < mpCurrentChallenge->GetNumActions(),
                    "liActionIndex < mpCurrentChallenge->GetNumActions()" );

        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction( liActionIndex );

        f32 lfValue;
        if ( ( lpAction->GetModifier() & BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
                 == BrnResource::ChallengeListEntryAction::KX_MODIFIER_BANK_FOR_SUCCESS )
        {
            if ( !mabBankedSkillThisFrame[ leFreeburnSkill ] )
            {
                return;
            }
            lfValue = mafBankedActionScores[ leFreeburnSkill ];
        }
        else
        {
            if ( !mabActiveSkillThisFrame[ leFreeburnSkill ] )
            {
                return;
            }

            lpAction = mpCurrentChallenge->GetAction( liActionIndex );
            if ( lpAction->GetCoopType() == BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION )
            {
                if ( mabBankedSkillThisFrame[ leFreeburnSkill ] )
                {
                    lfValue = mafBankedActionScores[ leFreeburnSkill ];
                }
                else
                {
                    lfValue = mafActiveSkillValue[ leFreeburnSkill ] + mafBankedActionScores[ leFreeburnSkill ];
                }
            }
            else
            {
                lfValue = mafActiveSkillValue[ leFreeburnSkill ];
            }
        }

        maafCurrentActionsScores[ leActiveRaceCarIndex ][ liActionIndex ] = lfValue * lfScore;
    }
}
