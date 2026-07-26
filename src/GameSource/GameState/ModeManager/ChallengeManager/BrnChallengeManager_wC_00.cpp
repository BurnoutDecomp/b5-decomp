// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_00.cpp
// ============================================================================
// BrnGameState::ChallengeManager -- wave-C partfile (group G1): the three
// E_ACTION_FREEBURN_CHALLENGE (id 153, 0x20-byte FreeburnChallengeAction) posters that drive
// the freeburn-challenge lifecycle from selection to results teardown.
//
//   BeginChallenge            (X360 0x823505B8)
//   TriggerFreeburnChallenge  (X360 0x82346DA0)
//   UpdateResults             (X360 0x82345FD0)
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is authoritative for every store, branch, early-out and
// assert; the raw offsets are mapped onto the keystone-frozen NAMED members/accessors of
// BrnChallengeManager.h and onto the frozen GameStateModuleIO::FreeburnChallengeAction payload
// in BrnGameActions.h (sizeof 0x20, offsets static_assert-pinned there). No header was edited.
//
// FreeburnChallengeAction::meEventType DRIFT (keystone pitfall F4): the values 0..3 match the
// committed BrnNetwork::BrnNetworkModuleIO::EChallengeEventType enumerators 1:1 and are written
// by name (BeginChallenge stores SELECTED, TriggerFreeburnChallenge stores TRIGGERED); the
// UpdateResults store is the RAW X360 value 6, for which the X360 enum carries an extra
// (unattested) enumerator ahead of ENDED -- it is deliberately NOT spelled with the PS3 names
// E_CHALLENGE_EVENT_ENDED(4) / E_CHALLENGE_EVENT_RESULTS_FINISHED(5).
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameSource/GameState/BrnGameActions.h"                  // FreeburnChallengeAction / E_ACTION_FREEBURN_CHALLENGE (153)
#include "GameSource/Network/BrnNetworkModuleIO.h"                // BrnNetworkModuleIO::E_CHALLENGE_EVENT_* (values 0..3)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // TGameActionQueue::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "SharedClasses/DataLists/ChallengeList.h"                // GetChallengeIndex / GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h"           // GetChallengeID / GetNumActions / GetAction / GetActionType
#include "GameSource/GameState/BrnGameStateModule.h"              // GetActiveRaceCarIndex / GetPlayerActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT + Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"      // CgsDev::StrStream (runtime-value assert message)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// BeginChallenge  (X360 0x823505B8)
// ----------------------------------------------------------------------------
// Arms a freeburn challenge: aborts whatever challenge is already live, resolves the challenge
// list entry for lChallengeID, optionally broadcasts the "challenge selected" action, then
// resets the whole per-challenge working set and parks the manager in PENDING.
//
// Register map (asm prologue): r3 this, r4 lChallengeID (64-bit CgsID, `mr r28,r4` + the later
// `std`), r5 lpActionQueue, r6 lpActiveRaceCarOutputInterface (NEVER read by this body -- it is
// only threaded through by the caller), r7 lbIsOnline, r8 lbRemote (the post gate).
void ChallengeManager::BeginChallenge(
    CgsID lChallengeID,
    TGameActionQueue* lpActionQueue,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutputInterface,
    bool lbIsOnline,
    bool lbRemote)
{
    // X360 folds the two status compares into one bool in r11 (`cmpwi 2` / `cmpwi 3`), then
    // branches on it -- a plain short-circuit || in the source.
    if (meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RUNNING ||
        meChallengeManagerStatus == E_CHALLENGE_MANAGER_STATUS_RESULTS)
    {
        EndChallenge(E_CHALLENGE_STATUS_ABORTED, lpActionQueue, true);   // li r4,2 / li r6,1
    }

    // Resolve the entry: index first, and only fetch the entry when the id was found
    // (a miss leaves the cached challenge NULL -- `mr r29,r30` with r30 == 0).
    const BrnResource::ChallengeListEntry* lpChallenge = 0;
    const s32 liChallengeIndex = mpFreeburnChallengeList->GetChallengeIndex(lChallengeID);
    if (liChallengeIndex >= 0)
    {
        lpChallenge = mpFreeburnChallengeList->GetChallengeData(liChallengeIndex);
    }

    if (lbRemote)
    {
        // The posted id is the RESOLVED entry's id (`ld 0xC0(r29)`), not the parameter; the X360
        // does not null-check lpChallenge here.
        GameStateModuleIO::FreeburnChallengeAction lAction;
        lAction.mChallengeID                  = lpChallenge->GetChallengeID();
        lAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_SELECTED;  // 0
        lAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;                                  // 0
        lAction.miActionIndex                 = 0;
        lAction.miNumChallengesComplete       = -1;
        lAction.miTotalNumChallenges          = -1;
        lAction.mbIsHost                      = lbIsOnline;   // stb r26 (r7), +0x1C
        lAction.mbAbortingToStartNewChallenge = false;

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                                sizeof(GameStateModuleIO::FreeburnChallengeAction));
    }

    mpCurrentChallenge         = lpChallenge;                            // stw +0xE0C
    miCurrentChallengeAction   = 0;                                      // stw +0xE10
    miCurrentArbitrationIndex  = 0;                                      // stw +0xE14
    mbChallengeTimerRunning    = false;                                  // stb +0xE1C
    mbConvoyTimerRunning       = false;                                  // stb +0xE24
    meChallengeManagerStatus   = E_CHALLENGE_MANAGER_STATUS_PENDING;     // stw 1, +0xE08
    mbResultsTimerRunning      = false;                                  // stb +0xE2C
    mLastSecondSuccessStatus.UnSetAll();                                 // std 0, +0xE00

    ClearPlayerSuccessData();
    ResetCurrentChallengeData(miCurrentChallengeAction);                 // lwz r4, +0xE10

    // Leap-car tracking reset (flt_820037C8 == -1.0f, the "timer invalid" sentinel).
    mbUpdateLeaptCars     = false;   // stb +0x118
    miNumCarsLeapt        = 0;       // stw +0x114
    mfLeapCarsValidTimer  = -1.0f;   // stfs +0x110
}

// ----------------------------------------------------------------------------
// TriggerFreeburnChallenge  (X360 0x82346DA0)
// ----------------------------------------------------------------------------
// Takes the armed (PENDING) challenge live: flags every finalised remote player and the local
// player as challenge participants, broadcasts the "challenge triggered" action, and caches
// whether any of the challenge's actions needs the leap-car tracker.
//
// Register map: r3 this, r4 lChallengeID (64-bit CgsID), r5 lpActionQueue, r6 lbRemote.
void ChallengeManager::TriggerFreeburnChallenge(CgsID lChallengeID, TGameActionQueue* lpActionQueue,
                                                bool lbRemote)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_PENDING)   // lwz +0xE08; cmpwi 1
    {
        // Runtime-valued diagnostic (the X360 streams the status into the assert message buffer
        // through the global StrStream at off_82000D08); reproduced with the committed
        // local-StrStream idiom. X360 file/line: BrnChallengeManager.cpp:2098.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Status is " << static_cast<s32>(meChallengeManagerStatus);
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    meChallengeManagerStatus = E_CHALLENGE_MANAGER_STATUS_RUNNING;   // stw 2, +0xE08
    CGS_ASSERT(mpGameStateModule, "mpGameStateModule");              // :2102

    // Every finalised remote slot's player joins the challenge. The X360 walks the slots by their
    // +0x100 id field (this+0x6C8, stride 0x108) and reads the mbFinalised byte right after it.
    for (s32 liSlot = 0; liSlot < KI_MAX_REMOTE_PLAYERS; ++liSlot)
    {
        const ChallengeCompletionData& lSlot = maChallengeCompletionData[liSlot];
        if (lSlot.mNetworkPlayerID != -1 && lSlot.mbFinalised)     // -1 == free slot
        {
            // `::` qualified: BrnGameState also declares a same-valued EActiveRaceCarIndex
            // (BrnTakedownManagerTypes.h), and GetActiveRaceCarIndex returns the global one.
            const ::EActiveRaceCarIndex leActiveRaceCarIndex =
                mpGameStateModule->GetActiveRaceCarIndex(lSlot.mNetworkPlayerID);

            // The two-sided range test the compiler folded into a single unsigned `cmplwi 7`
            // (E_ACTIVE_RACE_CAR_INDEX_INVALID == -1 fails it as a large unsigned).
            if (leActiveRaceCarIndex >= ::E_ACTIVE_RACE_CAR_INDEX_0 &&
                leActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                mabPlayerStartedChallenge[leActiveRaceCarIndex] = true;
            }
        }
    }

    // Both bound asserts re-call the accessor (three calls in total in the asm).
    CGS_ASSERT(mpGameStateModule->GetPlayerActiveRaceCarIndex() >= ::E_ACTIVE_RACE_CAR_INDEX_0,
               "mpGameStateModule->GetPlayerActiveRaceCarIndex() >= E_ACTIVE_RACE_CAR_INDEX_0");   // :2120
    CGS_ASSERT(mpGameStateModule->GetPlayerActiveRaceCarIndex() < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mpGameStateModule->GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT");  // :2121

    mabPlayerStartedChallenge[mpGameStateModule->GetPlayerActiveRaceCarIndex()] = true;

    // Posted id is the PARAMETER here (`std r26` == the r4 CgsID), not the cached entry's.
    GameStateModuleIO::FreeburnChallengeAction lAction;
    lAction.mChallengeID                  = lChallengeID;
    lAction.meEventType                   = BrnNetwork::BrnNetworkModuleIO::E_CHALLENGE_EVENT_TRIGGERED;  // 1
    lAction.meChallengeStatus             = E_CHALLENGE_STATUS_ONGOING;                                   // 0
    lAction.miActionIndex                 = 0;
    lAction.miNumChallengesComplete       = -1;
    lAction.miTotalNumChallenges          = -1;
    lAction.mbIsHost                      = lbRemote;   // stb r24 (r6), +0x1C
    lAction.mbAbortingToStartNewChallenge = false;

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                            sizeof(GameStateModuleIO::FreeburnChallengeAction));

    CGS_ASSERT(mpCurrentChallenge, "mpCurrentChallenge");   // :2137

    // Leap-car tracking is only run when one of the challenge's actions is LEAP_CARS.
    // GetAction inlines its own index guards (ChallengeListEntry.h:941/942) and GetActionType
    // inlines its type-range guard (:660) -- neither is duplicated here.
    mbChallengeRequiresLeapCars = false;   // stb 0, +0xE34
    for (s32 liActionIndex = 0; liActionIndex < mpCurrentChallenge->GetNumActions(); ++liActionIndex)
    {
        const BrnResource::ChallengeListEntryAction* lpAction =
            mpCurrentChallenge->GetAction(liActionIndex);
        CGS_ASSERT(lpAction, "lpAction");   // :2144

        if (lpAction->GetActionType() ==
            BrnResource::ChallengeListEntryAction::E_CHALLENGE_ACTION_LEAP_CARS)   // == 3
        {
            mbChallengeRequiresLeapCars = true;
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateResults  (X360 0x82345FD0)
// ----------------------------------------------------------------------------
// Runs the post-challenge results hold. While the results timer is still counting the manager
// just idles; when it expires the final "results finished" action goes out and the whole
// challenge working set is torn down back to NONE.
//
// Register map: r3 this, f1 lfTimeStep (its r4 slot is shadowed and unused), r5 lpActionQueue.
void ChallengeManager::UpdateResults(f32 lfTimeStep, TGameActionQueue* lpActionQueue)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RESULTS)   // lwz +0xE08; cmpwi 3
    {
        return;
    }

    if (UpdateResultsTimer(lfTimeStep))   // still running -> nothing to do this frame
    {
        return;
    }

    GameStateModuleIO::FreeburnChallengeAction lAction;
    lAction.mChallengeID                  = mpCurrentChallenge->GetChallengeID();   // ld 0xC0
    // RAW X360-drift event value (see the file header note): NOT the PS3 enumerator names.
    lAction.meEventType                   = 6;
    lAction.meChallengeStatus             = meLocalChallengeStatus;                 // lwz +0xE30
    lAction.miActionIndex                 = 0;
    lAction.miNumChallengesComplete       = -1;
    lAction.miTotalNumChallenges          = -1;
    lAction.mbIsHost                      = false;
    lAction.mbAbortingToStartNewChallenge = false;

    CGS_ASSERT(lpActionQueue, "lpActionQueue");   // :884
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                            GameStateModuleIO::E_ACTION_FREEBURN_CHALLENGE,
                            sizeof(GameStateModuleIO::FreeburnChallengeAction));

    meChallengeManagerStatus  = E_CHALLENGE_MANAGER_STATUS_NONE;   // stw 0, +0xE08
    mpCurrentChallenge        = 0;                                 // stw 0, +0xE0C
    miCurrentChallengeAction  = 0;                                 // stw 0, +0xE10
    miCurrentArbitrationIndex = 0;                                 // stw 0, +0xE14
    // The COUNT sentinel doubles as "no local status" (BurnoutConstants.h note).
    meLocalChallengeStatus    = E_CHALLENGE_STATUS_COUNT;          // stw 7, +0xE30
    mLastSecondSuccessStatus.UnSetAll();                           // std 0, +0xE00

    ClearPlayerSuccessData();
}

}
