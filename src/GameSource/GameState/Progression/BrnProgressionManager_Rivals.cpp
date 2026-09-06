// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_Rivals.cpp
// ============================================================================================
// [takedown P1 wave 2026-09-03] The rival-shutdown leg of BrnProgression::ProgressionManager --
// what TakedownManager::ProcessTakedownEvent's free-burn arm calls when the player takes a rival
// down (the OnPursuitWon park in BrnTakedownManager_Detect.cpp), plus the two
// AchievementManagerBase accessors that were still declare-only in BrnProgressionManager.h.
//
//   OnPursuitWon              @0x82389F40  (asserts BrnProgressionManager.cpp:2400 / :2410 / :2413)
//   DefeatRivalAndUnlockCar   @0x8237B1D0  (assert  BrnProgressionManager.cpp:2465)
//   CheckForAllRivalsUnlocked @0x8236FD90
//   GetProfileTotalTakedowns  no X360 symbol -- `lwz r11, 0x198(profile)` inlined in
//                             AchievementManagerBase::OnTakedown @0x8235AB70
//   GetCarChallengeWinCount   no X360 symbol -- `lwz r11, 0x358(pm)` inlined in
//                             AchievementManagerBase::OnEventWin @0x82372B68 / @0x82372BB4
//
// A partfile, not BrnProgressionManager.cpp, so this lane stays file-disjoint from the other
// ProgressionManager partfiles (the _Completion / _EventFinish / _Unlocks precedent).
// Every member is reached BY NAME; the console offsets are quoted to show which member each
// load lands on. The embedded Profile is this+0x170 on the console (`addi r27, r25, 0x170`).
// ============================================================================================
#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::FindRival / AddRival / FindCar / tallies
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"      // BrnProgression::CarData (AddCar's record)
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"    // BrnProgression::RivalData (+ EState)
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::FindRivalIndexFromId / GetRival
#include "SharedClasses/Progression/BrnRival.h"                          // BrnProgression::Rival
#include "GameSource/GameState/BrnGameActions.h"                         // RivalStateChangeAction / E_ACTION_*
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GameStateModuleIO::E_MODE_BURNING_ROUTE / GameActionQueue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include <cstring>                                                        // std::memset (the record's 3 pad bytes)
// ---- [progression wave 2026-09-06, lane rivals] ------------------------------------------
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"   // TrainingManager::RequestTraining (OnTakedownTo's inlined gauntlet)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnOnlineRaceComplete
#include "SharedClasses/DataLists/VehicleListEntry.h"                  // BrnResource::E_CARTYPE_AGGRESSION (Profile::meCurrentCarType == 1)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                // BrnProgression::ETrainingType
#include <stdlib.h>                                                    // getenv (the [rivals] witness gate)
// ---- [progression wave 2026-09-06, lane rivals2] ------------------------------------------
#include "SharedClasses/AI/AISectionsResourceType.h"                   // BrnAI::AISectionsData::GetAISection / AISection::GetMiddle
#include "SharedClasses/Progression/BrnRaceEventData.h"                // BrnProgression::EventRacerPersonality (the record's +0x58 field)
#include "BrnCommonTypes.h"                                            // Vector3 (the record's two 16-byte lanes)
#include <cstddef>                                                     // offsetof (the record's pinned offsets)

namespace BrnProgression
{

// --------------------------------------------------------------------------------------------
// GetProfileTotalTakedowns -- AchievementManagerBase::OnTakedown @0x8235AB20..0x8235AB74:
//   lwz  r11, 4(r31)        ; mpProgressionManager
//   addi r29, r11, 0x170    ; the embedded Profile (asserted "lpProfile", :188)
//   lwz  r11, 0x198(r29)    ; Profile+408 == miTotalTakedownCount ; cmpwi 0x1F4 (500)
// Profile+408 is the word Profile::AddTakedown @0x82354C00 increments (BrnProfile.h "+408").
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::GetProfileTotalTakedowns() const
{
    return mProfile.GetTotalTakedownCount();
}

// --------------------------------------------------------------------------------------------
// GetCarChallengeWinCount -- AchievementManagerBase::OnEventWin @0x82372B64..0x82372B6C (and the
// same pair again @0x82372BB0..0x82372BB8):
//   lwz r11, 4(r31)         ; mpProgressionManager
//   lwz r11, 0x358(r11)     ; cmpwi 0x19 (25) / 0x23 (35)
// 0x358 == 856 == 0x170 (the embedded Profile) + 488, and Profile+488 is
// maiWinsPerOfflineGameMode[5] (array base +468, DWARF BrnProfile.h:1199; BrnGui's
// SetBurningRouteDescription reads the very same word as `lwz r11, 0x1E8(profile)`, see
// Profile::GetNumWinsForGameMode). 5 == GameStateModuleIO::E_MODE_BURNING_ROUTE -- the "car
// challenge" the two XS-car achievements count is the Burning Route win tally.
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::GetCarChallengeWinCount() const
{
    return mProfile.GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE);
}

// --------------------------------------------------------------------------------------------
// CheckForAllRivalsUnlocked @0x8236FD90 (caller: DefeatRivalAndUnlockCar, both arms).
//   0x8236FDA8  bl GetNumberOfBeatenRivals   -> r30
//   0x8236FDB4  bl GetTrueNumberOfRivals     -> r3
//   0x8236FDB8  cmpw r30, r3 ; blt -> done
//   0x8236FDD8  stbx 0, this+0x20981          ; mbShowShutDownAllIfNeeded          = false
//   0x8236FDDC  stbx 1, this+0x20980          ; mbNeedToShowAllRivalsBeatenMessage = true
// (Member identity: the two are consecutive bools, DWARF BrnProgressionManager.h:885/:886 in
// that order; ProgressionManager::PreWorldUpdate @0x823A4F68 is their reader.)
// --------------------------------------------------------------------------------------------
void ProgressionManager::CheckForAllRivalsUnlocked()
{
    const s32 liBeatenRivals = GetNumberOfBeatenRivals();
    const s32 liTrueRivals   = GetTrueNumberOfRivals();
    if (liBeatenRivals >= liTrueRivals)
    {
        mbShowShutDownAllIfNeeded          = false;
        mbNeedToShowAllRivalsBeatenMessage = true;
    }
}

// --------------------------------------------------------------------------------------------
// DefeatRivalAndUnlockCar @0x8237B1D0 (r3 this, r4 liRivalIndex, r5 leUnlockSequenceType).
//   0x8237B1F4..0x8237B230  GetRival(liRivalIndex): the :468 "liIndex < miRivalCount" assert is the
//                           accessor's own (BrnProgressionData.h), then `add r29 = rivals + 0x38*idx`
//   0x8237B238              ld r26, 8(r29)               ; Rival::mCarId
//   0x8237B23C..0x8237B248  the SAME bound re-tested silently: out of range == nothing below runs
//   0x8237B24C..0x8237B284  r27 = this+0x170 (Profile); the inlined Profile::FindRival(Rival::mId)
//                           walk over maRivals (+0x6280, 0x38 stride, count +0x274) -> r28
//   0x8237B288  cmpwi r24, 1  (E_UNLOCK_SEQUENCE_TYPE_NONE): only then, and only on a miss,
//   0x8237B2A0    bl Profile::AddRival(mId, mCarId)
//   0x8237B2B0  assert "lpSavedRival" (:2465)
//   0x8237B2D0..0x8237B310  if (gxMessageFilterFlags & 1) log "Moving rival to beaten state: " + id
//   0x8237B314/0x8237B320   li 3 ; stw 0x10(r28)         ; RivalData::meState = E_STATE_BEATEN
//   0x8237B324  bl Profile::FindCar(mCarId) ; bne -> done
//   0x8237B330  li r5, 3 ; bl AddCar(mCarId, 3)          ; CarData::E_UNLOCK_TYPE_SHUTDOWN_RIVAL
//   0x8237B33C  cmpwi r24, 0 (DEFAULT):  stfs flt_82029BB8 (0.85f, image bytes 3F59999A) -> CarData+0xC
//               else if r24 == 1 (NONE): stb 1 -> CarData+0xA
//   0x8237B354 / 0x8237B384  bl CheckForAllRivalsUnlocked   (both arms)
// --------------------------------------------------------------------------------------------
void ProgressionManager::DefeatRivalAndUnlockCar(s32 liRivalIndex, EUnlockSequenceType leUnlockSequenceType)
{
    const ProgressionData* lpProgressionData = GetProgressionData();

    // GetRival carries the console's :468 assert; callers do not duplicate it.
    const Rival* lpRival = lpProgressionData->GetRival(liRivalIndex);

    // 0x8237B23C..0x8237B248: the silent re-test -- the console reads the (out-of-range) record's
    // car id first and then skips the whole body; the read is moved under the test so a bad index
    // never dereferences. Same observable behaviour.
    if (liRivalIndex >= lpProgressionData->GetRivalCount())
    {
        return;
    }
    const CgsID lRivalId = lpRival->GetId();
    const CgsID lCarId   = lpRival->GetCarId();

    RivalData* lpSavedRival = mProfile.FindRival(lRivalId);
    if (leUnlockSequenceType == E_UNLOCK_SEQUENCE_TYPE_NONE)
    {
        if (lpSavedRival == 0)
        {
            lpSavedRival = mProfile.AddRival(lRivalId, lCarId);
        }
    }
    CGS_ASSERT(lpSavedRival != 0, "lpSavedRival");   // BrnProgressionManager.cpp:2465

    if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
    {
        // The console streams the id through sub_82203EE8 (the StrStream CgsID formatter, not in
        // the tree); the raw 64-bit value is printed here.
        *CgsDev::Log::gpDebugPrint << "Moving rival to beaten state: " << static_cast<u64>(lRivalId) << "\n";
    }

    // [GUARD] the console stores through lpSavedRival unconditionally (`stw r11, 0x10(r28)`) and
    // would fault on null; the assert above is the console's, the null test is the host's.
    if (lpSavedRival != 0)
    {
        lpSavedRival->meState = RivalData::E_STATE_BEATEN;
    }

    if (mProfile.FindCar(lCarId) == 0)
    {
        CarData* lpCarData = AddCar(lCarId, CarData::E_UNLOCK_TYPE_SHUTDOWN_RIVAL);
        // [GUARD] the console writes CarData+0xC / +0xA through AddCar's result without a test.
        if (lpCarData != 0)
        {
            if (leUnlockSequenceType == E_UNLOCK_SEQUENCE_TYPE_DEFAULT)
            {
                lpCarData->SetUnlockDeformationAmount(0.85f);          // flt_82029BB8
            }
            else if (leUnlockSequenceType == E_UNLOCK_SEQUENCE_TYPE_NONE)
            {
                lpCarData->SetUnlockSequenceAlreadyShown();
            }
        }
        CheckForAllRivalsUnlocked();
    }
}

// --------------------------------------------------------------------------------------------
// OnPursuitWon @0x82389F40 (r3 this, r4 lRivalId (64-bit CgsID), r5 the game-action queue).
// Caller: TakedownManager::ProcessTakedownEvent @0x823940F4..0x823940F8
// (`lwz r3, 0x290(r30)` == mpProgressionManager ; r4 = GetRivalId(victim) ; r5 = lpOutput->
// GetGameActionQueue()).
//   0x82389F60..0x82389F6C  liIndex = GetProgressionData()->FindRivalIndexFromId(lRivalId)
//                           (@0x82676A90 answers miRivalCount on a miss)
//   0x82389F7C..0x8238A00C  assert liIndex < miRivalCount, "Could not find rival: " + id  (:2400)
//   0x8238A010..0x8238A054  GetRival(liIndex) (its own :468 assert) -> r30
//   0x8238A058..0x8238A064  the same bound tested silently; a miss skips everything below
//   0x8238A068..0x8238A088  assert "lpRival != NULL" (:2410)
//   0x8238A08C..0x8238A0D8  r8 = this+0x170 (Profile); the inlined Profile::FindRival(lRivalId)
//                           walk (count +0x274, table +0x6280, 0x38 stride) -> r31
//   0x8238A0DC..0x8238A0F4  assert "lpSavedRival" (:2413)
//   0x8238A0F8..0x8238A104  DefeatRivalAndUnlockCar(liIndex, 0 == E_UNLOCK_SEQUENCE_TYPE_DEFAULT)
//   0x8238A108..0x8238A128  7x ld/std: *lpRival      -> var_D0 (+0x00 of the record)
//   0x8238A12C..0x8238A14C  7x ld/std: *lpSavedRival -> var_98 (+0x38)   [taken AFTER the defeat]
//   0x8238A150/0x8238A168   li 2 ; stw var_60        (+0x70) mePreviousState = E_STATE_FLEEING
//   0x8238A154              stb r25, var_5C          (+0x74) miRivalIndex
//   0x8238A15C..0x8238A16C  AddEvent(queue, &record, 0xC5 == 197, 0x78 == 120)
//   0x8238A170..0x8238A188  li 1 ; stb var_F0 ; AddEvent(queue, &byte, 0x37 == 55, 1)
//                           == E_ACTION_REQUEST_AUTOSAVE with the FORCED payload (see the
//                           mbAutosaveRequested note in BrnProgressionManager.h)
// --------------------------------------------------------------------------------------------
void ProgressionManager::OnPursuitWon(CgsID lRivalId, BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    const ProgressionData* lpProgressionData = GetProgressionData();

    const s32 liRivalIndex = lpProgressionData->FindRivalIndexFromId(lRivalId);
    // The console streams the missing id into the assert text through sub_82203EE8; reduced to
    // the project CGS_ASSERT form (the BrnNetworkSharedIO.cpp precedent).
    CGS_ASSERT(liRivalIndex < lpProgressionData->GetRivalCount(), "Could not find rival: <lRivalId>");   // :2400

    const Rival* lpRival = lpProgressionData->GetRival(liRivalIndex);   // carries the :468 assert

    if (liRivalIndex >= lpProgressionData->GetRivalCount())
    {
        return;
    }
    CGS_ASSERT(lpRival != 0, "lpRival != NULL");   // :2410

    RivalData* lpSavedRival = mProfile.FindRival(lRivalId);
    CGS_ASSERT(lpSavedRival != 0, "lpSavedRival");   // :2413

    DefeatRivalAndUnlockCar(liRivalIndex, E_UNLOCK_SEQUENCE_TYPE_DEFAULT);

    // [GUARD] the console's two copy loops read through both pointers unconditionally; a null
    // here (the asserts above have already fired) would fault, so the post is skipped instead.
    if (lpRival != 0 && lpSavedRival != 0)
    {
        BrnGameState::GameStateModuleIO::RivalStateChangeAction lAction;
        // The console leaves the record's three tail pad bytes as stack residue; zeroed for a
        // reproducible payload (the ShutdownAction precedent in BrnTakedownManager_Detect.cpp).
        std::memset(&lAction, 0, sizeof(lAction));
        lAction.mRival          = *lpRival;
        lAction.mRivalSavedData = *lpSavedRival;                 // already E_STATE_BEATEN
        lAction.mePreviousState = RivalData::E_STATE_FLEEING;    // the literal 2
        lAction.miRivalIndex    = static_cast<s8>(liRivalIndex);
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                    BrnGameState::GameStateModuleIO::E_ACTION_RIVAL_STATE_CHANGED,
                                    static_cast<s32>(sizeof(lAction)));
    }

    const u8 luForcedAutosave = 1;
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luForcedAutosave),
                                BrnGameState::GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE, 1);
}


// ==============================================================================================
// [progression wave 2026-09-06, lane rivals] THE RIVAL LEG THAT NEVER RAN ON PC.
// ==============================================================================================
// CarSelectManager::UpdateExitState raises mbUpdateRivals (X360 +0x20971) on EVERY junkyard exit
// -- that writer is live on PC -- and ProgressionManager::PreWorldUpdate @0x823A4F68 drains it
// through UpdateRivals. Neither UpdateRivals nor anything it calls had a body, so since the
// PreWorldUpdate landing (issue #10) every single log carried
//   "[FLAG PC bring-up] ProgressionManager::PreWorldUpdate: UpdateRivals() is NOT reconstructed"
// and no unlocked rival was ever handed to the world. Landed here:
//
//   UnlockRivals         @0x8236F658  (asserts BrnProgressionManager.cpp:1096, ProgressionData.h:468)
//   UpdateRivals         @0x82396298  (assert  BrnProgressionData.h:468)
//   AddRivalToWorld      @0x8238B0A8  (asserts BrnProgressionManager.cpp:3673 / :3674 / :3686,
//                                      CgsArray.h:336, BrnProgressionData.h:497)   -- PARTIAL, see below
//   OnTakedownTo         @0x823666D0  (assert  BrnTrainingManager.cpp:382, via RequestTraining)
//   OnOnlineRaceComplete @0x82366B98
//
// THE TWO GAME ACTIONS, pinned at BOTH ends (see BrnGameActions.h for the enumerators):
//   195 == E_ACTION_REMOVE_ALL_RIVALS  (size 1) -> RaceCarEntityModule::RemoveAllRivalsFromWorld
//   196 == E_ACTION_ADD_RIVAL          (size 176) -> RaceCarEntityModule::AddRivalCar
// (X360 RaceCarEntityModule::HandleGameActions @0x8230BE08 `case 195:` / `case 196:`; the DWARF
// spells them 187 / 188, the same +8 the whole 187..202 band takes -- RIVAL_STATE_CHANGED
// 189 -> 197 is the neighbour this file's OnPursuitWon already posts.)
//
// Every member is reached BY NAME; the console offsets are quoted to show which member each load
// lands on. The embedded Profile is this+0x170 on the console.
// ==============================================================================================

namespace
{
    // The console's own baked assert location (BeginAssert/FireAssert/EndAssert called directly
    // so the file/line stay the binary's -- the _PreWorldUpdate / _GameStats treatment).
    const char* const KAC_PROGRESSION_MANAGER_CPP_R =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

    // One-shot park reporter (campaign house rule: a park must be visible, not silent).
    void ParkOnceRivals(bool& lrbAlreadySaid, const char* lpcMessage)
    {
        if (lrbAlreadySaid)
        {
            return;
        }
        lrbAlreadySaid = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
    bool gbSaidNoTrainingManager = false;
    bool gbSaidNoProgressionData = false;
    bool gbSaidNoAchievementFwd  = false;
    // ---- [progression wave 2026-09-06, lane rivals2] ----
    bool gbSaidNoAISectionData   = false;

    // [FLAG PC witness] the `[rivals] ...` lines -- NOT IN THE X360 BINARY. Opt-in behind
    // BRN_PROGRESSION_RIVALS, every line first-N capped, none per-frame unbounded.
    // tools/tests/cases/progression_rivals.ps1 reads them.
    bool RivalsDiagEnabled()
    {
        static const bool sbDiag = (getenv("BRN_PROGRESSION_RIVALS") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0;
    }

    // Drain-scoped counters for the witness (one UpdateRivals "drain" spans as many frames as
    // ProgressionData has rivals -- the console advances miLastUpdatedRival by at most one
    // world-add per call).
    s32 giDrainPosted    = 0;
    s32 giDrainLines     = 0;
    s32 giUnlockLines    = 0;
    s32 giTakedownLines  = 0;
    s32 giAddLines       = 0;   // [rivals2] AddRivalToWorld's own witness
}

// ----------------------------------------------------------------------------------------------
// UnlockRivals @0x8236F658 -- "has the player earned enough medals to release the next rival?"
// Callers: OnEventFinishUpdateProfile @0x823A0040 (its answer becomes ShowModeResultsAction's
// +0xC8) and PreWorldUpdate @0x823A5160 (the medals-dirty arm).
//
//   0x8236F680  lwzx r31, r26, 0xA780      ; Profile+42512 == muMedalCountFromTheStart ...
//   0x8236F6A4  clrlwi r20, r31, 24        ; ... TRUNCATED TO A BYTE for the compare below
//   loop idx = 0 .. GetRivalCount()-1 (the count is re-read every iteration):
//     0x8236F6C8  the :468 "liIndex < miRivalCount" assert is GetRival's own
//     0x8236F6EC  lpRival = rivals + 0x38*idx
//     0x8236F6F4  assert "lpRival" (BrnProgressionManager.cpp:1096)
//     0x8236F714  lbz  0x16(lpRival) ; cmplw r20 ; bgt -> next   == GetNumMedalsToUnlock()
//     0x8236F720  lbz  0x17(lpRival) ; bne -> next               == GetIsUsedForRankUpGiftCar()
//     0x8236F72C  the inlined Profile::FindRival(lpRival->mId) walk (count +0x274, table
//                 +0x6280, 0x38 stride); a miss falls into Profile::AddRival(mId, mCarId)
//     0x8236F784  lwz 0x10(saved) ; bne -> next                  == meState != E_STATE_LOCKED
//     0x8236F7B8  if (gxMessageFilterFlags & 1) "Unlocking Rival: " << mId << "\n"
//     0x8236F818  stw 1, 0x10(saved)                             == E_STATE_UNLOCKED
//     0x8236F820  ld  r3, 8(lpRival)                             == RETURN Rival::mCarId
//   0x8236F7AC  li r3, 0   -- nothing eligible
//
// ⚠️ THE RETURN VALUE IS THE RIVAL'S **CAR** ID (`ld 8(rival)`, the same word DefeatRivalAndUnlock-
// Car feeds Profile::AddRival as the car id), even though the DWARF names the results-action field
// it lands in `mNewlyUnlockedRivalID`. Pinned by the load offset, not by the field name.
// ----------------------------------------------------------------------------------------------
CgsID ProgressionManager::UnlockRivals(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    // DWARF :661 gives the queue; the X360 body never touches r4 (it unlocks state only -- the
    // world side is UpdateRivals' job, reached through mbUpdateRivals).
    (void)lpGameActionQueue;

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [PC GUARD] the console reaches the resource through ResourcePtr::operator-> (whose own
        // assert fires); GetProgressionData answers 0 before the load completes on PC.
        ParkOnceRivals(gbSaidNoProgressionData,
                       "[FLAG PC bring-up] ProgressionManager: the ProgressionData resource is not "
                       "bound yet; the rival legs (UnlockRivals / UpdateRivals) no-op this frame.\n");
        return 0;
    }

    const u8 lu8MedalCount = static_cast<u8>(mProfile.GetMedalCountFromTheStart());   // Profile+42512

    for (s32 liIndex = 0; liIndex < lpProgressionData->GetRivalCount(); ++liIndex)
    {
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);   // carries the :468 assert
        CGS_ASSERT(lpRival != 0, "lpRival");                          // :1096
        if (lpRival == 0)
        {
            continue;   // [GUARD] the console reads through it regardless
        }

        if (lpRival->GetNumMedalsToUnlock() > lu8MedalCount)          // Rival+0x16
        {
            continue;
        }
        if (lpRival->GetIsUsedForRankUpGiftCar())                     // Rival+0x17
        {
            continue;
        }

        RivalData* lpSavedRival = mProfile.FindRival(lpRival->GetId());
        if (lpSavedRival == 0)
        {
            lpSavedRival = mProfile.AddRival(lpRival->GetId(), lpRival->GetCarId());
        }
        if (lpSavedRival == 0)
        {
            continue;   // [GUARD] a full maRivals table; the console would fault on `lwz 0x10`
        }
        if (lpSavedRival->meState != RivalData::E_STATE_LOCKED)
        {
            continue;
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            // The console streams the id through sub_82203EE8 (the StrStream CgsID formatter,
            // not in the tree); the raw 64-bit value is printed here -- OnPursuitWon's precedent.
            *CgsDev::Log::gpDebugPrint << "Unlocking Rival: " << static_cast<u64>(lpRival->GetId()) << "\n";
        }

        lpSavedRival->meState = RivalData::E_STATE_UNLOCKED;          // stw 1, 0x10(saved)

        if (RivalsDiagEnabled() && giUnlockLines < 16)
        {
            ++giUnlockLines;
            *CgsDev::Log::gpDebugPrint
                << "[rivals] unlock: medals=" << static_cast<s32>(lu8MedalCount)
                << " index="                  << liIndex
                << " rival="                  << static_cast<u64>(lpRival->GetId())
                << " car="                    << static_cast<u64>(lpRival->GetCarId())
                << "\n";
        }

        return lpRival->GetCarId();                                   // ld r3, 8(lpRival)
    }

    return 0;
}

// ----------------------------------------------------------------------------------------------
// UpdateRivals @0x82396298 -- the drain of mbUpdateRivals. Sole caller: PreWorldUpdate
// @0x823A52BC. The console advances ONE rival per call (miLastUpdatedRival, X360 +133416 ==
// 0x20928, DWARF BrnProgressionManager.h:841), so a full pass costs GetRivalCount() frames and the
// request byte only clears when the cursor reaches the end.
//
//   0x823962AC  r29 = this + 0x20928                     ; &miLastUpdatedRival
//   0x823962B4  if (miLastUpdatedRival == 0)
//   0x823962C0    AddEvent(queue, &var_60 /*1 untouched stack byte*/, 0xC3 == 195, 1)
//   loop while (miLastUpdatedRival < GetRivalCount()):
//     0x8239631C  the :468 assert is GetRival's own
//     0x82396340  r4 = rivals + 0x38*idx
//     0x8239634C  the inlined Profile::FindRival(lpRival->mId) walk (count +0x274, +0x6280, 0x38)
//     0x82396398  lwz 0x10(saved) ; == 0 -> next ; == 3 -> next   (LOCKED / BEATEN are skipped)
//     0x823963D4  AddRivalToWorld(lpRival, miLastUpdatedRival, queue) ; ++miLastUpdatedRival ; BREAK
//     0x823963AC  ++miLastUpdatedRival ; continue
//   0x823963F0  if (miLastUpdatedRival == GetRivalCount()) { miLastUpdatedRival = 0;
//   0x82396414                                              *(this + 0x20971) = 0; }
// ----------------------------------------------------------------------------------------------
void ProgressionManager::UpdateRivals(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (miLastUpdatedRival == 0)
    {
        // `li r6,1 ; li r5,0xC3 ; addi r4, r1, var_60` -- ONE byte the console never stores to.
        // RemoveAllRivalsFromWorld ignores the payload; zeroed for a reproducible one.
        u8 lacRemoveAllRivals[1] = { 0 };
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacRemoveAllRivals),
                                    BrnGameState::GameStateModuleIO::E_ACTION_REMOVE_ALL_RIVALS, 1);
        giDrainPosted = 0;
    }

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [PC GUARD] see UnlockRivals. The request byte stays set so the drain resumes when the
        // resource is bound, exactly as an unfinished console drain would.
        ParkOnceRivals(gbSaidNoProgressionData,
                       "[FLAG PC bring-up] ProgressionManager: the ProgressionData resource is not "
                       "bound yet; the rival legs (UnlockRivals / UpdateRivals) no-op this frame.\n");
        return;
    }

    while (miLastUpdatedRival < lpProgressionData->GetRivalCount())
    {
        const s32    liIndex = miLastUpdatedRival;
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);   // carries the :468 assert

        const RivalData* lpSavedRival = (lpRival != 0) ? mProfile.FindRival(lpRival->GetId()) : 0;
        if (lpSavedRival != 0 &&
            lpSavedRival->meState != RivalData::E_STATE_LOCKED &&      // `cmpwi 0 ; beq -> next`
            lpSavedRival->meState != RivalData::E_STATE_BEATEN)        // `cmpwi 3 ; beq -> next`
        {
            AddRivalToWorld(lpRival, liIndex, lpGameActionQueue);
            ++miLastUpdatedRival;
            ++giDrainPosted;
            break;                                                     // one add per call
        }
        ++miLastUpdatedRival;
    }

    if (miLastUpdatedRival == lpProgressionData->GetRivalCount())
    {
        miLastUpdatedRival      = 0;
        mbUpdateRivalsRequested = false;                               // stbx 0 -> this+0x20971

        if (RivalsDiagEnabled() && giDrainLines < 32)
        {
            ++giDrainLines;
            s32 liProfileRivals = mProfile.GetRivalCount();
            s32 liUnlocked      = 0;
            for (s32 liRival = 0; liRival < liProfileRivals; ++liRival)
            {
                const RivalData* lpcSaved = mProfile.GetRivalData(liRival);
                if (lpcSaved != 0 &&
                    lpcSaved->meState != RivalData::E_STATE_LOCKED &&
                    lpcSaved->meState != RivalData::E_STATE_BEATEN)
                {
                    ++liUnlocked;
                }
            }
            *CgsDev::Log::gpDebugPrint
                << "[rivals] update: profileRivals=" << liProfileRivals
                << " unlocked="                      << liUnlocked
                << " posted="                        << giDrainPosted
                << " authoredRivals="                << lpProgressionData->GetRivalCount()
                << "\n";
        }
        giDrainPosted = 0;
    }
}

// ----------------------------------------------------------------------------------------------
// AddRivalToWorld @0x8238B0A8 -- builds the 176-byte AddRivalCar record and posts it as game
// action 196. Sole caller: UpdateRivals @0x823963E0.
//
//   0x8238B0CC  the inlined Profile::FindRival(lpRival->mId) walk               -> lpRivalSavedData
//   0x8238B100  lbz 0x14(lpRival) ; extsb                                        == GetDistrict()
//   0x8238B114  assert "lpRivalSavedData"                        (BrnProgressionManager.cpp:3673)
//   0x8238B134  assert "leRivalDistrict < BrnWorld::E_DISTRICT_VALID_COUNT" (:3674, bound 0x12)
//   0x8238B158  7x ld/std   *lpRival          -> record + 0x20   (the whole 56-byte Rival)
//   0x8238B180  lhz 0x10(lpRival) ; extsh                         == GetPersonalityIndex()
//   0x8238B18C  GetProgressionData()->GetPersonality(index)       (its own :497 assert)
//   0x8238B1DC  4x lwz/stw  *personality      -> record + 0x58    (EventRacerPersonality, 16 B)
//   0x8238B200  7x ld/std   *lpRivalSavedData -> record + 0x68    (the whole 56-byte RivalData)
//   0x8238B218  stb  liRivalIndex             -> record + 0xA2
//   0x8238B224  r31 = this + 20 * leRivalDistrict                 == &maRoamingSections[district]
//   0x8238B228  lwz 0x10(r31)  -> the CgsArray.h:336 "Array used before Construct/Clear" assert
//   0x8238B25C  assert "maRoamingSections[leRivalDistrict].GetLength() > 0"                (:3686)
//   0x8238B2A8  GetItem(liRivalIndex % GetLength())               -> the AI section index (s16)
//   0x8238B2D4  mpAISectionData->GetAISection(section)->GetMiddle()
//   0x8238B2E8  sth  section                 -> record + 0xA0
//   0x8238B30C  lvx/stvx unk_82181520        -> record + 0x10     (image bytes 00000000 00000000
//                                                                  3F800000 00000000 == (0,0,1,0),
//                                                                  the spawn heading)
//   0x8238B318  lvx/stvx GetMiddle()         -> record + 0x00     (the spawn position)
//   0x8238B324  AddEvent(queue, record, 0xC4 == 196, 0xB0 == 176)
//
// ⭐ [progression wave 2026-09-06, lane rivals2] THE PARK IS GONE -- THIS IS NOW THE WHOLE
// CONSOLE BODY. Both holes the earlier park named were PRODUCER holes, and the lifecycle lane
// filled both in this same wave:
//   (1) `maRoamingSections` is now the DWARF's `RoamingSections maRoamingSections[18]`
//       (`typedef Array<u16,8>`, BrnGameStateTypes.h:202) -- the header defect this file found is
//       corrected -- and ProgressionManager::SetupRoamingSections @0x8236FE60 fills it from
//       Prepare2's tail (the live log prints `[lifecycle] prepare2: ... roamingSections=139`).
//   (2) `mpAISectionData` (X360 +133380) is bound by ProgressionManager::LoadAIData @0x8239A0D0
//       (`[lifecycle] prepare: aiSections=1 stage=4`).
// So the section pick, the AI-section spawn point and the action-196 post are all landed below.
//
// THE RECORD (176 bytes == 0xB0, the size the console hands AddEvent). The stack offsets fall out
// of the frame: AddEvent's r4 is `0x160+var_100`, so var_100 IS record+0x00 and every other var_
// lands at (0x100 - var) -- var_F0 -> +0x10, var_E0 -> +0x20, var_A8 -> +0x58, var_98 -> +0x68,
// var_60 -> +0xA0, var_5E -> +0xA2. Modelled below as `AddRivalCarAction`, offsets pinned by
// static_assert, every field reached BY NAME.
//
// THE THREE PC GUARDS (each marked [PC GUARD] in the body): a null Rival/RivalData, an
// out-of-range district, and an EMPTY/unconstructed district section array. All three are states
// the console's own asserts (:3673 / :3674 / :3686 + CgsArray.h:336) halt a dev build on, and the
// console's fall-through past them is an out-of-bounds `GetItem` or the `twllei r11, 0` divide
// trap -- so the post is skipped instead of faulting. The asserts still fire first, always.
// ----------------------------------------------------------------------------------------------

namespace
{
    // ------------------------------------------------------------------------------------------
    // The 176-byte E_ACTION_ADD_RIVAL (196) payload -- RaceCarEntityModule::AddRivalCar's record.
    // Homed HERE, not in BrnGameActions.h, because that header belongs to another lane this wave
    // and because the PC CONSUMER (AddRivalCar / RemoveAllRivalsFromWorld) is still absent: there
    // is exactly one user, this producer.
    // REHOME-WHEN AddRivalCar lands: move this next to RivalStateChangeAction in BrnGameActions.h
    // so producer and consumer share one definition.
    //
    // The X360 offsets fall out of the host's own natural layout (Vector3 is the 16-byte SIMD
    // alias; Rival and RivalData are 56 bytes each; EventRacerPersonality is 16) -- the
    // static_asserts below are the proof, not an assumption.
    // ------------------------------------------------------------------------------------------
    struct AddRivalCarAction
    {
        Vector3                               mSpawnPosition;      // +0x00  stvx GetMiddle()
        Vector3                               mSpawnHeading;       // +0x10  stvx unk_82181520
        BrnProgression::Rival                 mRival;              // +0x20  7x ld/std, 56 B
        BrnProgression::EventRacerPersonality mPersonality;        // +0x58  4x lwz/stw, 16 B
        BrnProgression::RivalData             mRivalSavedData;     // +0x68  7x ld/std, 56 B
        s16                                   mi16AISectionIndex;  // +0xA0  sth r31
        u8                                    mu8RivalIndex;       // +0xA2  stb r26
        u8                                    mau8Pad[13];         // +0xA3  stack residue on the
                                                                   //        console; zeroed here
    };

    static_assert(sizeof(AddRivalCarAction) == 0xB0,
                  "the AddRivalCar record is the 176 bytes AddEvent is given (li r6, 0xB0)");
    static_assert(offsetof(AddRivalCarAction, mSpawnPosition)     == 0x00, "record +0x00");
    static_assert(offsetof(AddRivalCarAction, mSpawnHeading)      == 0x10, "record +0x10");
    static_assert(offsetof(AddRivalCarAction, mRival)             == 0x20, "record +0x20");
    static_assert(offsetof(AddRivalCarAction, mPersonality)       == 0x58, "record +0x58");
    static_assert(offsetof(AddRivalCarAction, mRivalSavedData)    == 0x68, "record +0x68");
    static_assert(offsetof(AddRivalCarAction, mi16AISectionIndex) == 0xA0, "record +0xA0");
    static_assert(offsetof(AddRivalCarAction, mu8RivalIndex)      == 0xA2, "record +0xA2");
}

// ----------------------------------------------------------------------------------------------
// BrnProgression::Rival::GetPersonalityIndex -- BODIED HERE. It was in BrnRival.h's declared-only
// list and defined nowhere in the tree (the same latent unresolved-external state GetId /
// GetCarId / GetNumMedalsToUnlock were in), and it surfaces the moment its first caller is
// mounted. No standalone X360 symbol: the console folds it into AddRivalToWorld as a bare
// `lhz r11, 0x10(r31) ; extsh r31, r11` on the 56-byte-strided record -- a SIGN-extended halfword
// load, which is where the declaration's s16 return comes from. Out-of-line here (rather than a
// BrnRival.h inline like its siblings) because this wave's lane ownership puts that header
// outside this lane; REHOME it there when a header-owning lane or the Rival TU lands. Grepped:
// no other definition exists in the tree.
// ----------------------------------------------------------------------------------------------
s16 Rival::GetPersonalityIndex() const
{
    return miPersonalityIndex;   // Rival+0x10
}

// ----------------------------------------------------------------------------------------------
void ProgressionManager::AddRivalToWorld(const Rival* lpRival, s32 liRivalIndex,
                                         BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    const RivalData* lpRivalSavedData = (lpRival != 0) ? mProfile.FindRival(lpRival->GetId()) : 0;
    const s32 leRivalDistrict = (lpRival != 0) ? static_cast<s32>(lpRival->GetDistrict()) : 0;

    if (lpRivalSavedData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpRivalSavedData", KAC_PROGRESSION_MANAGER_CPP_R, 3673);
        CgsDev::Assert::EndAssert();
    }
    if (leRivalDistrict >= 18)   // BrnWorld::E_DISTRICT_VALID_COUNT (`cmpwi r28, 0x12`)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("leRivalDistrict < BrnWorld::E_DISTRICT_VALID_COUNT",
                                   KAC_PROGRESSION_MANAGER_CPP_R, 3674);
        CgsDev::Assert::EndAssert();
    }

    // [PC GUARD] the console reads through both pointers and indexes maRoamingSections with the
    // district unconditionally; the two asserts above have already fired, so the post is skipped
    // rather than faulting (the OnPursuitWon precedent above).
    if (lpRival == 0 || lpRivalSavedData == 0 ||
        leRivalDistrict < 0 || leRivalDistrict >= KI_DISTRICT_COUNT)
    {
        return;
    }

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [PC GUARD] see UnlockRivals: the console reaches the resource through
        // ResourcePtr::operator-> (whose own assert fires); GetProgressionData answers 0 before
        // the load completes on PC. UpdateRivals cannot reach here with a null today.
        ParkOnceRivals(gbSaidNoProgressionData,
                       "[FLAG PC bring-up] ProgressionManager: the ProgressionData resource is not "
                       "bound yet; the rival legs (UnlockRivals / UpdateRivals) no-op this frame.\n");
        return;
    }

    AddRivalCarAction lAction;
    // The console leaves the record's 13 tail pad bytes as stack residue; zeroed here for a
    // reproducible payload (the RivalStateChangeAction precedent above).
    std::memset(&lAction, 0, sizeof(lAction));

    lAction.mRival = *lpRival;                                     // 7x ld/std  -> record +0x20

    // `lhz 0x10(lpRival) ; extsh` then GetPersonality, whose OWN :497 assert
    // ("luIndex < muPersonalityCount", BrnProgressionData.h) fires inside it -- not restated here.
    lAction.mPersonality = *lpProgressionData->GetPersonality(
        static_cast<u32>(lpRival->GetPersonalityIndex()));          // 4x lwz/stw -> record +0x58

    lAction.mRivalSavedData = *lpRivalSavedData;                    // 7x ld/std  -> record +0x68
    lAction.mu8RivalIndex   = static_cast<u8>(liRivalIndex);        // stb r26    -> record +0xA2

    // `slwi r11, r28, 2 ; add r11, r28, r11 ; slwi r11, r11, 2 ; add r31, r11, r27`
    // == this + 20 * district == &maRoamingSections[district] (the 20-byte Array<u16,8> stride).
    const RoamingSections& lrDistrictSections = maRoamingSections[leRivalDistrict];

    // GetLength() carries CgsArray's own "Array used before Construct/Clear was called" assert
    // (CgsArray.h:336) -- exactly the console's inlined `lwz 0x10(r31) ; cmpwi -1` pair at
    // 0x8238B228. The console calls GetLength() TWICE (again at 0x8238B284, for the modulo), so
    // both reads are spelled out here rather than cached.
    if (!(lrDistrictSections.GetLength() > 0))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("maRoamingSections[leRivalDistrict].GetLength() > 0",
                                   KAC_PROGRESSION_MANAGER_CPP_R, 3686);
        CgsDev::Assert::EndAssert();
    }

    // [PC GUARD] an empty (or never-Construct'ed) district array: the console guards its `divwu`
    // by that length only with `twllei r11, 0`, i.e. it TRAPS -- there is no console behaviour to
    // reproduce past the assert above. Districts with no authored RoamingLocation are real
    // (SetupRoamingSections fills 139 sections across the 18 districts, not 18 x 8).
    if (lrDistrictSections.GetCount() <= 0)
    {
        if (RivalsDiagEnabled() && giAddLines < 16)
        {
            ++giAddLines;
            *CgsDev::Log::gpDebugPrint
                << "[rivals] add: index=" << liRivalIndex
                << " district="           << leRivalDistrict
                << " sections=0 posted=0 (no roaming section authored in this district)\n";
        }
        return;
    }

    // `divwu / mullw / subf` == the UNSIGNED modulo of the rival index by the district's section
    // count; GetItem returns the u16 the console loads with `lhz 0(r11)`.
    const u32 luSectionSlot = static_cast<u32>(liRivalIndex) % lrDistrictSections.GetLength();
    const u16 lu16AISection = lrDistrictSections.GetItem(luSectionSlot);
    lAction.mi16AISectionIndex = static_cast<s16>(lu16AISection);   // sth r31 -> record +0xA0

    // [PC GUARD] LoadAIData @0x8239A0D0 binds mpAISectionData before Prepare2's tail runs
    // SetupRoamingSections, so a FILLED roaming array implies a bound resource; the console
    // dereferences it unconditionally (`addis r3, r27, 2 ; addi r3, r3, 0x904`).
    if (!mpAISectionData.HasMemoryResource())
    {
        ParkOnceRivals(gbSaidNoAISectionData,
                       "[FLAG PC bring-up] ProgressionManager::AddRivalToWorld @0x8238B0A8: "
                       "mpAISectionData is not bound (LoadAIData @0x8239A0D0 has not completed), "
                       "so the rival's spawn position cannot be read; game action 196 was NOT "
                       "posted this pass.\n");
        return;
    }

    const BrnAI::AISectionsData* const lpAISectionsData = mpAISectionData.operator->();
    const BrnAI::AISection* const      lpAISection      =
        lpAISectionsData->GetAISection(static_cast<u32>(lu16AISection));

    // [PC GUARD] GetAISection carries its own bounds assert; a null here would fault in GetMiddle.
    if (lpAISection == 0)
    {
        return;
    }

    lAction.mSpawnPosition = lpAISection->GetMiddle();              // stvx -> record +0x00
    // unk_82181520, read out of the image at file offset 0x181520:
    //   00000000 00000000 3F800000 00000000 == (0.0f, 0.0f, 1.0f, 0.0f)
    // i.e. +Z -- the spawn heading every rival is dropped into the world with.
    // The host Vector3 is the four-lane POD (x, y, z, w), so the aggregate carries the console's
    // w lane (0.0f) as well -- all sixteen bytes the stvx writes.
    lAction.mSpawnHeading = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };      // stvx -> record +0x10

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                BrnGameState::GameStateModuleIO::E_ACTION_ADD_RIVAL,
                                static_cast<s32>(sizeof(lAction)));  // li r5, 0xC4 ; li r6, 0xB0

    if (RivalsDiagEnabled() && giAddLines < 16)
    {
        ++giAddLines;
        *CgsDev::Log::gpDebugPrint
            << "[rivals] add: index="  << liRivalIndex
            << " district="            << leRivalDistrict
            << " sectionSlot="         << static_cast<s32>(luSectionSlot)
            << " section="             << static_cast<s32>(lu16AISection)
            << " rival="               << static_cast<u64>(lpRival->GetId())
            << " state="               << static_cast<s32>(lpRivalSavedData->meState)
            << " spawn="               << lAction.mSpawnPosition.x
            << ","                     << lAction.mSpawnPosition.z
            << " posted=196 size="     << static_cast<s32>(sizeof(lAction))
            << "\n";
    }
}

// ----------------------------------------------------------------------------------------------
// OnTakedownTo @0x823666D0 -- the PROGRESSION side of "the player took somebody down", the offline
// arm of GameStateModule::ProcessTakedownEvents @0x8238FD98.
//
//   0x823666EC  lwzx 0x208D0        ; mpCurrentCarData == 0        -> straight to the tally
//   0x823666F8  clrlwi r7           ; lbMarkedManTakeDown != 0     -> straight to the tally
//   0x8236670C  lwzx 0x1CE2C        ; Profile+117948 == meCurrentCarType ; == 1 ? arm A : arm B
//   arm A (E_CARTYPE_AGGRESSION):
//     0x82366720  ldx 0x1CE30       ; Profile+117952 == maHasPlayerSeenTraining word 0 (the
//                                     BitArray is 64-BIT worded: HasPlayerSeenTrainingType
//                                     @0x8231C878 does `word = type >> 6`, `bit = 1 << (type & 63)`)
//     0x82366724  rlwinm ...,0,9,9  ; mask 0x00400000 == bit 22    == seen(AGGRESSION_TAKEDOWN)
//     0x82366744  rlwinm ...,0,8,8  ; mask 0x00800000 == bit 23    == seen(AGGRESSION_LOST_BOOST_CHUNK)
//     0x82366764..0x82366818  the open-coded TrainingManager::RequestTraining(22) gauntlet:
//                 state == INACTIVE, !mbInPictureParadise, IsTipAllowedInGameMode(22),
//                 the "lpProfile" assert (BrnTrainingManager.cpp:382),
//                 !HasPlayerSeenTrainingType(22), (Profile+0x6C - tm+0x18) >= flt_8200426C (5.0f),
//                 then `*(tm+4) = 22 ; *(tm+0) = 1`.
//   arm B (any other car type): the identical gauntlet for type 0x28 == 40.
//   0x823668D8  BOTH ARMS AND EVERY EARLY-OUT: Profile::AddTakedown(leType).
//
// The gauntlet IS RequestTraining specialised (the tree's body at BrnTrainingManager.cpp:507 runs
// the same tests in the same order -- neither 22 nor 40 is in its 5-second-bypass set
// {0,1,2,7,17,33,39}), so the inlining is reversed into the real call, exactly as
// OnEventFinishUpdateProfile's WON_EVENT tip already is.
//
// ⚠️ r4 (the game-action queue) and r6 (the rival CgsID) are DEAD in this body -- nothing reads
// either register. The DWARF signature keeps them; the rival's own tally lives in the takedown
// manager's OnPursuitWon path, not here.
// ----------------------------------------------------------------------------------------------
void ProgressionManager::OnTakedownTo(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                      BrnGameState::ETakedownType leType,
                                      CgsID lRivalId, bool lbMarkedManTakeDown)
{
    (void)lpGameActionQueue;
    (void)lRivalId;

    if (mpCurrentCarData != 0 && !lbMarkedManTakeDown)
    {
        ETrainingType leTip = E_TRAINING_TYPE_TAKEDOWN;               // arm B, `li r4, 0x28`
        bool          lbRequest = true;

        if (mProfile.GetCurrentCarType() == BrnResource::E_CARTYPE_AGGRESSION)   // Profile+117948 == 1
        {
            leTip = E_TRAINING_TYPE_AGGRESSION_TAKEDOWN;              // arm A, `li r4, 0x16`
            lbRequest =
                !mProfile.HasPlayerSeenTrainingType(E_TRAINING_TYPE_AGGRESSION_TAKEDOWN) &&
                !mProfile.HasPlayerSeenTrainingType(E_TRAINING_TYPE_AGGRESSION_LOST_BOOST_CHUNK);
        }

        if (lbRequest)
        {
            if (mpTrainingManager != 0)
            {
                mpTrainingManager->RequestTraining(leTip);
            }
            else
            {
                // The same hole OnEventFinishUpdateProfile's WON_EVENT tip sits in: nothing calls
                // SetTrainingManager because the outer ProgressionManager::Construct/Prepare pair
                // is not reconstructed. DELETE-WHEN that installer lands.
                ParkOnceRivals(gbSaidNoTrainingManager,
                               "[FLAG PC bring-up] ProgressionManager::OnTakedownTo: "
                               "mpTrainingManager (X360 +133440) is NULL -- nothing calls "
                               "SetTrainingManager yet, so the takedown training tip was not "
                               "requested. The takedown itself IS tallied.\n");
            }
        }

        if (RivalsDiagEnabled() && giTakedownLines < 16)
        {
            ++giTakedownLines;
            *CgsDev::Log::gpDebugPrint
                << "[rivals] takedownTo: type=" << static_cast<s32>(leType)
                << " carType="                  << mProfile.GetCurrentCarType()
                << " tip="                      << static_cast<s32>(leTip)
                << " requested="                << (lbRequest ? 1 : 0)
                << "\n";
        }
    }

    mProfile.AddTakedown(leType);                                     // 0x823668E0
}

// ----------------------------------------------------------------------------------------------
// OnOnlineRaceComplete @0x82366B98 -- ONLINE. Caller: ModeManager::SendModeStopMessages
// @0x8234BEC0 (the `!timedOut && meCurrentGameModeType == E_MODE_ONLINE_RACE` arm, which is inside
// that function's still-deferred online block -- see BrnModeManager_Start.cpp).
//
//   0x82366B9C  r10 = this + 0x170                                   ; the embedded Profile
//   0x82366BAC  lwz/addi/stw  profile + 0x1CCE0                      ; ++miNumOnlineRacesDone
//   0x82366BB8  if (lbWonRace) lwz/addi/stw profile + 0x1CCE4        ; ++miNumOnlineRacesWon
//               (that pair IS the inlined DWARF `Profile::OnOnlineRaceComplete(int32_t, bool)`;
//                the player-count argument is not stored anywhere)
//   0x82366BE8  r7 = *(this + 0x1CE50) == profile + 0x1CCE0          ; the updated "done" tally
//   0x82366BEC  r6 = *(this + 0x1CE54) == profile + 0x1CCE4          ; the updated "won" tally
//   0x82366BF0  r3 = *(this + 0x20938) == mpAchievementManager
//   0x82366BF4  b  AchievementManagerBase::OnOnlineRaceComplete      ; r4/r5 still hold our args
// ----------------------------------------------------------------------------------------------
void ProgressionManager::OnOnlineRaceComplete(s32 liNumberOfPlayers, bool lbWonRace)
{
    mProfile.OnOnlineRaceComplete(liNumberOfPlayers, lbWonRace);

    // ⛔ PARK -- console: `AchievementManagerBase::OnOnlineRaceComplete(mpAchievementManager,
    //   liNumberOfPlayers, lbWonRace, mProfile.GetNumOnlineRacesDone(),
    //   mProfile.GetNumOnlineRacesWon());`  (the tail branch at 0x82366BF4, whose r6/r7 are the
    // two counters just bumped). The body EXISTS in the tree -- BrnGameStateAchievementManager-
    // PS3.cpp:193, X360 0x8235B6A8, achievements 35/36/37/38 -- but that TU is NOT MOUNTED in
    // tools/build/build_game_exe.bat, so calling it is an LNK2019. Every other consumer of that
    // class parks around the same hole (see the P2 parks in GameStateModule_gRR_00.cpp).
    // The profile tallies above ARE written, which is the part this function owns.
    // DELETE-WHEN BrnGameStateAchievementManagerPS3.cpp is mounted.
    ParkOnceRivals(gbSaidNoAchievementFwd,
                   "[FLAG PC bring-up] ProgressionManager::OnOnlineRaceComplete: "
                   "AchievementManagerBase::OnOnlineRaceComplete @0x8235B6A8 is bodied but its TU "
                   "is not mounted; the online-race achievements were not evaluated. The profile "
                   "tallies WERE updated.\n");
    (void)mpAchievementManager;
}

}
