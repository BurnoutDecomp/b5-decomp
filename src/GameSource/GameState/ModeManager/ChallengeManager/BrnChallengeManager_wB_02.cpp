// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wB_02.cpp
// ============================================================================
// Wave-B partfile 02 of BrnGameState::ChallengeManager -- the per-frame / per-action
// clear helpers. Bodies reconstructed store-for-store from the X360 ARTIST asm onto the
// keystone-frozen named layout in BrnChallengeManager.h:
//
//   ClearFreeburnSkillsThisFrame  @ 0x82323498  (per-skill scratch clear)
//   ClearPlayerSuccessData        @ 0x82323158  (per-player success/contribution clear)
//   ResetCurrentChallengeData     @ 0x823244F8  (per-action scratch reset)
//
// The inlined FastBitArray<38> range-assert StrStream machinery around the skill loops is
// NOT reproduced -- the committed container methods (IsBitSet / UnSetAll) carry it (spec
// pitfall 5). The per-skill loops drive the frozen EFreeburnSkill operator++ (which carries
// the verbatim "leEnumIndex <= E_FREEBURN_SKILL_COUNT" assert vs 38); the ResetCurrentChallengeData
// player loop drives the committed EActiveRaceCarIndex operator++ (its own count assert).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // ClearFreeburnSkillsThisFrame @ 0x82323498
    // ------------------------------------------------------------------------
    // Called once a frame (Construct / UpdateFreeburnSkillsThisFrame): drop every skill's
    // per-frame banked/active flags and running value, and drop the cached location-enter
    // value for any skill whose score was NOT set this frame, then reset the whole
    // "scores set this frame" bit array.
    void ChallengeManager::ClearFreeburnSkillsThisFrame()
    {
        for (EFreeburnSkill le = E_FREEBURN_SKILL_START; (s32)le < KI_FREEBURN_SKILL_COUNT_X360; le++)
        {
            mabActiveSkillThisFrame[le] = 0;
            mabBankedSkillThisFrame[le] = 0;
            mafActiveSkillValue[le]     = 0.0f;

            if (!mScoresSetThisFrameBitArray.IsBitSet(le))
                mafCachedActiveSkillValueOnLocationEnter[le] = 0.0f;
        }

        mScoresSetThisFrameBitArray.UnSetAll();
    }

    // ------------------------------------------------------------------------
    // ClearPlayerSuccessData @ 0x82323158
    // ------------------------------------------------------------------------
    // Wipe every per-player / per-action success-tracking scratch field back to zero
    // (challenge begin/end, results, remote-end). The full mafBankedActionScores[38] sweep
    // is emitted redundantly inside the per-action loop by the X360 build -- kept verbatim.
    void ChallengeManager::ClearPlayerSuccessData()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "CLEARING CUMULATIVE CONTRIBUTIONS\n";

        for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; liPlayer++)
        {
            maPlayerSuccessUpdateArray[liPlayer].UnSetAll();
            mabReceivedSuccessUpdates[liPlayer]     = 0;
            mabPlayerStartedChallenge[liPlayer]     = 0;
            maiCrashedWithChallengePlayer[liPlayer] = 0;

            for (s32 liAction = 0; liAction < KI_MAX_CHALLENGE_ACTIONS; liAction++)
            {
                maafCurrentActionsScores[liPlayer][liAction]   = 0.0f;
                maaePlayersSuccessStatus[liPlayer][liAction]   = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;
                maafCumulativeContributions[liPlayer][liAction] = 0.0f;
                maiRemainingTarget[liAction]                   = 0;
                mafCumulativeActionScores[liAction]            = 0.0f;
                mabIsLocationOk[liAction]                      = 0;
                mabIndividualActionsSuccessUpdateSent[liAction] = 0;

                for (EFreeburnSkill le = E_FREEBURN_SKILL_START; (s32)le < KI_FREEBURN_SKILL_COUNT_X360; le++)
                    mafBankedActionScores[le] = 0.0f;
            }
        }
    }

    // ------------------------------------------------------------------------
    // ResetCurrentChallengeData @ 0x823244F8
    // ------------------------------------------------------------------------
    // Reset the per-action scratch for the current challenge from liActionIndex onward:
    // re-seed each remaining action's target from its ChallengeListEntryAction target value,
    // clear the "individual success update sent" flag, then wipe the per-player cumulative
    // grids (and the redundant banked-action-score sweep) and clear the billboard tally.
    void ChallengeManager::ResetCurrentChallengeData(s32 liActionIndex)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "CLEARING CUMULATIVE CONTRIBUTIONS\n";

        for (s32 liAction = liActionIndex; liAction < mpCurrentChallenge->GetNumActions(); liAction++)
        {
            // (The liActionIndex range asserts the X360 emits here are the ones INLINED from the
            // committed const ChallengeListEntry::GetAction below -- do not duplicate them.)
            const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liAction);
            maiRemainingTarget[liAction]                    = lpAction->GetTargetValue(0);
            mabIndividualActionsSuccessUpdateSent[liAction] = 0;
        }

        for (EActiveRaceCarIndex le = E_ACTIVE_RACE_CAR_INDEX_0; (s32)le < E_ACTIVE_RACE_CAR_INDEX_COUNT; le++)
        {
            for (s32 liAction = liActionIndex; liAction < mpCurrentChallenge->GetNumActions(); liAction++)
            {
                maafCumulativeContributions[le][liAction] = 0.0f;
                maaePlayersSuccessStatus[le][liAction]    = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_NONE;

                for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START; (s32)leSkill < KI_FREEBURN_SKILL_COUNT_X360; leSkill++)
                    mafBankedActionScores[leSkill] = 0.0f;
            }
        }

        maBillboardsCollected.Clear();
    }
}
