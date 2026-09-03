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

}
