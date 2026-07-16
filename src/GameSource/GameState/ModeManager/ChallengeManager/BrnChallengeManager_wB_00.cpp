// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_00.cpp
// ============================================================================
// Wave-B partfile for BrnGameState::ChallengeManager -- the skill-score WRITE core.
// Store-for-store reconstructions of three X360 ARTIST methods over the keystone's NAMED
// members (see BrnChallengeManager.h for the layout evidence):
//
//   BankSkillScore               @ 0x82317140  (DWARF :692)  -- the actual bank primitive
//   SetCurrentSkillScore         @ 0x82324290  (DWARF :686)  -- per-frame skill-score writer
//   UpdateLocationOKStatusChange @ 0x82323FF8  (DWARF :590)  -- on location-exit, bank pending
//
// BankSkillScore is bodied first because both SetCurrentSkillScore and
// UpdateLocationOKStatusChange call it. All three use the X360-recovered per-skill flag
// tables (KAB_FREEBURN_SKILL_IS_CONTINUOUS / KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT) and
// the mScoresSetThisFrameBitArray FastBitArray<38>; the per-skill loops use the header's
// EFreeburnSkill operator++ (which reproduces the inlined h:113 count assert).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "SharedClasses/DataLists/ChallengeListEntry.h"   // BrnResource::ChallengeListEntryAction::{GetCoopType, EChallengeCoopType}, ChallengeListEntry::{GetAction, GetNumActions}

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// BankSkillScore -- X360 0x82317140. Commit one skill's running value to the banked stores
// for the current challenge action. Non-negative scores only (a negative score is a no-op:
// the X360 compares against flt_82001CC0 (0.0) and skips the whole body when score < 0.0).
// On a valid score it flags the skill banked + active this frame, records the running value,
// then adds-or-replaces the per-skill banked-action total depending on the current action's
// co-op type (INDIVIDUAL_ACCUMULATION accumulates; every other type overwrites).
// ----------------------------------------------------------------------------
void ChallengeManager::BankSkillScore(EFreeburnSkill leFreeburnSkill, f32 lfScore)
{
    if (lfScore >= 0.0f)   // X360 fcmpu f31,flt_82001CC0(0.0); blt -> skip body
    {
        mabBankedSkillThisFrame[leFreeburnSkill] = true;   // X360 stb 1,+0xE50 (base + skill)
        mabActiveSkillThisFrame[leFreeburnSkill] = true;   // X360 stb 1,+0xE76 (base + skill)
        mafActiveSkillValue[leFreeburnSkill]     = lfScore; // X360 stfsx +0xE9C ((skill+935)*4)

        // action = mpCurrentChallenge->GetAction(miCurrentChallengeAction) (X360 lwz +0xE0C,+0xE10)
        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(miCurrentChallengeAction);

        if (lpAction->GetCoopType() ==
            BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION)  // X360 lbz action+1 == 2
        {
            mafBankedActionScores[leFreeburnSkill] += lfScore;  // X360 lfsx/fadds/stfsx +0x4E8 ((skill+314)*4)
        }
        else
        {
            mafBankedActionScores[leFreeburnSkill] = lfScore;   // X360 stfsx +0x4E8
        }
    }
}

// ----------------------------------------------------------------------------
// SetCurrentSkillScore -- X360 0x82324290. The per-frame entry point skill sources call to
// report a skill value. Range-asserts the skill id (non-fatal), then:
//   * If the manager is RUNNING and the current action's location is ok:
//       - bank-immediately  -> BankSkillScore(skill, score - cachedOnLocationEnter[skill])
//       - otherwise         -> flag active this frame + store the (score - cached) running value
//   * Otherwise (not RUNNING, or location not ok): if the skill is a CONTINUOUS-value skill,
//     re-cache the incoming score as the location-enter baseline.
// Finally, for CONTINUOUS-value skills, set this frame's "score was set" bit.
//
// The two "cache the score" arms below are the single shared X360 tail (LABEL_23) that both
// the non-RUNNING branch and the RUNNING-but-location-not-ok branch fall into.
// ----------------------------------------------------------------------------
void ChallengeManager::SetCurrentSkillScore(EFreeburnSkill leFreeburnSkill, f32 lfScore, bool lbBankImmediately)
{
    // Non-fatal range asserts (X360 cmpwi r29,0 / cmpwi 0x26; both continue on failure). The
    // upper bound is the X360-drift count (38) per KI_FREEBURN_SKILL_COUNT_X360; the verbatim
    // string names the enumerator E_FREEBURN_SKILL_COUNT.
    CGS_ASSERT(static_cast<s32>(leFreeburnSkill) >= 0, "leFreeburnSkill >= 0");
    CGS_ASSERT(static_cast<s32>(leFreeburnSkill) < KI_FREEBURN_SKILL_COUNT_X360,
               "leFreeburnSkill < E_FREEBURN_SKILL_COUNT");

    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz +0xE08 == 2
    {
        CGS_ASSERT(mpCurrentChallenge != 0, "mpCurrentChallenge");
        CGS_ASSERT(miCurrentChallengeAction < mpCurrentChallenge->GetNumActions(),
                   "miCurrentChallengeAction < mpCurrentChallenge->GetNumActions()");

        if (mabIsLocationOk[miCurrentChallengeAction])   // X360 lbz +0xFD8 (base + miCurrentChallengeAction)
        {
            if (lbBankImmediately)   // X360 clrlwi r27,24; bne
            {
                BankSkillScore(leFreeburnSkill,
                               lfScore - mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill]);  // X360 lfsx +0xF34; fsubs; bl BankSkillScore
            }
            else
            {
                mabActiveSkillThisFrame[leFreeburnSkill] = true;   // X360 stb 1,+0xE76
                mafActiveSkillValue[leFreeburnSkill] =
                    lfScore - mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill];  // X360 lfsx +0xF34; fsubs; stfsx +0xE9C
            }
        }
        else
        {
            if (KAB_FREEBURN_SKILL_IS_CONTINUOUS[leFreeburnSkill])   // X360 LABEL_23: lbzx byte_8202132C
            {
                mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill] = lfScore;  // X360 stfsx +0xF34
            }
        }
    }
    else
    {
        if (KAB_FREEBURN_SKILL_IS_CONTINUOUS[leFreeburnSkill])   // X360 LABEL_23 (non-RUNNING fall-through)
        {
            mafCachedActiveSkillValueOnLocationEnter[leFreeburnSkill] = lfScore;
        }
    }

    if (KAB_FREEBURN_SKILL_IS_CONTINUOUS[leFreeburnSkill])   // X360 2nd lbzx byte_8202132C
    {
        // X360 inlines FastBitArray<38>::SetBit here (with the "max bits: 38" range assert);
        // call the committed container method (spec pitfall 5).
        mScoresSetThisFrameBitArray.SetBit(static_cast<u32>(leFreeburnSkill));
    }
}

// ----------------------------------------------------------------------------
// UpdateLocationOKStatusChange -- X360 0x82323FF8. Called by UpdateAction on a location-ok
// transition. On an ok -> not-ok edge (was ok, now not ok), walk every skill and bank the
// ones that carry a non-zero running value AND opt into banking on location exit
// (KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT). Returns whether anything was banked. The
// per-skill loop uses the header's EFreeburnSkill operator++ (which carries the inlined
// "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert at BrnChallengeManager.h:113).
// ----------------------------------------------------------------------------
bool ChallengeManager::UpdateLocationOKStatusChange(bool lbWasLocationOk, bool lbIsLocationOk)
{
    bool lbBankedAny = false;   // X360 li r25,0

    if (!lbIsLocationOk && lbWasLocationOk)   // X360 clrlwi r5; bne skip / clrlwi r4; beq skip
    {
        for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
             static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
             leSkill++)
        {
            if (mafActiveSkillValue[leSkill] != 0.0f &&               // X360 lfs +0xE9C; fcmpu vs flt_82001CC0(0.0)
                KAB_FREEBURN_SKILL_BANK_ON_LOCATION_EXIT[leSkill])    // X360 lbzx byte_82021354
            {
                BankSkillScore(leSkill, mafActiveSkillValue[leSkill]); // X360 f1 == the just-loaded running value
                lbBankedAny = true;                                    // X360 li r25,1
            }
        }
    }

    return lbBankedAny;
}

} // namespace BrnGameState
