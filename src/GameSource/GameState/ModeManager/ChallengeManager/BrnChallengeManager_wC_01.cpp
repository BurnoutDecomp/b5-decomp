// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_01.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G2): the challenge COMPLETION +
// ARBITRATION spine. The three bodies call each other (both arbitration paths terminate in
// EndChallenge), so they live in one partfile.
//
//   EndChallenge             (X360 0x8234DE30, keystone row 4)
//   UpdateArbitration        (X360 0x82351AF8, keystone row 5)
//   UpdateArbitrationSuccess (X360 0x82350340, keystone row 6)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out and
// assert; the raw offsets are mapped onto the keystone-frozen NAMED members/accessors of
// BrnChallengeManager.h and onto the frozen GameStateModuleIO action payloads in
// BrnGameActions.h (FreeburnChallengeAction sizeof 0x20 / FburnChallengeShowSelectorAction
// sizeof 8, offsets static_assert-pinned there). NO header was edited.
//
// Conventions carried from the keystone spec:
//  * FreeburnChallengeAction::meEventType -- values 0..3 match the committed
//    BrnNetwork::BrnNetworkModuleIO::EChallengeEventType enumerators 1:1 and are written by
//    name (UpdateArbitration stores RESET, UpdateArbitrationSuccess stores ACTION_SUCCESS);
//    EndChallenge's store is the RAW X360-drift value 5 -- the X360 enum carries one extra
//    (unattested) enumerator ahead of ENDED, so it is deliberately NOT spelled
//    E_CHALLENGE_EVENT_ENDED(4).
//  * The "Index N is out of range (max bits: 2000)" (CgsFastBitArray.h:431) and
//    "Index: N, Number of bits: 8" (CgsBitArray.h:222) StrStream blobs in the asm are the
//    INLINED container range asserts -- the committed container methods are called instead of
//    reproducing them.
//  * The per-index `leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT` /
//    `leEnumIndex <= E_FREEBURN_SKILL_COUNT` asserts are the inlined enum operator++ guards
//    (BurnoutConstants.h:39 / BrnChallengeManager.h:113) -- the committed operators are used.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameSource/GameState/BrnGameActions.h"                  // FreeburnChallengeAction (153) / FburnChallengeShowSelectorAction (160)
#include "GameSource/Network/BrnNetworkModuleIO.h"                // BrnNetworkModuleIO::E_CHALLENGE_EVENT_* (values 0..3)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // TGameActionQueue::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<8> (case-6 rank scratch)
#include "SharedClasses/DataLists/ChallengeList.h"                // ChallengeList::GetChallengeCount
#include "SharedClasses/DataLists/ChallengeListEntry.h"           // GetChallengeID / GetNumActions / GetNumPlayers / GetAction / GetCoopType / GetCombineAction
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // GetProfile / GetAchievementManager
#include "GameSource/GameState/Progression/BrnProfile.h"          // HasPlayerCompletedFreeburnChallenge / CompleteFreeburnChallenge
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnFreeburnChallengeComplete
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT + Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"      // CgsDev::StrStream (runtime-value assert message)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// EndChallenge  (X360 0x8234DE30)
// ----------------------------------------------------------------------------
// Tears the live freeburn challenge down with the supplied outcome. On SUCCESS it also banks
// the completion in the player profile (first time only -- which additionally feeds the
// achievement counter and the online-unlock re-scan), records the challenge in the local
// completion bit store, mirrors that into the remote players' slots, broadcasts the
// every-player status event, posts the unnamed id-55 notification and marks every
// participating player DONE on both action slots. In ALL cases it posts the id-153 lifecycle
// action and parks the manager in RESULTS with the leap-car tracking state cleared.
//
// Register map (asm prologue): r3 this, r4 leChallengeStatus, r5 lpActionQueue (the assert
// string still spells the parameter "lpOutput"), r6 lbIsOnline.
void ChallengeManager::EndChallenge(EChallengeStatus leChallengeStatus,
                                    TGameActionQueue* lpActionQueue,
                                    bool lbIsOnline)
{
    // X360 inlines IsChallengeActive() as `meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_NONE`
    // (lwz +0xE08; cmpwi 0) -- the assert text is the source expression.
    CGS_ASSERT(IsChallengeActive(), "IsChallengeActive()");   // :2239
    CGS_ASSERT(lpActionQueue, "lpOutput");                    // :2240

    const CgsID lActiveChallengeID = mpCurrentChallenge->GetChallengeID();   // lwz +0xE0C; ld +0xC0
    CGS_ASSERT(lActiveChallengeID != 0, "lActiveChallengeID != 0");          // :2246

    meLocalChallengeStatus = leChallengeStatus;   // stw +0xE30

    if (leChallengeStatus == E_CHALLENGE_STATUS_SUCCESS)   // X360 folds `status - 1 == 0` into a bool
    {
        CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :2258
        CGS_ASSERT(mpProgression, "mpProgression");             // :2259

        // First completion only: bank it in the profile, tell the achievement manager the new
        // completed-challenge count and re-scan the online unlocks.
        if (!mpProgression->GetProfile()->HasPlayerCompletedFreeburnChallenge(lActiveChallengeID))
        {
            const u32 luCompletedChallengeCount =
                mpProgression->GetProfile()->CompleteFreeburnChallenge(lActiveChallengeID);
            mpProgression->GetAchievementManager()->OnFreeburnChallengeComplete(luCompletedChallengeCount);
            CheckForOnlineChallengeUnlocks();
        }

        const s32 liChallengeIndex = GetChallengeIndex(lActiveChallengeID);
        if (liChallengeIndex >= 0)
        {
            // Inlined FastBitArray<2000>::SetBit (its "Index N is out of range (max bits: 2000)"
            // StrStream blob at CgsFastBitArray.h:431 is that container's own range assert).
            mLocalChallengeCompletionData.SetBit(static_cast<u32>(liChallengeIndex));
            SetRemotePlayersChallengeCompleted(liChallengeIndex);
            OutputFreeburnChallengeEveryPlayerStatusEvent(lpActionQueue);
        }
        else
        {
            CGS_ASSERT(liChallengeIndex >= 0, "liChallengeIndex >= 0");   // :2268
        }

        // X360 posts a single ZEROED byte under the RAW action id 55. No committed
        // EGameActionType enumerator carries that value, so the raw s32 id is kept here
        // (AddEvent's type parameter is a plain s32).
        const u8 luChallengeCompleteActionPayload = 0;   // stb 0, sp+var_B0
        lpActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&luChallengeCompleteActionPayload), 55, 1);

        // Every player that took part finishes DONE on both action slots (stw 3, +0x00/+0x04
        // over the 8-byte per-player stride of maaePlayersSuccessStatus).
        for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
             static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
             lePlayer++)
        {
            if (mabPlayerStartedChallenge[lePlayer])
            {
                maaePlayersSuccessStatus[lePlayer][0] = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
                maaePlayersSuccessStatus[lePlayer][1] = GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;
            }
        }
    }

    GameStateModuleIO::FreeburnChallengeAction lChallengeAction;
    lChallengeAction.mChallengeID = lActiveChallengeID;                       // std sp+var_90
    // RAW X360-drift event value (see the file header note): NOT E_CHALLENGE_EVENT_ENDED(4).
    lChallengeAction.meEventType                   = 5;                      // stw sp+var_88
    lChallengeAction.meChallengeStatus             = leChallengeStatus;      // stw sp+var_84
    lChallengeAction.miActionIndex                 = 0;                      // stw sp+var_80
    lChallengeAction.miNumChallengesComplete       = CountCompletedChallenges();               // stw sp+var_7C
    lChallengeAction.miTotalNumChallenges          = mpFreeburnChallengeList->GetChallengeCount(); // lwz +0x32E0; stw sp+var_78
    lChallengeAction.mbIsHost                      = false;                  // stb 0, sp+var_74
    lChallengeAction.mbAbortingToStartNewChallenge = lbIsOnline;             // stb r20, sp+var_73 (attested store)
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChallengeAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                            sizeof(GameStateModuleIO::FreeburnChallengeAction));   // li r5,0x99; li r6,0x20

    mbChallengeTimerRunning     = false;                                   // stb 0, +0xE1C
    meChallengeManagerStatus    = E_CHALLENGE_MANAGER_STATUS_RESULTS;      // stw 3, +0xE08
    mbChallengeRequiresLeapCars = false;                                   // stb 0, +0xE34
    mbUpdateLeaptCars           = false;                                   // stb 0, +0x118
    miNumCarsLeapt              = 0;                                       // stw 0, +0x114
    mfLeapCarsValidTimer        = -1.0f;                                   // stfs flt_820037C8, +0x110

    // Drop every cached location-enter skill value (stfs 0.0 over the 38-entry +0xF34 run).
    for (EFreeburnSkill leSkill = E_FREEBURN_SKILL_START;
         static_cast<s32>(leSkill) < KI_FREEBURN_SKILL_COUNT_X360;
         leSkill++)
    {
        mafCachedActiveSkillValueOnLocationEnter[leSkill] = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// UpdateArbitration  (X360 0x82351AF8)
// ----------------------------------------------------------------------------
// The per-frame arbitration pass UpdateRunning runs after the challenge update. It first
// handles the two global exits (the challenge action ran out of time; the convoy/meet-up
// window closed), then walks the challenge's actions from miCurrentArbitrationIndex and, per
// action co-op type, decides whether the action's success condition is now met -- handing the
// accepted action to UpdateArbitrationSuccess, which may end the whole challenge.
//
// Register map (asm prologue): r3 this, f1 lfTimeStep, r5 lpActionQueue, r6
// lpActiveRaceCarOutputInterface (spilled; only threaded into UpdateAction), r7 the declared
// lbIsOnline slot. X360 SLOT NOTE (same class of drift as UpdateRunning/PostWorldUpdate): the
// committed caller UpdateRunning passes its "the challenge action timer expired" flag in this
// slot, and this body uses it as that gate; the declared parameter name is kept.
void ChallengeManager::UpdateArbitration(
    f32                                                                          lfTimeStep,
    TGameActionQueue*                                                            lpActionQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool                                                                         lbIsOnline)
{
    if (lbIsOnline)
    {
        // Time is up: bounce the player back to the challenge selector and fail the challenge.
        GameStateModuleIO::FburnChallengeShowSelectorAction lShowSelectorAction;
        lShowSelectorAction.mChallengeID = mpCurrentChallenge->GetChallengeID();   // lwz +0xE0C; ld +0xC0
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowSelectorAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR,
                                sizeof(GameStateModuleIO::FburnChallengeShowSelectorAction));  // li r5,0xA0; li r6,8
        EndChallenge(E_CHALLENGE_STATUS_FAILURE, lpActionQueue, false);            // li r4,6; li r6,0
        return;
    }

    // Inlined convoy countdown (the UpdateConvoyTimer shape: no arming -- the convoy timer is
    // armed by UpdateActionSuccess -- just the fsel clamp-at-zero decrement and the expiry test
    // against the SAME once-loaded running flag).
    if (mbConvoyTimerRunning && mfConvoyTimer > 0.0f)   // lbz +0xE24; lfs +0xE20
    {
        // f0 = timer - dt; fsel(-(timer-dt), 0.0, f0) clamps the countdown at 0.0.
        const f32 lfNewConvoyTimer = mfConvoyTimer - lfTimeStep;
        mfConvoyTimer = (lfNewConvoyTimer >= 0.0f) ? lfNewConvoyTimer : 0.0f;   // stfs +0xE20
    }

    if (mbConvoyTimerRunning && mfConvoyTimer <= 0.0f)
    {
        // The convoy/meet-up window closed without the group forming: rewind the challenge to
        // the arbitration action and tell everyone it was reset.
        GameStateModuleIO::FreeburnChallengeAction lResetAction;
        lResetAction.mChallengeID                  = mpCurrentChallenge->GetChallengeID();  // std sp+var_D0
        lResetAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_RESET;  // stw 3, sp+var_C8
        lResetAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;            // stw 0, sp+var_C8+4
        lResetAction.miActionIndex                 = miCurrentArbitrationIndex;             // lwz +0xE14; stw sp+var_C0
        lResetAction.miNumChallengesComplete       = -1;                                    // stw -1, sp+var_BC
        lResetAction.miTotalNumChallenges          = -1;                                    // stw -1, sp+var_B8
        lResetAction.mbIsHost                      = true;                                  // stb 1, sp+var_B4
        lResetAction.mbAbortingToStartNewChallenge = false;                                 // stb 0, sp+var_B3

        miCurrentChallengeAction = miCurrentArbitrationIndex;   // stw +0xE10 (before the post)

        CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :1349
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                                sizeof(GameStateModuleIO::FreeburnChallengeAction));   // li r5,0x99; li r6,0x20

        mbConvoyTimerRunning = false;                          // stb 0, +0xE24
        ResetCurrentChallengeData(miCurrentChallengeAction);   // lwz +0xE10
        return;
    }

    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1363

    for (s32 liActionIndex = miCurrentArbitrationIndex;                 // lwz +0xE14
         liActionIndex < mpCurrentChallenge->GetNumActions();           // lbz +0xD4
         ++liActionIndex)
    {
        const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);
        const BrnResource::ChallengeListEntryAction::EChallengeCoopType leCoopType = lpAction->GetCoopType();  // lbz +0x01

        switch (leCoopType)   // X360 jump table, cases 0..6 (case 5 falls to the default)
        {
            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_ONCE:   // 0
            {
                // Anyone succeeding is enough.
                if (GetNumPlayerSucceeding(liActionIndex) > 0)
                {
                    if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                    {
                        return;
                    }
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL:               // 1
            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_INDIVIDUAL_ACCUMULATION:  // 2
            {
                // Everyone in the challenge has to be succeeding, and the per-action success
                // update must not already have gone out.
                const s32 liNumPlayerSucceeding = GetNumPlayerSucceeding(liActionIndex);
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1508
                if (liNumPlayerSucceeding == mpCurrentChallenge->GetNumPlayers() &&   // lbz +0xD3 & 0xF
                    !mabIndividualActionsSuccessUpdateSent[liActionIndex])            // lbz +0xFDA
                {
                    if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                    {
                        return;
                    }
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_SIMULTANEOUS:   // 3
            {
                // Everyone has reported in this frame: intersect the per-player success windows
                // and accept the action if there is any frame where every participant succeeded
                // at the same time.
                if (!ReceivedSuccessUpdatesFromAllPlayers())
                {
                    break;
                }

                // 0x82351D9C `mr r3, r24` re-seeds the accepted flag with r24 == 0 once the
                // gate has passed, so an empty intersection keeps scanning the later actions
                // this frame rather than returning.
                bool lbArbitrationAccepted = false;

                // X360 keeps the intersection in two stack qwords: cleared, then bits 0..119 set
                // (the FastBitArray<120> SetAll), then ANDed with each participant's window.
                CgsContainers::FastBitArray<120> lCommonSuccessWindow;
                lCommonSuccessWindow.UnSetAll();
                lCommonSuccessWindow.SetAll();
                for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
                {
                    if (mabPlayerStartedChallenge[liPlayer])   // lbz +0x598
                    {
                        // Per-field AND of maPlayerSuccessUpdateArray[liPlayer] into the
                        // intersection, expressed through the container's public bit API.
                        for (u32 luBit = 0; luBit < lCommonSuccessWindow.GetCapacity(); ++luBit)
                        {
                            if (!maPlayerSuccessUpdateArray[liPlayer].IsBitSet(luBit))
                            {
                                lCommonSuccessWindow.UnSetBit(luBit);
                            }
                        }
                    }
                }

                // X360 tests the two fields for "all zero"; a surviving bit accepts the action.
                if (lCommonSuccessWindow.GetFirstBitSet() !=
                    CgsContainers::FastBitArray<120>::KI_INVALID_BIT_INDEX)
                {
                    lbArbitrationAccepted = UpdateArbitrationSuccess(liActionIndex, lpActionQueue);
                }

                // Start the next reporting round regardless (stb 0 over the 8-byte run @+0x590).
                for (s32 liPlayer = 0; liPlayer < KI_MAX_CHALLENGE_PLAYERS; ++liPlayer)
                {
                    mabReceivedSuccessUpdates[liPlayer] = false;
                }

                if (lbArbitrationAccepted)
                {
                    return;
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE:   // 4
            {
                CGS_ASSERT(liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE,
                           "liActionIndex < BrnResource::ChallengeListEntry::KI_MAX_ACTIONS_PER_CHALLENGE");   // :1449

                // Fold this frame's per-player scores into the cumulative total and clear them.
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    mafCumulativeActionScores[liActionIndex] += maafCurrentActionsScores[lePlayer][liActionIndex];
                    maafCurrentActionsScores[lePlayer][liActionIndex] = 0.0f;
                }

                const s32 liNumPlayersContributing = GetNumPlayersContributing(liActionIndex);
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1464
                if (liNumPlayersContributing != mpCurrentChallenge->GetNumPlayers())   // lbz +0xD3 & 0xF
                {
                    break;
                }

                // Re-run the action against the pooled total; only a SUCCESS accepts it.
                if (UpdateAction(liActionIndex, mpCurrentChallenge->GetAction(liActionIndex), lfTimeStep,
                                 lpActiveRaceCarOutputInterface, true) != E_CHALLENGE_STATUS_SUCCESS)
                {
                    break;
                }

                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        maaePlayersSuccessStatus[lePlayer][liActionIndex] =
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE;   // stw 3
                    }
                }

                if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                {
                    return;
                }
                break;
            }

            case BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_COUNT:   // 6 (a real X360 co-op type)
            {
                // Ranked co-op: every participant's cumulative contribution is a 1-based rank.
                // The action is accepted once the used ranks form a gap-free run covering all
                // participants (the find-first-CLEAR-bit scan over the rank scratch).
                const s32 liNumPlayersContributing = GetNumPlayersContributing(liActionIndex);
                CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1529
                if (liNumPlayersContributing != mpCurrentChallenge->GetNumPlayers())   // lbz +0xD3 & 0xF
                {
                    break;
                }

                CgsContainers::BitArray<8> lRanksUsed;
                lRanksUsed.UnSetAll();   // std 0, sp+var_100
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer])
                    {
                        // fctiwz/stfiwx == the float->s32 truncation of the banked contribution.
                        const s32 liScore =
                            static_cast<s32>(maafCumulativeContributions[lePlayer][liActionIndex]) - 1;
                        CGS_ASSERT(liScore >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                                   "liScore >= E_ACTIVE_RACE_CAR_INDEX_0");     // :1545
                        CGS_ASSERT(liScore < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                                   "liScore < E_ACTIVE_RACE_CAR_INDEX_COUNT");  // :1546
                        // Inlined BitArray<8>::SetBit ("Index: N, Number of bits: 8" blob
                        // @CgsBitArray.h:222 is that container's own range assert).
                        lRanksUsed.SetBit(static_cast<u32>(liScore));
                    }
                }

                // X360 inlines the find-first-clear-bit scan TWICE (once per side of the &&).
                if (lRanksUsed.GetFirstClearBit() < mpCurrentChallenge->GetNumPlayers() &&
                    lRanksUsed.GetFirstClearBit() != CgsContainers::BitArray<8>::KI_INVALID_BITINDEX)
                {
                    break;
                }

                if (mabIndividualActionsSuccessUpdateSent[liActionIndex])   // lbz +0xFDA
                {
                    break;
                }

                if (UpdateArbitrationSuccess(liActionIndex, lpActionQueue))
                {
                    return;
                }
                break;
            }

            default:
            {
                // X360 streams the co-op type into the assert message buffer (:1568).
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Can't deal with challenges coop type: "
                           << static_cast<s32>(leCoopType)
                           << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                break;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateArbitrationSuccess  (X360 0x82350340)
// ----------------------------------------------------------------------------
// Accepts one challenge action: latches its "success update sent" flag, then decides whether
// the whole challenge is now finished. For a CHAIN-terminating action (the X360 co-op/combine
// value 5) that means every participant is DONE on every action; for anything else it means
// this was the last action. If the challenge is finished it shows the selector again and ends
// the challenge with SUCCESS (returning true). Otherwise -- unless the action is INDEPENDENT --
// it broadcasts the id-153 "action success" and advances the arbitration/current action index.
//
// Register map (asm prologue): r3 this, r4 liActionIndex, r5 lpActionQueue.
bool ChallengeManager::UpdateArbitrationSuccess(s32 liActionIndex, TGameActionQueue* lpActionQueue)
{
    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :1595

    const BrnResource::ChallengeListEntryAction* lpAction = mpCurrentChallenge->GetAction(liActionIndex);
    CGS_ASSERT(lpAction, "lpAction");   // :1597

    mabIndividualActionsSuccessUpdateSent[liActionIndex] = true;   // stb 1, +0xFDA

    bool lbChallengeComplete;
    if (lpAction->GetCombineAction() == BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)  // lbz +0x03 == 5
    {
        lbChallengeComplete = true;
        if (mpCurrentChallenge->GetNumActions() != 0)   // lbz +0xD4
        {
            s32 liAction = 0;
            do
            {
                for (::EActiveRaceCarIndex lePlayer = ::E_ACTIVE_RACE_CAR_INDEX_0;
                     static_cast<s32>(lePlayer) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
                     lePlayer++)
                {
                    if (mabPlayerStartedChallenge[lePlayer] &&                     // lbz +0x598
                        maaePlayersSuccessStatus[lePlayer][liAction] !=            // lwz +0x428
                            GameStateModuleIO::E_FREEBURN_CHALLENGE_SUCCESS_DONE)  // cmpwi 3
                    {
                        lbChallengeComplete = false;
                        break;
                    }
                }
                if (!lbChallengeComplete)
                {
                    break;
                }
                ++liAction;
            }
            while (liAction < mpCurrentChallenge->GetNumActions());
        }
    }
    else
    {
        lbChallengeComplete = (mpCurrentChallenge->GetNumActions() - 1 == liActionIndex);
    }

    if (lbChallengeComplete)
    {
        GameStateModuleIO::FburnChallengeShowSelectorAction lShowSelectorAction;
        lShowSelectorAction.mChallengeID = mpCurrentChallenge->GetChallengeID();   // ld +0xC0
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowSelectorAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE_SHOW_SELECTOR,
                                sizeof(GameStateModuleIO::FburnChallengeShowSelectorAction));  // li r5,0xA0; li r6,8
        EndChallenge(E_CHALLENGE_STATUS_SUCCESS, lpActionQueue, false);            // li r4,1; li r6,0
        return true;
    }

    if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_INDEPENDENT)  // lbz +0x03 == 4
    {
        GameStateModuleIO::FreeburnChallengeAction lSuccessAction;
        lSuccessAction.mChallengeID                  = mpCurrentChallenge->GetChallengeID();  // std sp+var_A0
        lSuccessAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_ACTION_SUCCESS;  // stw 2, sp+var_98
        lSuccessAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;   // stw 0, sp+var_94
        lSuccessAction.miActionIndex                 = liActionIndex;                // stw sp+var_90
        lSuccessAction.miNumChallengesComplete       = -1;                           // stw -1, sp+var_8C
        lSuccessAction.miTotalNumChallenges          = -1;                           // stw -1, sp+var_88
        lSuccessAction.mbIsHost                      = true;                         // stb 1, sp+var_84
        lSuccessAction.mbAbortingToStartNewChallenge = false;                        // stb 0, sp+var_83

        CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :1670
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSuccessAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                                sizeof(GameStateModuleIO::FreeburnChallengeAction));   // li r5,0x99; li r6,0x20

        if (lpAction->GetCombineAction() != BrnResource::ChallengeListEntryAction::E_COMBINE_ACTION_COUNT)  // lbz +0x03 == 5
        {
            // Advance the arbitration index, clamped to the last action; drag the current
            // challenge action along with it when it has fallen behind (X360 stores the raw
            // index to +0xE14 first, then re-stores the clamped one).
            s32 liNextActionIndex = liActionIndex + 1;
            miCurrentArbitrationIndex = liNextActionIndex;                            // stw +0xE14 (raw)
            const s32 liLastActionIndex = mpCurrentChallenge->GetNumActions() - 1;    // lbz +0xD4; -1
            if (liNextActionIndex >= liLastActionIndex)
            {
                liNextActionIndex = liLastActionIndex;
            }
            miCurrentArbitrationIndex = liNextActionIndex;                            // stw +0xE14 (clamped)

            if (liNextActionIndex > miCurrentChallengeAction)                         // lwz +0xE10; cmpw
            {
                if (liNextActionIndex >= liLastActionIndex)
                {
                    liNextActionIndex = liLastActionIndex;
                }
                miCurrentChallengeAction = liNextActionIndex;                         // stw +0xE10
            }
        }
    }

    return false;
}

}   // namespace BrnGameState
