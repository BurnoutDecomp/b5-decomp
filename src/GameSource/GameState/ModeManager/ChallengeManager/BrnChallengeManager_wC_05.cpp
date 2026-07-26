// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager_wC_05.cpp
// ============================================================================
// Wave-C partfile (group G6) for BrnGameState::ChallengeManager. One function,
// reconstructed store-for-store from the X360 ARTIST asm over the keystone's named
// members and committed accessors:
//   PostWorldUpdate  @ 0x8233AC28
//
// The X360 body is the challenge manager's END-of-frame pass while a challenge is
// RUNNING: it tallies the frame's race-car crashes between challenge participants,
// clears this network frame's success bit for every REMOTE player's success ring
// (only the local player's bit is left for the success-update path to set), and then
// forwards the stunt-score snapshot to UpdateStuntScores.
//
// SIGNATURE (asm-derived, matches the frozen header): three params -- r4 the crash
// event queue, r5 the stunt-score snapshot (spilled to its home slot at the prologue
// `stw r5, arg_24(r1)` and reloaded at the tail `lwz r4, arg_24(r1)` immediately
// before the UpdateStuntScores tail call, i.e. passed straight through), r6 the timer
// status interface.
// ============================================================================

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"

// Completes VehicleManagerOutputInterface::RaceCarCrashEventQueue (the r4 param) --
// EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent,8>, carrying GetLength()/GetEvent(i),
// which are the X360's `lwz 8(queue)` and the per-element helper call.
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"
// BrnPhysics::Vehicle::RaceCarCrashEvent itself (mRaceCarVolumeInstanceID / mCrasherEntityID).
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
// CgsSceneManager::EntityId -- the committed decoder for the packed 32-bit entity word
// (GetOwner / GetEntityIndex); see the crash-walk comment below.
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
// CgsSystem::TimerStatusInterface::IsSimTimerFrequency50Hz() -- the X360 inlines this as
// the `(mfBaseTimeStep * mfTimeStepMultiplier) == 0.02f` product on the SIM sub-status
// (lfs 0x1C / lfs 0x20 == mSimTimerStatus +4 / +8).
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
// GameStateModule::GetPlayerActiveRaceCarIndex() (the inline accessor with its own
// "module is updating" assert), reached through mpGameStateModule @+0xE48.
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// ChallengeManager::PostWorldUpdate -- X360 0x8233AC28
// ----------------------------------------------------------------------------
// Whole body is gated on meChallengeManagerStatus == RUNNING (lwz 0xE08 / cmpwi 2 /
// bne -> epilogue); nothing at all happens in any other state.
//
// (1) CRASH WALK. The event count is read ONCE before the loop (`lwz r27,8(r28)`) and
//     the loop runs i in [0, count). Each event is fetched through the queue's element
//     accessor. Only crashes whose CRASHER entity word carries owner byte 1 (the race-car
//     entity type -- `srwi r10,r11,24 ; cmplwi r10,1`) are considered; for those the two
//     participants' active-race-car slots are decoded out of the packed handles, range
//     asserted (cpp:751 / cpp:752), and -- when BOTH cars are in the challenge -- each
//     car's "crashed with a challenge player" tally is bumped.
//
//     DECODE: both slots are the 14-bit entity-index field at bit 10 of a packed entity
//     word (owner:[31..24] | entityIndex:[23..10] | partIndex:[9..0]).
//       * crashED comes from mRaceCarVolumeInstanceID, whose entity word is the HIGH dword
//         (`ld 0(event) ; srdi 32 ; extrwi 14,8`) -- exactly the committed
//         VolumeInstanceId::GetEntityIDEntityIndex(), which documents that same inline.
//       * crashER comes from mCrasherEntityID (`lwz 8(event)`), the bare 32-bit entity word.
//         The minimal BrnCommonTypes.h `EntityId` is only the storage word; the committed
//         decoder for that word is CgsSceneManager::EntityId (GetOwner / GetEntityIndex,
//         the same 8/14/10 geometry), so the word is bridged into it here rather than
//         re-spelling the bit math (the file-local-helper alternative used by
//         BrnScoringSystem_UpdateA.cpp predates that accessor).
//
// (2) NETWORK-FRAME BIT. liBit = (miFramesSinceNetworkStart + 1) modulo the ring length
//     of the success array: 100 slots at 50Hz, 120 otherwise. The X360 emits the modulo as
//     the usual magic-multiply/shift/multiply-subtract pair (0x51EB851F>>5 for 100,
//     0x88888889>>6 for 120) -- written back as `%` per the de-optimisation rule.
//
// (3) REMOTE-PLAYER CLEAR. Walk every active-race-car slot with the range-guarded
//     operator++ (its "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert is
//     BurnoutConstants.h:39, seen inlined in the loop tail). mpGameStateModule is asserted
//     EVERY iteration (cpp:779 -- the X360 re-loads and re-tests +0xE48 inside the loop),
//     and every slot that is NOT the local player has this frame's bit cleared. The giant
//     "Index N is out of range (max bits: 120)" StrStream block at 0x8233AE68 is
//     FastBitArray's own inlined range assert (CgsFastBitArray.h:452), so the container
//     method is called instead of reproducing it.
//
// (4) TAIL. UpdateStuntScores(lpStuntScoreInfo) -- the r5 param, untouched.
// ----------------------------------------------------------------------------
void ChallengeManager::PostWorldUpdate(
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashEventQueue,
    const void*                                                  lpStuntScoreInfo,
    const CgsSystem::TimerStatusInterface*                       lpTimerStatusInterface)
{
    if (meChallengeManagerStatus != E_CHALLENGE_MANAGER_STATUS_RUNNING)   // X360 lwz 0xE08, cmpwi 2
    {
        return;
    }

    const s32 liNumCrashEvents = lpRaceCarCrashEventQueue->GetLength();   // X360 lwz 8(queue), hoisted

    for (s32 liCrashEvent = 0; liCrashEvent < liNumCrashEvents; ++liCrashEvent)
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lCrashEvent =
            lpRaceCarCrashEventQueue->GetEvent(liCrashEvent);

        // Packed crasher entity word (event +8) decoded through the committed EntityId.
        const CgsSceneManager::EntityId lCrasherEntityID(lCrashEvent.mCrasherEntityID.muValue);

        // Owner byte 1 == the race-car entity type; every other owner is skipped outright
        // (the X360 branches straight to the loop increment). Named per the committed
        // BrnCrashModeScoring.cpp:528 precedent so the un-homed enum stays greppable.
        const u32 K_OWNER_RACE_CAR = 1;   // BrnWorld::E_ENTITYTYPE_RACE_CAR (FLAG: enum un-homed)
        if (lCrasherEntityID.GetOwner() == K_OWNER_RACE_CAR)
        {
            const ::EActiveRaceCarIndex leCrasherActiveRaceCarIndex =
                static_cast< ::EActiveRaceCarIndex>(lCrasherEntityID.GetEntityIndex());
            const ::EActiveRaceCarIndex leCrashedActiveRaceCarIndex =
                static_cast< ::EActiveRaceCarIndex>(
                    lCrashEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());

            CGS_ASSERT((leCrasherActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leCrasherActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "( leCrasherActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                       "( leCrasherActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");  // X360 line 751
            CGS_ASSERT((leCrashedActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leCrashedActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "( leCrashedActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                       "( leCrashedActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");  // X360 line 752

            // Both cars must be taking part in the current challenge (X360 tests the CRASHED
            // car's flag first, then the CRASHER's, both off the +0x598 byte array).
            if (mabPlayerStartedChallenge[leCrashedActiveRaceCarIndex] &&
                mabPlayerStartedChallenge[leCrasherActiveRaceCarIndex])
            {
                ++maiCrashedWithChallengePlayer[leCrashedActiveRaceCarIndex];
                ++maiCrashedWithChallengePlayer[leCrasherActiveRaceCarIndex];
            }
        }
    }

    // Success-ring slot for this network frame: 100 slots when the sim timer runs at 50Hz,
    // 120 otherwise (X360: the 0.02f time-step product, then the magic-multiply modulo).
    const s32 liNetworkFrame = miFramesSinceNetworkStart + 1;
    const s32 liSuccessUpdateBit =
        liNetworkFrame % (lpTimerStatusInterface->IsSimTimerFrequency50Hz() ? 100 : 120);

    for (::EActiveRaceCarIndex leActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         static_cast<s32>(leActiveRaceCarIndex) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++)
    {
        CGS_ASSERT(mpGameStateModule, "mpGameStateModule");    // X360 line 779 (re-tested each pass)

        if (static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex()) !=
            static_cast<s32>(leActiveRaceCarIndex))
        {
            maPlayerSuccessUpdateArray[leActiveRaceCarIndex].UnSetBit(
                static_cast<u32>(liSuccessUpdateBit));
        }
    }

    UpdateStuntScores(lpStuntScoreInfo);
}

}
