// BrnWorld::CrashModule -- three out-of-line members the X360 ARTIST build emitted for the world
// crash module (World/CrashModule/BrnCrashModule.cpp). See BrnCrashModule.h for the class banner
// and the X360 member-offset map.
//
// Reconstructed methods (X360 spine authoritative):
//   CrashModule()                                   @ 0x827DE960
//   FindCrashForRaceCar(EActiveRaceCarIndex) const  @ 0x827C6AD8
//   WillTrafficVehicleBeRecycledNextFrame(u16)      @ 0x827BBB10
#define CGS_ARTIST_BRNCRASHMODULE_H_FILE "..\\..\\..\\GameSource\\World/CrashModule/BrnCrashModule.h"
#define CGS_ARTIST_CGSFASTBITARRAY_H_FILE "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsFastBitArray.h"
#define CGS_ARTIST_UNITY_BRNCRASHMODULE_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/CrashModule/BrnCrashModule.cpp"
#include "GameSource/World/CrashModule/BrnCrashModule.h"

#include "GameSource/BurnoutConstants.h"                                   // E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>

namespace BrnWorld
{
    // ARTIST 0x827D08D0. Actions are handled even when the simulation update is paused.
    void CrashModule::HandleGameActions(const CrashIO::InputBuffer_PreScene* lpInput,
                                        CrashIO::OutputBuffer_PreScene* lpOutput)
    {
        using namespace BrnGameState::GameStateModuleIO;
        CGS_ASSERT_AT(lpInput, "lpInput", CGS_ARTIST_UNITY_BRNCRASHMODULE_CPP_FILE, 690);
        const CrashIO::InputBuffer_PreScene::GameActionQueue* lpQueue = lpInput->GetGameActionQueue();
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liAction = lpQueue->GetFirstEvent(&lpEvent, &liSize); lpEvent;
             liAction = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liAction)
            {
            case E_ACTION_SET_TAKEDOWN_CAMERA_STATE:
                mbClearUpEnabled = !reinterpret_cast<const SetTakedownCameraAction*>(lpEvent)->mbActive;
                break;
            case E_ACTION_RESET_CRASHING:
                ForceClearupAllCrashes(lpOutput);
                break;
            case E_ACTION_RESET_RACE_CAR_CRASHING:
            {
                const u32 luCrash = FindCrashForRaceCar(
                    reinterpret_cast<const ResetRaceCarCrashingAction*>(lpEvent)->meActiveRaceCarIndex);
                if (luCrash != KU_INVALID_CRASH)
                    ResetRaceCarFromCrashIndex(lpOutput, luCrash, false);
                break;
            }
            case E_ACTION_REMOTE_PLAYER_DISCONNECTED:
                if (mbIsOnlineGameMode)
                    OnNetworkPlayerDisconnected(
                        reinterpret_cast<const RemotePlayerDisconnectedAction*>(lpEvent)->meActiveRaceCarIndex, lpOutput);
                break;
            case E_ACTION_PREPARE_FOR_MODE:
            {
                const PrepareForModeAction* lpAction = reinterpret_cast<const PrepareForModeAction*>(lpEvent);
                if (!lpAction->IsFirstPrepareForMode())
                    break;
                const BrnGameState::GameModeParams& lrParams = *lpAction->GetGameModeParams();
                const EGameModeType leMode = lrParams.GetGameModeType();
                mbIsOnlineGameMode = lrParams.mbIsOnline;
                mbIsShowtimeGameMode = leMode == E_MODE_OFFLINE_SHOWTIME || leMode == E_MODE_ONLINE_SHOWTIME;
                mbFastCrashesForAI = lrParams.GetFlag(BrnGameState::GameModeParams::KU_FLAG_RAPID_CRASHES);
                mbIsInAGameMode = true;
                if (!(lpAction->IsMovingBetweenOnlineLobbyModes()
                      && (leMode == E_MODE_ONLINE_FREE_BURN_LOBBY || leMode == E_MODE_ONLINE_SHOWTIME)))
                    meLocalActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
                mbClearUpEnabled = !lrParams.GetFlag(BrnGameState::GameModeParams::KU_FLAG_DISABLE_CRASH_CLEAN_UP);
                if (mbClearUpEnabled)
                    ForceClearupAllCrashes(lpOutput);
                if (lrParams.GetFlag(BrnGameState::GameModeParams::KU_FLAG_DISABLE_CRASH_EXTENSIONS))
                    miNumCrashExtensions = 0;
                else if (lrParams.GetFlag(BrnGameState::GameModeParams::KU_FLAG_LIMITED_CRASH_EXTENSIONS))
                    miNumCrashExtensions = 3;
                else
                    miNumCrashExtensions = 10;
                mfPlayerCrashTime = lrParams.GetFlag(BrnGameState::GameModeParams::KU_FLAG_SHORT_CRASH_TIME) ? 3.0f : 4.0f;
                break;
            }
            case E_ACTION_STOP_MODE:
                if (!reinterpret_cast<const StopModeAction*>(lpEvent)->mu8Field14)
                {
                    mfPlayerCrashTime = 4.0f;
                    mbClearUpEnabled = true;
                    mbIsInAGameMode = false;
                    miNumCrashExtensions = 10;
                    ForceClearupAllCrashes(lpOutput);
                }
                mbIsOnlineGameMode = false;
                mbIsShowtimeGameMode = false;
                break;
            case E_ACTION_ROAD_RAGE_PLAYER_DAMAGE:
                if (reinterpret_cast<const RoadRagePlayerDamageAction*>(lpEvent)->mbPlayerTotalled && !mbIsOnlineGameMode)
                    for (u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength(); ++luCrash)
                        mRaceCarCrashes.GetItem(luCrash).SetSecondsBeforeCleanup(60.0f);
                break;
            default:
                continue;
            }
            // Opt-in witness of the live game-action queue and resulting crash policy.
            static u32 luTraceCount = 0;
        }
    }

    // ARTIST 0x827CDD28: consume all race-car records; publish traffic cleanup
    // before clearing each vehicle's ownership bookkeeping and finally its array.
    void CrashModule::ForceClearupAllCrashes(CrashIO::OutputBuffer_PreScene* lpOutput)
    {
        CGS_ASSERT(lpOutput, "lpOutput");
        while (mRaceCarCrashes.GetLength() > 0)
            ResetRaceCarFromCrashIndex(lpOutput, 0, false);
        for (u32 luCrash = 0; luCrash < mTrafficCrashes.GetLength(); ++luCrash)
        {
            const TrafficCrash& lrCrash = mTrafficCrashes.GetItem(luCrash);
            const u32 luVehicle = lrCrash.GetVehicleIndex();
            CGS_ASSERT(luVehicle < 0x4000u, "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
            CrashIO::CleanupTrafficEvent lEvent;
            lEvent.mVolumeInstanceId.muId = static_cast<u64>(0x02000000u | (luVehicle << 10)) << 32;
            lpOutput->GetTrafficOutputInterface()->GetCleanupTrafficEventQueue().AddEvent(lEvent);
            OnTrafficCarRemovedFromCrash(luVehicle, lrCrash.GetOwner());
        }
        mTrafficCrashes.Clear();
    }

    // ARTIST 0x827CCEE8. EraseFast moves another record into the current slot.
    void CrashModule::OnNetworkPlayerDisconnected(EActiveRaceCarIndex lePlayer,
                                                  CrashIO::OutputBuffer_PreScene* lpOutput)
    {
        for (u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength();)
        {
            if (mRaceCarCrashes.GetItem(luCrash).GetOwner() == lePlayer)
                ResetRaceCarFromCrashIndex(lpOutput, luCrash, true);
            else
                ++luCrash;
        }
        for (u32 luCrash = 0; luCrash < mTrafficCrashes.GetLength(); ++luCrash)
            if (mTrafficCrashes.GetItem(luCrash).GetOwner() == lePlayer)
                mTrafficCrashes.GetItem(luCrash).OnOwnerDisconnected();
    }

    // ARTIST 0x827C67E0: remove the vehicle from both bitsets and the owner's set.
    void CrashModule::OnTrafficCarRemovedFromCrash(u32 luVehicleIndex, EActiveRaceCarIndex leOwner)
    {
        CGS_ASSERT(luVehicleIndex < 600, "Index is out of range (max bits: 600)");
        mCrashingTraffic.UnSetBit(luVehicleIndex);
        CGS_ASSERT(luVehicleIndex < 600, "Index is out of range (max bits: 600)");
        mCrashingNetworkTraffic.UnSetBit(luVehicleIndex);
        maiSlammedTrafficOwners[luVehicleIndex] = -1;
        const u16 luVehicle = static_cast<u16>(luVehicleIndex);
        if (leOwner == static_cast<EActiveRaceCarIndex>(-1))
        {
            for (s32 liPlayer = 0; liPlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liPlayer)
                if (maCrashingTrafficForPlayers[liPlayer].Find(luVehicle) != CgsContainers::Set<u16, 160>::KU_INVALID)
                {
                    maCrashingTrafficForPlayers[liPlayer].Erase(luVehicle);
                    break;
                }
        }
        else if (maCrashingTrafficForPlayers[leOwner].Find(luVehicle) != CgsContainers::Set<u16, 160>::KU_INVALID)
            maCrashingTrafficForPlayers[leOwner].Erase(luVehicle);
    }

    // 0x827DE960. The X360 ctor stores the base ModuleSingleBuffered vtable, constructs the two
    // base RWMutexes (mInputMutex @ +0x10, mOutputMutex @ +0x118), re-points the vtable to the
    // derived CrashModule slot, then stores -1 into the count word of each crash array
    // (mRaceCarCrashes @ +0x2F0, mTrafficCrashes @ +0x7F8) -- i.e. marks both arrays as
    // not-yet-constructed (the "Array used before Construct/Clear" sentinel). The base sub-object
    // init and vtable stores are emitted by the compiler-generated base ctor + vtable setup; the
    // CrashModule ctor body is just the two MarkUnconstructed() stores.
    CrashModule::CrashModule()
    {
        mRaceCarCrashes.MarkUnconstructed();
        mTrafficCrashes.MarkUnconstructed();
    }

    // 0x827C6AD8. Walk every live RaceCarCrash record (mRaceCarCrashes, count @ +0xC0) and return
    // the index of the one whose crashing-race-car volume-instance entity index matches the given
    // active-race-car slot; KU_INVALID_CRASH (-1) when none matches. The GetItem accessor asserts
    // the array was constructed; the per-element read asserts the embedded entity index is a valid
    // active-race-car slot (< E_ACTIVE_RACE_CAR_INDEX_COUNT == 8).
    u32 CrashModule::FindCrashForRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        const u32 luCount = mRaceCarCrashes.GetLength();
        for (u32 luCrash = 0; luCrash < luCount; ++luCrash)
        {
            // The X360 inlines RaceCarCrash::GetOwner here: read the crashing-race-car volume
            // instance entity index and assert it is a valid active-race-car slot (< 8).
            const RaceCarCrash& lrCrash = mRaceCarCrashes.GetItem(luCrash);
            if (lrCrash.GetOwner() == static_cast<s32>(leActiveRaceCarIndex))
            {
                return luCrash;
            }
        }

        return KU_INVALID_CRASH;
    }

    // 0x827BBB10. True when the traffic vehicle luVehicle is queued in mRecycledTrafficQueue (the
    // vehicle-manager removed-traffic events mirrored for this frame), i.e. it will be recycled on
    // the next frame. Each queued TrafficRemovedEvent carries the removed vehicle's packed
    // EntityId; the X360 extracts its 14-bit Burnout entity index ((word >> 10) & 0x3FFF) and
    // compares it to luVehicle. Asserts luVehicle is a valid traffic index (< KU_MAX_TOTAL_TRAFFIC
    // == 600 == 0x258).
    bool CrashModule::WillTrafficVehicleBeRecycledNextFrame(u16 luVehicle)
    {
        CGS_ASSERT_AT(luVehicle < 0x258u, "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC", CGS_ARTIST_BRNCRASHMODULE_H_FILE, 467);

        const s32 liCount = mRecycledTrafficQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const BrnPhysics::Vehicle::TrafficRemovedEvent& lrEvent = mRecycledTrafficQueue.GetEvent(liEvent);
            const u32 luEventVehicleIndex = (lrEvent.mRemovedVehicleEntityId.muValue >> 10) & 0x3FFFu;
            if (luEventVehicleIndex == static_cast<u32>(luVehicle))
            {
                return true;
            }
        }

        return false;
    }
}

// FOLDED FROM BrnCrashModule_Lifecycle.cpp
// =================================================================================================
// GameSource/World/CrashModule/BrnCrashModule_Lifecycle.cpp   (crash exit wave, 2026-08-25)
//
// The four lifecycle members of BrnWorld::CrashModule, from the X360 ARTIST spine:
//   CrashModule::Construct @0x827CAA28   (24 insns)
//   CrashModule::Prepare   @0x827B16D8   (41 insns)
//   CrashModule::Release   @0x827B1780   (41 insns)
//   CrashModule::Reset     @0x827BF318   (54 insns)
//
// DWARF home is World/CrashModule/BrnCrashModule.cpp (the assert strings in all four name it).
// They live in their own file for the same reason WorldBridgeCrashPostScene.cpp does: the home TU
// still carries ~20 unreconstructed traffic/network helpers, and these four can land without
// waiting on them. DELETE-WHEN the home TU becomes mountable whole.
//
// =================================================================================================
// ⭐⭐ WHY THIS FILE IS THE FIRST THING THE CRASH EXIT NEEDED, AND WHY NOBODY NOTICED
// =================================================================================================
// BrnCrashModule.h declared THREE methods and no lifecycle. `mCrashModule.Construct()` at
// BrnWorldModule.cpp:505 therefore bound to the BASE CgsModule::ModuleSingleBuffered::Construct,
// which knows nothing about crash tunables. Every one of them stayed at whatever the pool held:
//
//     mbClearUpEnabled      = 0     <- and TickCrashes' FIRST assert is `mbClearUpEnabled`,
//                                      while CrashModule::PreSceneUpdate GATES both TickCrashes
//                                      and ClearupCrashes on it. ⇒ THE COUNTDOWN COULD NEVER RUN.
//     mfPlayerCrashTime     = 0.0f  <- every player crash would have been created already expired
//     miNumCrashExtensions  = 0     <- no wreck could ever earn its slide-out extension
//     meLocalActiveRaceCarIndex = 0 <- not the -1 "no player yet" sentinel
//     both crash Arrays        unconstructed (the ctor's MarkUnconstructed, never Reset)
//
// So the campaign could have landed all ~1900 instructions of ProcessCrashedRaceCarEvents /
// TickCrashes / RaceCarCrash::Tick / ClearupCrashes and a crash STILL would not have ended --
// silently, with no assert firing, because the module simply never entered the arm that ticks.
// This is the [[hollow-shell-classes]] shape one level up: not "a class that declares none of its
// virtuals" but a MODULE THAT DECLARES NONE OF ITS LIFECYCLE, so every lifecycle call lands on a
// base default that does nothing for it. The tell was available all along -- the boot gate at
// BrnWorldModule.cpp:1163 says in as many words "the call resolves to the BASE
// ModuleSingleBuffered::Prepare" -- and it was read as a reason to SKIP rather than as the defect.
//
// ⭐ The Prepare gate's stated reason for skipping is now measured and it was HALF right. The base
// Prepare does assert on the data-structure path -- but every arm of it is guarded by
// `if (!mbIsNewModule)`, and the LAST thing CrashModule::Construct does (0x827CAA80 `stw r11(1),
// 4(r3)`) is set mbIsNewModule. The assert was only reachable BECAUSE the derived Construct was
// missing. Landing Construct is what makes Prepare safe; skipping Prepare could never have.
// =================================================================================================

#include "GameSource/World/CrashModule/BrnCrashModule.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT + MessageAt
#include "GameSource/BurnoutConstants.h"             // E_ACTIVE_RACE_CAR_INDEX_*

#include <cstring>                                   // memset

namespace BrnWorld
{

// =================================================================================================
// Construct @ 0x827CAA28   (24 insns)
//
//   0x827CAA34  bl  ModuleSingleBuffered::Construct
//   0x827CAA3C  stw r11(2),  0x22C(this)   -- meReleaseStage = E_RELEASESTAGE_DONE
//   0x827CAA44  stfs f0,     0x1524(this)  -- mfPlayerCrashTime = 4.0f
//   0x827CAA48  stw r11(0),  0x228(this)   -- mePrepareStage = E_PREPARESTAGE_START
//   0x827CAA4C  stb r11(0),  0x1529(this)  -- mbIsOnlineGameMode   = false
//   0x827CAA50  stb r11(0),  0x152A(this)  -- mbIsShowtimeGameMode = false
//   0x827CAA54  stb r11(0),  0x152F(this)  -- mbIsInAGameMode      = false
//   0x827CAA58  stb r11(1),  0x152B(this)  -- mbClearUpEnabled     = TRUE
//   0x827CAA5C  stb r11(10), 0x152C(this)  -- miNumCrashExtensions = 10
//   0x827CAA60  stb r11(0),  0x1528(this)  -- mbFastCrashesForAI   = false
//   0x827CAA64  stb r11(0),  0x152E(this)  -- mbNeedToSendEndingMessage = false
//   0x827CAA68  stw r11(-1), 0x1520(this)  -- meLocalActiveRaceCarIndex = INVALID
//   0x827CAA70  bl  EventQueue<TrafficRemovedEvent,25>::Construct(this + 0x1530)
//   0x827CAA78  bl  CrashModule::Reset
//   0x827CAA80  stw r11(1),  4(this)       -- Module::mbIsNewModule = true
//
// The 4.0f is the console's own default player crash duration; CrashModule::HandleGameActions'
// case 39 (GAME MODE STOP) restores exactly this same set, which is what makes free burn -- where
// no game mode ever starts -- run on these values for the whole session.
// =================================================================================================
void CrashModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    meReleaseStage            = E_RELEASESTAGE_DONE;
    mfPlayerCrashTime         = 4.0f;
    mePrepareStage            = E_PREPARESTAGE_START;
    mbIsOnlineGameMode        = false;
    mbIsShowtimeGameMode      = false;
    mbIsInAGameMode           = false;
    mbClearUpEnabled          = true;
    miNumCrashExtensions      = 10;
    mbFastCrashesForAI        = false;
    mbNeedToSendEndingMessage = false;
    meLocalActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

    mRecycledTrafficQueue.Construct();

    Reset();

    // 0x827CAA80. The crash module is a "new module": it owns no DataStructure pair, so every
    // buffer/data-structure arm of the base Prepare/Release must be skipped. See the file banner.
    mbIsNewModule = true;
}

// =================================================================================================
// Prepare @ 0x827B16D8   (41 insns)
//
//   0x827B16F4  lwz    r11, 0x228(this)     -- mePrepareStage
//   0x827B16F8  cmplwi r11, 1 ; blt/beq     -- stage 0 or 1 -> run the manager step
//   0x827B1704  cmplwi r11, 3 ; blt         -- stage 2      -> already done, fall to the tail
//   0x827B170C  else FireAssert("Invalid Stage\n", BrnCrashModule.cpp:457) and return false
//   0x827B1738  stw    r30(1), 0x228(this)  -- mePrepareStage = E_PREPARESTAGE_MANAGER
//   0x827B173C  bl     ModuleSingleBuffered::Prepare ; return false if it is not finished
//   0x827B1758  stw    r30(1), 0x228(this)  -- mePrepareStage = E_PREPARESTAGE_MANAGER (again)
//   0x827B1760  stw    r11(0), 0x22C(this)  -- meReleaseStage = E_RELEASESTAGE_START
//   0x827B175C  li     r3, 1                -- return true
//
// ⚠️ TRANSCRIBED AS WRITTEN, INCLUDING THE ODD BIT: the tail stores MANAGER(1) into
// mePrepareStage, NOT DONE(2), even though DONE is what the stage test above accepts as
// "already prepared". `stw r30` with `li r30, 1` at 0x827B16F0 is unambiguous -- it is a 1. The
// consequence on the console is that a second Prepare() re-enters the manager step, which is
// harmless because the base Prepare is itself staged and returns true immediately once its own
// stage is DONE. Do NOT "fix" this to E_PREPARESTAGE_DONE: it is the console's behaviour, and the
// resume path depends on the base's stage, not this one. (Release @0x827B1780 is the exact mirror,
// stage word 0x22C, assert line 508.)
// =================================================================================================
bool CrashModule::Prepare()
{
    // The modules' resumable-stage switch (LegacySrc BrnTriggerEntityModule.cpp shape): a failed
    // base Prepare breaks out to the shared `return false`, DONE owns the success tail.
    switch (mePrepareStage)
    {
        case E_PREPARESTAGE_START:
        {
        }
        // fall through
        case E_PREPARESTAGE_MANAGER:
        {
            mePrepareStage = E_PREPARESTAGE_MANAGER;

            if (!CgsModule::ModuleSingleBuffered::Prepare())
            {
                break;
            }
        }
        // fall through
        case E_PREPARESTAGE_DONE:
        {
            mePrepareStage = E_PREPARESTAGE_MANAGER;   // see the banner -- 1, not 2, on purpose
            meReleaseStage = E_RELEASESTAGE_START;
            return true;
        }
        break;

        default:
        {
            CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("Invalid Stage\n", __FILE__, 457));   // BrnCrashModule.cpp:457
        }
    }
    return false;
}

// =================================================================================================
// Release @ 0x827B1780   (41 insns). The exact mirror of Prepare over meReleaseStage (0x22C),
// with the base ModuleSingleBuffered::Release and assert line BrnCrashModule.cpp:508.
// =================================================================================================
bool CrashModule::Release()
{
    // The modules' resumable-stage switch (LegacySrc BrnTriggerEntityModule.cpp shape): a failed
    // base Release breaks out to the shared `return false`, DONE owns the success tail.
    switch (meReleaseStage)
    {
        case E_RELEASESTAGE_START:
        {
        }
        // fall through
        case E_RELEASESTAGE_MANAGER:
        {
            meReleaseStage = E_RELEASESTAGE_MANAGER;

            if (!CgsModule::ModuleSingleBuffered::Release())
            {
                break;
            }
        }
        // fall through
        case E_RELEASESTAGE_DONE:
        {
            meReleaseStage = E_RELEASESTAGE_MANAGER;
            mePrepareStage = E_PREPARESTAGE_START;
            return true;
        }
        break;

        default:
        {
            CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("Invalid Stage\n", __FILE__, 508));   // BrnCrashModule.cpp:508
        }
    }
    return false;
}

// =================================================================================================
// Reset @ 0x827BF318   (54 insns). Drop every tracked crash and every per-vehicle crash bit.
//
//   0x827BF32C  stw 0, 0x2F0(this)          -- mRaceCarCrashes.Clear()   (the count word)
//   0x827BF330  stw 0, 0x7F8(this)          -- mTrafficCrashes.Clear()
//   0x827BF334+ 21 x `std 0` over +0x800..+0x8A0 -- the three FastBitArrays, zeroed
//   0x827BF38C  bl  memset(this + 0x8A8, -1, 600)  -- maiSlammedTrafficOwners = "no owner"
//   0x827BF394  stw 0, 0x1538(this)         -- mRecycledTrafficQueue.Clear()  (miLength)
//   0x827BF3A0- the 8-iteration loop at stride 324 from +0xC40 -- maCrashingTrafficForPlayers[i]
//               .Clear(), carrying BurnoutConstants.h:39's own
//               "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" tripwire from the ++ operator
//
// ⚠️ Written BY NAME, not by offset. The console offsets above are documentation: this build's
// FastBitArray/Set/EventQueue sub-objects widen, and only the member ORDER is load-bearing
// ([[reconstruction-gotchas]] -- parity by named members).
// ⚠️ The memset length really is 600 (KU_MAX_TOTAL_TRAFFIC) while the array is [601]; the last
// byte is left alone. Faithful, and it is the console's own off-by-one, not a transcription slip.
// =================================================================================================
void CrashModule::Reset()
{
    mRaceCarCrashes.Clear();
    mTrafficCrashes.Clear();

    // FastBitArray spells "zero every field" UnSetAll(); the console's 21 `std 0` stores over
    // +0x800..+0x8A0 are exactly the three arrays' bit-field words.
    mCrashingRaceCars.UnSetAll();
    mCrashingTraffic.UnSetAll();
    mCrashingNetworkTraffic.UnSetAll();

    memset(maiSlammedTrafficOwners, -1, 600);

    mRecycledTrafficQueue.Clear();

    for (s32 liPlayer = 0; liPlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liPlayer)
    {
        maCrashingTrafficForPlayers[liPlayer].Clear();
    }
}

}   // namespace BrnWorld

// FOLDED FROM BrnCrashModule_RaceCarCrashes.cpp
// =================================================================================================
// GameSource/World/CrashModule/BrnCrashModule_RaceCarCrashes.cpp   (crash exit wave, 2026-08-25)
//
// ⭐⭐⭐ THIS IS THE FILE THAT MAKES A CRASH END. Everything upstream of it has worked for weeks --
// driving into traffic fires SetRaceCarCrashing, the physics module publishes a RaceCarCrashEvent,
// and the world bridges carry it. Nothing consumed it, so the car froze at the impact point
// forever. These six bodies are the consumer: they open a crash RECORD, run its countdown, and
// then say "this one is over" into the ring the race-car module already reads.
//
// X360 ARTIST spine:
//   CrashModule::PreSceneUpdate            @0x827D3A60   (86 insns)
//   CrashModule::PostPhysicsUpdate         @0x827D3BB8   (83 insns)
//   CrashModule::ProcessCrashedRaceCarEvents @0x827CAAB8 (370 insns)  -- allocates the record
//   CrashModule::TickCrashes               @0x827C6490  (212 insns)  -- runs the countdown
//   CrashModule::ClearupCrashes            @0x827CDE98  (475 insns)  -- retires expired records
//   CrashModule::ResetRaceCarFromCrashIndex@0x827C6C40  (133 insns)  -- POSTS the complete event
//
// DWARF home is World/CrashModule/BrnCrashModule.cpp; same file-split rationale as
// BrnCrashModule_Lifecycle.cpp. DELETE-WHEN the home TU becomes mountable whole.
//
// Traffic producers, ownership, countdown and cleanup now run through the original phases.
// The network race-car reset (ResetCrashedNetworkRaceCars + OnContactFromNetworkPlayer), the
// owned-traffic publisher (GenerateOwnedTrafficUpdates) and its receive twin
// (HandleNetworkCrashingTraffic, G64-D2) run under the online gate; none of the online arms is
// certified by the offline lifecycle pass.

#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include <cstdlib>
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"              // IOBuffer lock/unlock
#include "GameSource/BurnoutConstants.h"                            // E_ACTIVE_RACE_CAR_INDEX_*
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"   // BrnNetworkDriverControls, E_DRIVER_TYPE_NETWORK
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h" // BrnTraffic::MakeTrafficVolumeInstanceId
namespace BrnWorld
{

namespace
{
    // BrnCrashModule.cpp:53 (DWARF). A dynamically initialised .bss float on the console: CRT thunk
    // 0x82C6AC28 sums 0x82065B70 (1.0f) + 0x82065B6C (3.0f) + 0x82065B68 (10.0f) + 0x82001C98 (1.0f)
    // and stfs's the result into 0x8300E9B0 at 0x82C6AC58. Read at 0x827CAEE0.
    const f32 KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS = 1.0f + 3.0f + 10.0f + 1.0f;   // == 15.0f

    // BrnCrashModule.cpp:35 (DWARF): the network wreck timeout, flt_820CA5A8 == 20.0f (.rdata).
    // Refreshed by RaceCarCrash / TrafficCrash::ResetNetworkTimeout below.
    const f32 KF_NETWORK_CRASH_TIMEOUT = 20.0f;

    const s32 KI_NETCRASH_DIAG_MAX_LINES = 40;

}

// =================================================================================================
// ProcessCrashedRaceCarEvents @ 0x827CAAB8   (370 insns)
//
// Drain the physics module's RaceCarCrashEvent ring and open one RaceCarCrash record per newly
// crashing race car. This is where a crash becomes a TRACKED crash.
//
//   0x827CAC7C  bl  EventQueue<RaceCarCrashEvent,8>::GetEvent(queue, i)
//   0x827CAC84  ld  r11, 0(event) ; srdi ; rlwinm 8,24,31 -- owner byte of the volume instance id,
//               asserted == E_ENTITYTYPE_RACECAR (BrnCrashModule.cpp:893)
//   0x827CAC98  extrwi r28, r10, 14, 8                    -- the active-race-car slot
//               asserted >= 0 (:894) and < 8 (:895, with the FastBitArray range message)
//   0x827CAE18  the mCrashingRaceCars bit test -- ALREADY-CRASHING CARS ARE SKIPPED ENTIRELY
//   0x827CAE6C  CGS_ASSERT(FindCrashForRaceCar(idx) == KU_INVALID_CRASH)   (:900)
//   0x827CAE90  the crash-duration select (see below)
//   0x827CAF04  bl  Array<RaceCarCrash,8>::Grow ; bl RaceCarCrash::Construct(id, seconds)
//   0x827CB038  set this slot's bit in mCrashingRaceCars
//
// THE DURATION SELECT -- every constant READ FROM THE IMAGE, none guessed:
//   event.mbCarIsAI      -> mbFastCrashesForAI ? 2.0f (0x82001D9C) : 4.5f (0x820CA5B4)
//   event.mbCarIsNetwork -> 20.0f (0x820CA5A8)
//   mbIsShowtimeGameMode -> KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS (flt_8300E9B0 == 15.0f, below)
//   mbIsOnlineGameMode   -> 5.0f (0x8200426C)
//   otherwise            -> mfPlayerCrashTime  (Construct: 4.0f)   <-- THE PLAYER'S PATH
// flt_8300E9B0 is .bss (it reads 0.0 from the image by definition); its only writer is the CRT
// dyn-init thunk 0x82C6AC28 (init-table entry 0x82CD29B4), which sums 1.0f (0x82065B70) + 3.0f
// (0x82065B6C) + 10.0f (0x82065B68) + 1.0f (0x82001C98) and stores 15.0f with `stfs f0,-0x1650(r11)`
// at 0x82C6AC58 (findinit: that is the only store). DWARF: KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS,
// BrnCrashModule.cpp:53 -- the thunk right before KVF_DIST_TO_ALLOW_CLEANUP_BEHIND's (:1773,
// 0x82C6AC60). The arm is LIVE: HandleGameActions sets mbIsShowtimeGameMode for modes 2/16
// (console 0x827D0CFC). (G64-D1: this arm used to store the .bss image value 0.0f.) The value is
// only ever consumed by TickCrashes/ClearupCrashes, i.e. while mbClearUpEnabled is set, which
// Showtime's KU_FLAG_DISABLE_CRASH_CLEAN_UP clears for the mode's own duration.
// =================================================================================================
void CrashModule::ProcessCrashedRaceCarEvents( const CrashIO::InputBuffer_PostPhysics* lpInput,
                                               CrashIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT( lpInput  != 0, "lpInput" );    // BrnCrashModule.cpp:877
    CGS_ASSERT( lpOutput != 0, "lpOutput" );   // BrnCrashModule.cpp:878

    const BrnPhysics::Vehicle::VehicleManagerOutputInterface* lpVehicleManagerOutputInterface =
        lpInput->GetVehicleManagerOutputInterface();
    const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleInterface =
        lpInput->GetVehicleOutputInterface();

    CGS_ASSERT(lpVehicleManagerOutputInterface != 0, CGS_ASSERT_MESSAGE_AT("lpVehicleManagerOutputInterface", __FILE__, 882));   // :882
    CGS_ASSERT(lpVehicleInterface != 0, CGS_ASSERT_MESSAGE_AT("lpVehicleInterface", __FILE__, 883));                             // :883

    const BrnPhysics::Vehicle::VehicleManagerOutputInterface::RaceCarCrashEventQueue*
        lpRaceCarCrashEvents = lpVehicleManagerOutputInterface->GetRaceCarCrashEventQueue();
    CGS_ASSERT(lpRaceCarCrashEvents != 0, CGS_ASSERT_MESSAGE_AT("lpRaceCarCrashEvents", __FILE__, 886));                         // :886

    const s32 liEventCount = lpRaceCarCrashEvents->GetLength();
    for( s32 liEvent = 0; liEvent < liEventCount; ++liEvent )
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lrEvent =
            lpRaceCarCrashEvents->GetEvent( liEvent );

        CGS_ASSERT( lrEvent.mRaceCarVolumeInstanceID.GetEntityIDOwner() == 1u,
                    "lEvent.mRaceCarVolumeInstanceID.GetEntityIDOwner() == E_ENTITYTYPE_RACECAR" );

        const u32 luActiveRaceCarIndex = lrEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex();
        CGS_ASSERT( luActiveRaceCarIndex < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );
        if( luActiveRaceCarIndex >= static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ) )
        {
            continue;   // the console's FastBitArray guard would fire below; do not corrupt memory
        }

        // 0x827CAE18..0x827CAE5C -- a car already tracked as crashing does NOT get a second record.
        if( mCrashingRaceCars.IsBitSet( luActiveRaceCarIndex ) )
        {
            continue;
        }

        CGS_ASSERT( FindCrashForRaceCar( static_cast<EActiveRaceCarIndex>( luActiveRaceCarIndex ) )
                        == KU_INVALID_CRASH,
                    "FindCrashForRaceCar( leActiveRaceCarIndex ) == KU_INVALID_CRASH" );   // :900

        f32 lfSecondsBeforeCleanup;
        if( lrEvent.mbCarIsAI )
        {
            lfSecondsBeforeCleanup = mbFastCrashesForAI ? 2.0f : 4.5f;
        }
        else if( lrEvent.mbCarIsNetwork )
        {
            lfSecondsBeforeCleanup = 20.0f;
        }
        else if( mbIsShowtimeGameMode )
        {
            // 0x827CAED0 `lbz 0x152A` ; 0x827CAEE0 `lfs f31, -0x1650(0x8301 << 16)` == flt_8300E9B0.
            lfSecondsBeforeCleanup = KF_PLAYER_SHOWTIME_CAR_RESET_SECONDS;
        }
        else if( mbIsOnlineGameMode )
        {
            lfSecondsBeforeCleanup = 5.0f;
        }
        else
        {
            lfSecondsBeforeCleanup = mfPlayerCrashTime;
        }

        RaceCarCrash* lpCrash = mRaceCarCrashes.Grow();
        lpCrash->Construct( lrEvent.mRaceCarVolumeInstanceID, lfSecondsBeforeCleanup );

        mCrashingRaceCars.SetBit( luActiveRaceCarIndex );

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "[crash-exit] OPENED crash record for active race car "
                                       << static_cast<s32>( luActiveRaceCarIndex )
                                       << " seconds=" << lfSecondsBeforeCleanup << "\n";
        }
    }
}

// =================================================================================================
// TickCrashes @ 0x827C6490   (212 insns)
//
//   0x827C64F8  CGS_ASSERT(lpInput)          (:1687)
//   0x827C6510  CGS_ASSERT(mbClearUpEnabled) (:1688)   <- the flag CrashModule::Construct sets
//   0x827C6528  lpInput->GetTimerStatusInterface(): the step is `[8] * [7]` == the SIM status'
//               mfTimeStepMultiplier * mfBaseTimeStep == GetSimTimerStatus()->GetCurrentTimeStep()
//   0x827C6690  `addi r11, owner, 0x13C0 ; slwi 1 ; lbzx ; clrlwi 31` -- the HIGH byte of
//               maxRaceCarFlags[owner], bit 0 of that byte == bit 8 of the word ==
//               E_RACE_CAR_OUTPUT_FLAG_IN_SHOWTIME. ⭐ A SHOWTIME WRECK IS NOT TICKED AT ALL, and
//               if it is the player's, the pending ending message is RETRACTED.
//   0x827C66A8  r6 = lpInput->mbPlayerPressingBoost && !mbIsShowtimeGameMode
//   0x827C6714  bl RaceCarCrash::Tick(...)   -- see BrnRaceCarCrash.cpp for the 8-argument map
//   then the TrafficCrash tail loop (kept: it needs no parked helper).
// =================================================================================================
void CrashModule::TickCrashes( const CrashIO::InputBuffer_PreScene* lpInput )
{
    CGS_ASSERT(lpInput != 0, CGS_ASSERT_MESSAGE_AT("lpInput", __FILE__, 1687));                    // :1687
    CGS_ASSERT(mbClearUpEnabled, CGS_ASSERT_MESSAGE_AT("mbClearUpEnabled", __FILE__, 1688));       // :1688

    const f32 lfTimeStep =
        lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep();

    const bool lbIsOfflineGameMode = !mbIsOnlineGameMode;

    const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
        lpInput->GetActiveRaceCarInterface();

    for( u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength(); ++luCrash )
    {
        const EActiveRaceCarIndex leOwner = static_cast<EActiveRaceCarIndex>(
            mRaceCarCrashes.GetItem( luCrash ).GetOwner() );

        const bool lbIsPlayerCrash =
            ( lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() == leOwner );

        if( lpActiveRaceCarInterface->IsCarInShowtime( leOwner ) )
        {
            // 0x827C6728 -- a showtime wreck never times out; retract any pending ending message.
            if( lbIsPlayerCrash )
            {
                mbNeedToSendEndingMessage = false;
            }
            continue;
        }

        const bool lbPlayerPressingBoostOutsideShowtime =
            lpInput->GetPlayerPressingBoost() && !mbIsShowtimeGameMode;

        mRaceCarCrashes.GetItem( luCrash ).Tick( lfTimeStep,
                                                 lpActiveRaceCarInterface,
                                                 lbPlayerPressingBoostOutsideShowtime,
                                                 static_cast<s32>( miNumCrashExtensions ),
                                                 lbIsOfflineGameMode,
                                                 lbIsPlayerCrash,
                                                 mbIsInAGameMode,
                                                 &mbNeedToSendEndingMessage );
    }

    // ARTIST827C6748..827C67CC: the same simulation step ticks each traffic record.
    for (u32 luCrash = 0; luCrash < mTrafficCrashes.GetLength(); ++luCrash)
        mTrafficCrashes.GetItem(luCrash).Tick(lfTimeStep);

}

// =================================================================================================
// ClearupCrashes @ 0x827CDE98   (475 insns) -- the RACE-CAR arm
//
//   0x827CDF3C  the "is there a player at all" pair: mePlayerActiveRaceCarIndex != -1 AND the
//               interface's mbIsPlayerCarActive byte. ⚠️ THE WHOLE LOOP IS SKIPPED WITHOUT A
//               PLAYER -- wrecks are only retired while there is a player car in the world.
//   0x827CDF88  the loop over mRaceCarCrashes
//   0x827CDFB6  `lfs f, 0xC(item)` < 0.0f  -- EXPIRED (strictly negative, not <= 0)
//   0x827CDFC4  if (!mbIsInAGameMode) -> clean up
//               else if (!IsRaceCarRival(owner)) -> clean up
//               else if (mfSecondsBeforeCleanup <= -20.0f) -> clean up (the hard backstop)
//               else the two VMX tests: keep the wreck alive only while it is BOTH within a
//               40-unit radius AND ahead of the plane 5 units behind the player.
//   0x827CE0xx  ResetRaceCarFromCrashIndex(lpOutput, luCrash--, IsRaceCarNetwork(owner))
//
// Rival hold constants are initialized by ARTIST startup thunks, not zero defaults:
// 0x82C6AC60 splats 5.0 (8200426C) to 8300E9C0; 0x82C6AC88 splats 40.0
// (82004D0C) to 8300EA30; 0x82C6ACB0 squares it into 8300F3B0 (1600.0).
// The unnormalized forward projection must exceed -5.0, and distance squared
// must be strictly below 1600.0. The hold ends at the -20-second backstop.
// =================================================================================================
void CrashModule::ClearupCrashes( const CrashIO::InputBuffer_PreScene* lpInput,
                                  CrashIO::OutputBuffer_PreScene* lpOutput )
{
    CGS_ASSERT( lpInput  != 0, "lpInput" );
    CGS_ASSERT( lpOutput != 0, "lpOutput" );
    CGS_ASSERT( mbClearUpEnabled, "mbClearUpEnabled" );

    const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
        lpInput->GetActiveRaceCarInterface();

    if( lpActiveRaceCarInterface->IsPlayerCarActive() )
    {
        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
        const Vector3 lPlayerDirection = lpActiveRaceCarInterface->GetPlayerDirection();
        for( u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength(); ++luCrash )
        {
            RaceCarCrash& lrCrash = mRaceCarCrashes.GetItem( luCrash );
            if( lrCrash.GetSecondsBeforeCleanup() >= 0.0f )
            {
                continue;
            }

            bool lbClearUp = true;

            if( mbIsInAGameMode )
            {
                const EActiveRaceCarIndex leOwner =
                    static_cast<EActiveRaceCarIndex>( lrCrash.GetOwner() );

                if( lpActiveRaceCarInterface->IsRaceCarRival( leOwner ) &&
                    lrCrash.GetSecondsBeforeCleanup() > -20.0f )
                {
                    const Vector3 lSeparation =
                        lpActiveRaceCarInterface->GetRaceCarState(leOwner)->mTransform.Pos() - lPlayerPosition;
                    lbClearUp = !(rw::math::vpu::Dot(lSeparation, lSeparation) < 1600.0f &&
                                  rw::math::vpu::Dot(lSeparation, lPlayerDirection) > -5.0f);
                }
            }

            if( lbClearUp )
            {
                const u32 luOwnerIndex =
                    lrCrash.GetVolumeInstanceId().GetEntityIDEntityIndex();
                CGS_ASSERT( luOwnerIndex < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                            "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

                const bool lbIsNetwork = lpActiveRaceCarInterface->IsRaceCarNetwork(
                    static_cast<EActiveRaceCarIndex>( luOwnerIndex ) );

                ResetRaceCarFromCrashIndex( lpOutput, luCrash, lbIsNetwork );
                --luCrash;   // 0x827CE0xx `v29--` -- EraseFast swapped a new element into this slot
            }
        }
    }

    // ARTIST827CE208..827CE5DC: offline wrecks are retired only offscreen and far away.
    for (u32 i = 0; i < mTrafficCrashes.GetLength();)
    {
        TrafficCrash& crash = mTrafficCrashes.GetItem(i);
        if (!crash.IsAllowedToBeClearedUp()) { ++i; continue; }
        const u32 vehicle = crash.GetVehicleIndex();
        bool clear = mbIsOnlineGameMode;
        if (!clear)
        {
            CGS_ASSERT(vehicle < 600, "Index is out of range (max bits: 600)");
            if (!mTrafficRenderedLastFrame.IsBitSet(vehicle))
            {
                CGS_ASSERT(vehicle < 600, "Index is out of range (max bits: 600)");
                clear = mTrafficFarFromCameraLastFrame.IsBitSet(vehicle);
            }
        }
        if (clear)
        {
            CGS_ASSERT(crash.IsAllowedToBeClearedUp(), "lpTrafficCrash->IsAllowedToBeClearedUp()");
            const EActiveRaceCarIndex owner = crash.GetOwner();
            CGS_ASSERT(vehicle < 0x4000, "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
            CrashIO::CleanupTrafficEvent event;
            event.mVolumeInstanceId.muId = static_cast<u64>(0x02000000u | (vehicle << 10)) << 32;
            lpOutput->GetTrafficOutputInterface()->GetCleanupTrafficEventQueue().AddEvent(event);
            OnTrafficCarRemovedFromCrash(vehicle, owner);
            mTrafficCrashes.EraseFast(i);
        }
        else { crash.MarkVehicleAsOnscreen(); ++i; }
    }

}

// =================================================================================================
// ResetRaceCarFromCrashIndex @ 0x827C6C40   (133 insns)
//
// ⭐ THE PRODUCER OF THE CRASH EXIT. Three statements, and the middle one is the whole point.
//
//   0x827C6CB0  bl Array<RaceCarCrash,8>::GetItem(this + 0x230, luCrashIndex)
//   0x827C6CB8  ld r30, 0(item) ; std into the 16-byte stack record  -- the VolumeInstanceId
//   0x827C6CC4  bl OutputBuffer_PreScene::GetRaceCarOutputInterface()   (the WRITE-lock overload)
//   0x827C6CCC  stb a4, 8(record)                                       -- mbRemoveRaceCar
//   0x827C6CD0  bl EventQueue<RaceCarCrashCompleteEvent,10>::AddEvent   <-- "THIS CRASH IS OVER"
//   0x827C6D5C  clear this slot's bit in mCrashingRaceCars
//   0x827C6D6C  bl Array<RaceCarCrash,8>::EraseFast(this + 0x230, luCrashIndex)
//
// The event travels: OutputBuffer_PreScene::mRaceCarOutputInterface
//   -> WorldModule::BridgeCrashModuleToRaceCarModule_PostScene  (landed c3655e4a)
//   -> RaceCarEntityModuleIO::InputBuffer_PostScene::mCrashInterface
//   -> RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents
//   -> ActiveRaceCar::ResetAfterCrash / RaceCar::RequestResetOnTrack.
// =================================================================================================
void CrashModule::ResetRaceCarFromCrashIndex( CrashIO::OutputBuffer_PreScene* lpOutput,
                                              u32 luCrashIndex, bool lbRemoveRaceCar )
{
    CGS_ASSERT(lpOutput != 0, CGS_ASSERT_MESSAGE_AT("lpOutput", __FILE__, 2083));                                     // :2083
    CGS_ASSERT( luCrashIndex < mRaceCarCrashes.GetLength(),
                CGS_ASSERT_MESSAGE_AT("luCrashIndex < mRaceCarCrashes.GetLength()", __FILE__, 2084));                  // :2084

    const RaceCarCrash& lrCrash = mRaceCarCrashes.GetItem( luCrashIndex );

    CrashIO::RaceCarCrashCompleteEvent lEvent;
    lEvent.mRaceCarVolumeInstanceId = lrCrash.GetVolumeInstanceId();
    lEvent.mbRemoveRaceCar          = lbRemoveRaceCar;

    lpOutput->GetRaceCarOutputInterface()->GetRaceCarCrashCompleteEventQueue()->AddEvent( lEvent );

    const u32 luOwnerIndex = lEvent.mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();
    CGS_ASSERT( luOwnerIndex < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "[crash-exit] CRASH COMPLETE posted for active race car "
                                   << static_cast<s32>( luOwnerIndex )
                                   << " remove=" << ( lbRemoveRaceCar ? 1 : 0 ) << "\n";
    }

    mCrashingRaceCars.UnSetBit( luOwnerIndex );
    mRaceCarCrashes.EraseFast( luCrashIndex );
}

// =================================================================================================
// RaceCarCrash::ResetNetworkTimeout / TrafficCrash::ResetNetworkTimeout  (DWARF BrnCrashModule.h:78
// and :134, bodies BrnCrashModule.cpp:166 and :295). Neither has an out-of-line X360 symbol: both are
// inlined into OnContactFromNetworkPlayer below, which is where their instructions are quoted.
// =================================================================================================
void RaceCarCrash::ResetNetworkTimeout()
{
    mfSecondsBeforeCleanup = KF_NETWORK_CRASH_TIMEOUT;   // 0x827C63B4/B8 lfs flt_820CA5A8 ; stfs 0xC
}

void TrafficCrash::ResetNetworkTimeout()
{
    CGS_ASSERT( IsConfirmedNetwork(), "IsConfirmedNetwork()" );   // 0x827C644C..0x827C6470 (:297)
    mfTimeTillClearup = KF_NETWORK_CRASH_TIMEOUT;                 // 0x827C6474/78 stfs 4(item)
}

// =================================================================================================
// OnContactFromNetworkPlayer @ 0x827C62E8   (105 insns)   -- G64-D4 (crash parity 2026-09-23)
//
// A remote player touched something: every wreck that player owns gets its network timeout
// refreshed to KF_NETWORK_CRASH_TIMEOUT.
//   0x827C6328..0x827C63C0  over mRaceCarCrashes (+0x230): the inlined RaceCarCrash::GetOwner
//               (`extrwi 14,8` of the id's high word, :174 range tripwire), `cmpw` against
//               lePlayer, then RaceCarCrash::ResetNetworkTimeout (a plain stfs, no tripwire).
//   0x827C63D4..0x827C6480  over mTrafficCrashes (+0x2F8): owner s8 at +0 == lePlayer (`lbz ;
//               extsb ; cmpw`) AND flag bit 0x4 (IsConfirmedNetwork), then
//               TrafficCrash::ResetNetworkTimeout.
// Callers: HandleNetworkCrashingTraffic @0x827CCAF8 (G64-D2, below) and
// ResetCrashedNetworkRaceCars @0x827CE9BC below -- both online-only.
// =================================================================================================
void CrashModule::OnContactFromNetworkPlayer( EActiveRaceCarIndex lePlayer )
{
    for( u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength(); ++luCrash )
    {
        if( mRaceCarCrashes.GetItem( luCrash ).GetOwner() == lePlayer )
        {
            mRaceCarCrashes.GetItem( luCrash ).ResetNetworkTimeout();
        }
    }

    for( u32 luCrash = 0; luCrash < mTrafficCrashes.GetLength(); ++luCrash )
    {
        if( mTrafficCrashes.GetItem( luCrash ).GetOwner() == lePlayer &&
            mTrafficCrashes.GetItem( luCrash ).IsConfirmedNetwork() )
        {
            mTrafficCrashes.GetItem( luCrash ).ResetNetworkTimeout();
        }
    }
}

// =================================================================================================
// ResetCrashedNetworkRaceCars @ 0x827CE6E0   (193 insns)   -- G65-D1 (crash parity 2026-09-23)
//
// A remote player's driver record says its car is no longer crashing: end our local record of that
// crash (CRASH COMPLETE, no removal) and refresh its other wrecks' network timeout.
//   0x827CE700..0x827CE774  tripwires lpInput (:2032), lpOutput (:2033), IsOnlineGameMode() (:2034)
//   0x827CE77C  GetActiveRaceCarInterface (0x827BB480) -- asserted (:2038) and never read again
//   0x827CE788  GetVehicleDriverInterface (0x827BB3D8, +0x3CD0); the driver queue is its +0
//               (:2039 "lpDriverQueue")
//   0x827CE7E8  VariableEventQueue<5040,16>::GetFirstEvent ... 0x827CE9D0 GetNextEvent, while >= 0
//   0x827CE848  `cmpwi r3, 2` -- E_DRIVER_TYPE_NETWORK records only; lpNetworkDriver tripwire (:2049)
//   0x827CE870  `lbz 0xB9` -- BrnNetworkDriverControls::mbCrash set: still crashing, skip
//   0x827CE87C  `lwz 0` miVehicleID; 0x827CE928..0x827CE968 the inlined FastBitArray<8>::IsBitSet
//               on mCrashingRaceCars (+0x800), its range tripwire first (CgsFastBitArray.h:396)
//   0x827CE978  FindCrashForRaceCar, tripwire != KU_INVALID_CRASH (:2060, non-gating)
//   0x827CE9B0  ResetRaceCarFromCrashIndex(lpOutput, crash, `li r6, 0` -- not removed)
//   0x827CE9BC  OnContactFromNetworkPlayer(miVehicleID)
// =================================================================================================
void CrashModule::ResetCrashedNetworkRaceCars( const CrashIO::InputBuffer_PreScene* lpInput,
                                               CrashIO::OutputBuffer_PreScene* lpOutput )
{
    CGS_ASSERT(lpInput != 0, CGS_ASSERT_MESSAGE_AT("lpInput != NULL", __FILE__, 2032));             // :2032
    CGS_ASSERT(lpOutput != 0, CGS_ASSERT_MESSAGE_AT("lpOutput != NULL", __FILE__, 2033));           // :2033
    CGS_ASSERT(mbIsOnlineGameMode, CGS_ASSERT_MESSAGE_AT("IsOnlineGameMode()", __FILE__, 2034));    // :2034

    const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
        lpInput->GetActiveRaceCarInterface();
    const BrnPhysics::Vehicle::VehicleDriverInputInterface::UpdateDriverEventQueue* lpDriverQueue =
        lpInput->GetVehicleDriverInterface()->GetUpdateDriverQueue();

    CGS_ASSERT(lpActiveRaceCarInterface != 0, CGS_ASSERT_MESSAGE_AT("lpActiveRaceCarInterface", __FILE__, 2038));   // :2038
    CGS_ASSERT(lpDriverQueue != 0, CGS_ASSERT_MESSAGE_AT("lpDriverQueue", __FILE__, 2039));                         // :2039
    (void)lpActiveRaceCarInterface;

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for( s32 liDriverType = lpDriverQueue->GetFirstEvent( &lpEvent, &liSize );
         liDriverType >= 0;
         liDriverType = lpDriverQueue->GetNextEvent( lpEvent, &lpEvent, &liSize ) )
    {
        if( liDriverType != BrnPhysics::Vehicle::E_DRIVER_TYPE_NETWORK )
        {
            continue;
        }

        const BrnPhysics::Vehicle::BrnNetworkDriverControls* lpNetworkDriver =
            reinterpret_cast<const BrnPhysics::Vehicle::BrnNetworkDriverControls*>( lpEvent );
        CGS_ASSERT(lpNetworkDriver != 0, CGS_ASSERT_MESSAGE_AT("lpNetworkDriver", __FILE__, 2049));   // :2049

        if( lpNetworkDriver->mbCrash )
        {
            continue;
        }

        CGS_ASSERT_AT( lpNetworkDriver->miVehicleID < 8, "Index is out of range (max bits: 8)" , CGS_ARTIST_CGSFASTBITARRAY_H_FILE, 396);
        if( !mCrashingRaceCars.IsBitSet( static_cast<u32>( lpNetworkDriver->miVehicleID ) ) )
        {
            continue;
        }

        const EActiveRaceCarIndex leRaceCar = static_cast<EActiveRaceCarIndex>( lpNetworkDriver->miVehicleID );
        const u32 luRaceCarCrash = FindCrashForRaceCar( leRaceCar );
        CGS_ASSERT( luRaceCarCrash != KU_INVALID_CRASH, "luRaceCarCrash != KU_INVALID_CRASH" );   // :2060

        ResetRaceCarFromCrashIndex( lpOutput, luRaceCarCrash, false );
        OnContactFromNetworkPlayer( leRaceCar );
    }
}

// =================================================================================================
// GenerateOwnedTrafficUpdates @ 0x827C53F0   (957 insns; Hex-Rays gave no pseudocode, so this is
// read from the ARTIST asm)   -- G64-D3 (crash parity 2026-09-23). DWARF BrnCrashModule.cpp:1355,
// locals laTrafficTransforms[601] (:1367), lInitialisedTransforms (:1368), lpVehicleOutputInterface
// (:1374), liEvent (:1375), luVehicle / lbTrafficWillBeRecycled (:1403/:1407), lpNetworkInterface
// (:1420), luTrafficCrash (:1424).
//
// Online only: publish, for every crashing traffic vehicle THIS machine owns, the transform the
// physics module reported this frame, so the other players can replay the wreck.
//   0x827C5418..0x827C547C  tripwires lpOutput != NULL (:1357), IsOnlineGameMode() (:1358)
//   0x827C5480  `lbz 0x152A ; bne -> return` -- nothing is published in a Showtime mode
//   0x827C5494  lInitialisedTransforms.UnSetAll() (10 x `std 0`)
//   0x827C54B0  lpInput->GetVehicleOutputInterface() (0x827BB870); its physical-traffic-state queue
//               is at +0x2620 (length `lwz 0x2628`), read with GetEvent (0x8227BE58)
//   per state:  the entity's owner byte (`lbz 0x320`) must be E_ENTITYTYPE_TRAFFIC_VEHICLE (2) (:1381);
//               luVehicle = its 14-bit index (`extrwi 14,8`); when mCrashingTraffic (+0x808) has the
//               bit: keep the state's mTransform (+0x1C0, 4 x lvx/stvx into the local array), assert
//               it was not seen twice (:1392) and mark it
//   0x827C591C..0x827C5E20  an ASSERT-ONLY pass over all 600 vehicles: a crashing vehicle with no
//               transform that is neither network-crashing (+0x858) nor about to be recycled
//               (WillTrafficVehicleBeRecycledNextFrame @0x827BBB10) trips :1410
//   0x827C5E68  tripwire: no traffic crashes, or meLocalActiveRaceCarIndex is a valid slot (:1418)
//   0x827C5EB0  lpOutput->GetNetworkOutputInterface() (0x827BB9C0, write lock)
//   per record: owner (`lbz 0 ; extsb`) == meLocalActiveRaceCarIndex (+0x1520) -> luVehicle (`lhz 2`),
//               tripwire mCrashingTraffic bit (:1433 "Inconsistent state for vehicle"), and when the
//               transform arrived: AddOwnedTrafficUpdate(luVehicle, transform) (0x827C3528)
// =================================================================================================
void CrashModule::GenerateOwnedTrafficUpdates( const CrashIO::InputBuffer_PostPhysics* lpInput,
                                               CrashIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT(lpOutput != 0, CGS_ASSERT_MESSAGE_AT("lpOutput != NULL", __FILE__, 1357));             // :1357
    CGS_ASSERT(mbIsOnlineGameMode, CGS_ASSERT_MESSAGE_AT("IsOnlineGameMode()", __FILE__, 1358));      // :1358

    if( mbIsShowtimeGameMode )
    {
        return;
    }

    Matrix44Affine                   laTrafficTransforms[601];
    CgsContainers::FastBitArray<601> lInitialisedTransforms;
    lInitialisedTransforms.UnSetAll();

    const CrashIO::InputBuffer_PostPhysics::VehicleOutputInterface* lpVehicleOutputInterface =
        lpInput->GetVehicleOutputInterface();
    const BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue* lpTrafficStates =
        lpVehicleOutputInterface->GetTrafficStateQueue();

    for( s32 liEvent = 0; liEvent < lpTrafficStates->GetLength(); ++liEvent )
    {
        const BrnPhysics::Vehicle::PhysicalTrafficState* lpEvent = &lpTrafficStates->GetEvent( liEvent );
        CGS_ASSERT( ( lpEvent->mEntityID.muValue >> 24 ) == 2u,
                    "lpEvent->mEntityID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE" );   // :1381
        const u32 luVehicle = ( lpEvent->mEntityID.muValue >> 10 ) & 0x3FFFu;
        CGS_ASSERT( luVehicle < 600, "Index is out of range (max bits: 600)" );
        if( mCrashingTraffic.IsBitSet( luVehicle ) )
        {
            laTrafficTransforms[luVehicle] = lpEvent->mTransform;
            CGS_ASSERT( !lInitialisedTransforms.IsBitSet( luVehicle ),
                        "!lInitialisedTransforms.IsBitSet( luVehicle )" );                        // :1392
            lInitialisedTransforms.SetBit( luVehicle );
        }
    }

    for( u32 luVehicle = 0; luVehicle < 600; ++luVehicle )
    {
        const bool lbTrafficWillBeRecycled = WillTrafficVehicleBeRecycledNextFrame( static_cast<u16>( luVehicle ) );
        if( mCrashingTraffic.IsBitSet( luVehicle ) && !lInitialisedTransforms.IsBitSet( luVehicle ) &&
            !mCrashingNetworkTraffic.IsBitSet( luVehicle ) )
        {
            CGS_ASSERT( lbTrafficWillBeRecycled, "Didn't receive transform for crashing traffic vehicle" );   // :1410
        }
    }

    CGS_ASSERT( mTrafficCrashes.GetLength() == 0 ||
                ( meLocalActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                  meLocalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                "( mTrafficCrashes.GetLength() == 0 ) || ( (meLocalActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0)"
                " && (meLocalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) )" );                // :1418

    CrashIO::NetworkOutputInterface* lpNetworkInterface = lpOutput->GetNetworkOutputInterface();
    for( u32 luTrafficCrash = 0; luTrafficCrash < mTrafficCrashes.GetLength(); ++luTrafficCrash )
    {
        const TrafficCrash& lrTrafficCrash = mTrafficCrashes.GetItem( luTrafficCrash );
        if( lrTrafficCrash.GetOwner() != meLocalActiveRaceCarIndex )
        {
            continue;
        }

        const u32 luVehicle = lrTrafficCrash.GetVehicleIndex();
        CGS_ASSERT( luVehicle < 600, "Index is out of range (max bits: 600)" );
        CGS_ASSERT( mCrashingTraffic.IsBitSet( luVehicle ), "Inconsistent state for vehicle" );   // :1433
        if( lInitialisedTransforms.IsBitSet( luVehicle ) )
        {
            lpNetworkInterface->AddOwnedTrafficUpdate( luVehicle, laTrafficTransforms[luVehicle] );
        }
    }

}

// =================================================================================================
// HandleNetworkCrashingTraffic @ 0x827CB788   (1268 insns)   -- G64-D2 (crash parity FX-NETCRASH
// 2026-09-24). DWARF BrnCrashModule.cpp:1054, locals liEvent (:1060), lpCrashingTrafficQueue (:1061),
// lCrashingTrafficForPlayer (:1063), leActiveRaceCarIndex (:1064), lEvent / luVehicle /
// lbContentious (:1083-:1085), luTrafficCrash / lpTrafficCrash (:1096 / :1101), lVolumeInstanceId
// (:1130), lNewCrashingTraffic / lClearedUpTraffic (:1140 / :1141), luVehicle / luTrafficCrash /
// lpTrafficCrash (:1176-:1184, :1202, :1223-:1228).
//
// Online only -- the receive twin of GenerateOwnedTrafficUpdates. For every OTHER player whose
// crashing-traffic updates arrived this frame (the network TrafficManager marks the car and fills
// its queue): replay each transform onto the physics module, then diff the set of vehicles that
// player updated this frame against the set it owned last frame -- the new ones become (or are
// confirmed as) that player's network wrecks, the ones it stopped sending are cleared up.
//   0x827CB7B4..0x827CB82C  tripwires lpInput (:1056), lpOutput (:1057), IsOnlineGameMode() (:1058)
//   0x827CB9A4  per race car 0..7: GetNetworkInputInterface (0x827BB330) ->
//               IsRaceCarMarkedForUpdate; not marked -> next car (0x827CCB14)
//   0x827CB9C0..0x827CBA0C  tripwires meLocalActiveRaceCarIndex in [0,8) (:1074) and
//               leActiveRaceCarIndex != meLocalActiveRaceCarIndex (:1075)
//   0x827CBA14  lCrashingTrafficForPlayer.Clear() (`stw 0` the length)
//   0x827CBA18..0x827CBA58  GetNetworkInputInterface()->GetCrashingTrafficUpdateQueue(car) (inlined)
//   PER EVENT (0x827CBA74..0x827CBD6C):
//     GetEvent (0x8254D968), copy; Contains-assert "Duplicate crashing traffic update for player"
//     (:1089); lbContentious when the vehicle is crashing here (mCrashingTraffic +0x808) with a
//     record that another player owns and has CONFIRMED (0x827CBC88..0x827CBCA8), or that wants to
//     be cleared up (flag bit 0, 0x827CBCAC..0x827CBCBC); and when another player slammed it
//     (maiSlammedTrafficOwners +0x8A8 != -1 and != this car, 0x827CBCC0..0x827CBCDC).
//     Not contentious: MakeTrafficVolumeInstanceId (inlined, CgsEntityId.h:160 assert) ->
//     GetVehicleInputInterface (0x827BB678) -> UpdateNetworkTraffic (inlined AddEvent 0x827C2950 on
//     +139760) -> lCrashingTrafficForPlayer.Insert (0x827C7B40).
//   0x827CBD70..0x827CBD98  lNewCrashingTraffic = this frame \ maCrashingTrafficForPlayers[car]
//               (+0xB00 + car*0x144); lClearedUpTraffic = last frame's \ this frame
//               (SetDifference 0x827C8608, both)
//   NEW (0x827CBDA8..0x827CC56C):
//     already crashing here -> assert a record (:1182) that is still UNCONFIRMED (:1185); another
//       owner's -> Erase from that owner's set (0x827C2E80) + Insert into this car's; then
//       ConfirmNetworkOwner(car) (inlined, :280)
//     else -> asserts !mCrashingNetworkTraffic (:1199) and no record (:1200); Grow (0x827B54D8) +
//       Construct(car, vehicle, KF_NETWORK_CRASH_TIMEOUT flt_820CA5A8 20.0f, true) + ConfirmNetworkOwner
//       (inlined); mCrashingTraffic SetBit; Insert into this car's set;
//       GetTrafficOutputInterface (0x827BB5D0)->StartNetworkTrafficVehicleCrashing (inlined AddEvent
//       0x827C2D48 on +0x510)
//     both -> mCrashingNetworkTraffic (+0x858) SetBit
//   CLEARED UP (0x827CC570..0x827CCAEC): assert a record (:1226); an UNCONFIRMED record is skipped;
//     else tripwires owner == car (:1236), mCrashingTraffic (:1238) and mCrashingNetworkTraffic (:1239)
//     set, then SetNetworkVehicleClearedUp() (inlined, :312). The vehicle is NOT erased from the
//     player's set here.
//   0x827CCAF8  OnContactFromNetworkPlayer(car) -- every marked car, whatever its queue held.
// =================================================================================================
void CrashModule::HandleNetworkCrashingTraffic( const CrashIO::InputBuffer_PreScene* lpInput,
                                                CrashIO::OutputBuffer_PreScene* lpOutput )
{
    CGS_ASSERT(lpInput != 0, CGS_ASSERT_MESSAGE_AT("lpInput != NULL", __FILE__, 1056));             // :1056
    CGS_ASSERT(lpOutput != 0, CGS_ASSERT_MESSAGE_AT("lpOutput != NULL", __FILE__, 1057));           // :1057
    CGS_ASSERT(mbIsOnlineGameMode, CGS_ASSERT_MESSAGE_AT("IsOnlineGameMode()", __FILE__, 1058));    // :1058

    CgsContainers::Set<u16, 160> lCrashingTrafficForPlayer;

    for( EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++ )
    {
        if( !lpInput->GetNetworkInputInterface()->IsRaceCarMarkedForUpdate( leActiveRaceCarIndex ) )
        {
            continue;
        }

        CGS_ASSERT( meLocalActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                    meLocalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    "(meLocalActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && "
                    "(meLocalActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)" );                 // :1074
        CGS_ASSERT( leActiveRaceCarIndex != meLocalActiveRaceCarIndex,
                    "leActiveRaceCarIndex != meLocalActiveRaceCarIndex" );                          // :1075

        lCrashingTrafficForPlayer.Clear();

        const CrashIO::NetworkInputInterface::CrashingTrafficUpdateQueue* lpCrashingTrafficQueue =
            lpInput->GetNetworkInputInterface()->GetCrashingTrafficUpdateQueue( leActiveRaceCarIndex );

        for( s32 liEvent = 0; liEvent < lpCrashingTrafficQueue->GetLength(); ++liEvent )
        {
            const CrashIO::CrashingTrafficUpdateEvent lEvent = lpCrashingTrafficQueue->GetEvent( liEvent );
            const u16 luVehicle = lEvent.muVehicleId;
            bool lbContentious = false;

            CGS_ASSERT( !lCrashingTrafficForPlayer.Contains( luVehicle ),
                        "Duplicate crashing traffic update for player" );                           // :1089

            if( mCrashingTraffic.IsBitSet( luVehicle ) )
            {
                const u32 luTrafficCrash = FindCrashForTrafficVehicle( luVehicle );
                if( luTrafficCrash != KU_INVALID_CRASH )
                {
                    const TrafficCrash* lpTrafficCrash = &mTrafficCrashes.GetItem( luTrafficCrash );
                    if( lpTrafficCrash->GetOwner() != leActiveRaceCarIndex &&
                        !lpTrafficCrash->IsUnconfirmedNetwork() )
                    {
                        lbContentious = true;
                    }
                    if( lpTrafficCrash->WantsToBeClearedUp() )
                    {
                        lbContentious = true;
                    }
                }
            }

            if( maiSlammedTrafficOwners[luVehicle] != -1 &&
                maiSlammedTrafficOwners[luVehicle] != leActiveRaceCarIndex )
            {
                lbContentious = true;
            }

            if( !lbContentious )
            {
                const CgsSceneManager::VolumeInstanceId lVolumeInstanceId =
                    BrnTraffic::MakeTrafficVolumeInstanceId( luVehicle );
                lpOutput->GetVehicleInputInterface()->UpdateNetworkTraffic( lVolumeInstanceId, lEvent.mTransform );
                lCrashingTrafficForPlayer.Insert( luVehicle );

            }
        }

        CgsContainers::Set<u16, 160> lNewCrashingTraffic;
        CgsContainers::Set<u16, 160> lClearedUpTraffic;
        lNewCrashingTraffic.SetDifference( lCrashingTrafficForPlayer,
                                           maCrashingTrafficForPlayers[leActiveRaceCarIndex] );
        lClearedUpTraffic.SetDifference( maCrashingTrafficForPlayers[leActiveRaceCarIndex],
                                         lCrashingTrafficForPlayer );

        for( u32 luVehicleIndex = 0; luVehicleIndex < lNewCrashingTraffic.GetLength(); ++luVehicleIndex )
        {
            const u32 luVehicle = lNewCrashingTraffic[luVehicleIndex];

            if( mCrashingTraffic.IsBitSet( luVehicle ) )
            {
                const u32 luTrafficCrash = FindCrashForTrafficVehicle( luVehicle );
                CGS_ASSERT( luTrafficCrash != KU_INVALID_CRASH, "luTrafficCrash != KU_INVALID_CRASH" );   // :1182
                TrafficCrash* lpTrafficCrash = &mTrafficCrashes.GetItem( luTrafficCrash );
                CGS_ASSERT( lpTrafficCrash->IsUnconfirmedNetwork(),
                            "lpTrafficCrash->IsUnconfirmedNetwork()" );                                 // :1185

                if( lpTrafficCrash->GetOwner() != leActiveRaceCarIndex )
                {
                    maCrashingTrafficForPlayers[lpTrafficCrash->GetOwner()].Erase( static_cast<u16>( luVehicle ) );
                    maCrashingTrafficForPlayers[leActiveRaceCarIndex].Insert( static_cast<u16>( luVehicle ) );
                }
                lpTrafficCrash->ConfirmNetworkOwner( leActiveRaceCarIndex );
            }
            else
            {
                CGS_ASSERT( !mCrashingNetworkTraffic.IsBitSet( luVehicle ),
                            "!mCrashingNetworkTraffic.IsBitSet( luVehicle )" );                         // :1199
                CGS_ASSERT( FindCrashForTrafficVehicle( luVehicle ) == KU_INVALID_CRASH,
                            "FindCrashForTrafficVehicle( luVehicle ) == KU_INVALID_CRASH" );             // :1200

                TrafficCrash* lpTrafficCrash = mTrafficCrashes.Grow();
                lpTrafficCrash->Construct( leActiveRaceCarIndex, static_cast<u16>( luVehicle ),
                                           KF_NETWORK_CRASH_TIMEOUT, true );
                lpTrafficCrash->ConfirmNetworkOwner( leActiveRaceCarIndex );

                mCrashingTraffic.SetBit( luVehicle );
                maCrashingTrafficForPlayers[leActiveRaceCarIndex].Insert( static_cast<u16>( luVehicle ) );
                lpOutput->GetTrafficOutputInterface()->StartNetworkTrafficVehicleCrashing( luVehicle );
            }

            mCrashingNetworkTraffic.SetBit( luVehicle );
        }

        for( u32 luVehicleIndex = 0; luVehicleIndex < lClearedUpTraffic.GetLength(); ++luVehicleIndex )
        {
            const u32 luVehicle = lClearedUpTraffic[luVehicleIndex];
            const u32 luTrafficCrash = FindCrashForTrafficVehicle( luVehicle );
            CGS_ASSERT( luTrafficCrash != KU_INVALID_CRASH, "luTrafficCrash != KU_INVALID_CRASH" );       // :1226
            TrafficCrash* lpTrafficCrash = &mTrafficCrashes.GetItem( luTrafficCrash );

            if( lpTrafficCrash->IsUnconfirmedNetwork() )
            {
                continue;
            }

            CGS_ASSERT( lpTrafficCrash->GetOwner() == leActiveRaceCarIndex,
                        "lpTrafficCrash->GetOwner() == leActiveRaceCarIndex" );                         // :1236
            CGS_ASSERT( mCrashingTraffic.IsBitSet( luVehicle ),
                        "mCrashingTraffic.IsBitSet( luVehicle ) is false" );                            // :1238
            CGS_ASSERT( mCrashingNetworkTraffic.IsBitSet( luVehicle ),
                        "mCrashingNetworkTraffic.IsBitSet( luVehicle ) is false" );                     // :1239
            lpTrafficCrash->SetNetworkVehicleClearedUp();
        }

        OnContactFromNetworkPlayer( leActiveRaceCarIndex );
    }
}

// =================================================================================================
// PreSceneUpdate @ 0x827D3A60   (86 insns)
//
//   0x827D3A80  LockForWrite(lpOutput) ; LockForRead(lpInput)         -- in THAT order
//   0x827D3A90  lpInput->GetActiveRaceCarInterface(): latch the player's slot into
//               meLocalActiveRaceCarIndex, but ONLY when the interface says a player car is active
//               (0x827D3AD8 `lbz r11, 0x2860` == mbIsPlayerCarActive; :967/:980 tripwires)
//   0x827D3B30  HandleGameActions                                     [LIVE]
//   0x827D3B44  if (!(lUpdateSet & 1)) {
//   0x827D3B48      ClearUpRecycledTraffic                            [LIVE]
//   0x827D3B4C      if (mbIsOnlineGameMode) { HandleNetworkCrashingTraffic ;       [LIVE online, G64-D2]
//                                             ResetCrashedNetworkRaceCars }   [LIVE online, G65-D1]
//   0x827D3B78      if (mbClearUpEnabled)  { TickCrashes ; ClearupCrashes }   <-- THE EXIT
//               }
//   0x827D3BA4  UnlockForRead(lpInput) ; UnlockForWrite(lpOutput)
//
// ⚠️ `lUpdateSet & 1` is the console's own skip bit (the same `a6 & 1` every module's update
// carries). Reproduced, not dropped.
// =================================================================================================
void CrashModule::PreSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                  CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                  const CrashIO::InputBuffer_PreScene* lpInput,
                                  CrashIO::OutputBuffer_PreScene* lpOutput,
                                  BrnUpdateSet lUpdateSet )
{
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    {
        const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
            lpInput->GetActiveRaceCarInterface();

        if( lpActiveRaceCarInterface->IsPlayerCarActive() )
        {
            meLocalActiveRaceCarIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        }
    }

    HandleGameActions(lpInput, lpOutput);

    if( ( lUpdateSet & 1 ) == 0 )
    {
        ClearUpRecycledTraffic(lpOutput);

        if( mbIsOnlineGameMode )                                  // 0x827D3B4C lbz 0x1529
        {
            HandleNetworkCrashingTraffic( lpInput, lpOutput );    // 0x827D3B64 (G64-D2)
            ResetCrashedNetworkRaceCars( lpInput, lpOutput );    // 0x827D3B74
        }

        if( mbClearUpEnabled )
        {
            TickCrashes( lpInput );
            ClearupCrashes( lpInput, lpOutput );
        }
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

// =================================================================================================
// PostPhysicsUpdate @ 0x827D3BB8   (83 insns)
//
//   0x827D3BC8  LockForWrite(lpOutput) ; LockForRead(lpInput)
//   if (!(lUpdateSet & 1)) {
//     0x827D3BDC  ProcessCrashedRaceCarEvents                          <-- THE ENTRY
//     0x827D3BE8  mRecycledTrafficQueue.Clear() then .Append(vmOut->mRemovedTrafficEventQueue)
//                 and the two 80-byte traffic bitmask copies from the traffic input interface
//                                                                       [LIVE]
//     0x827D3C40  ProcessSlammedTrafficEvents / HandleNewCrashingTraffic /
//                 HandleRecoveredSlammedTraffic / HandleCleanedUpTrafficEvents  [LIVE]
//     0x827D3CA8  if (mbIsOnlineGameMode) GenerateOwnedTrafficUpdates   [LIVE online, G64-D3/G65-D3]
//     0x827D3C68  if (mbNeedToSendEndingMessage) { VariableEventQueue<1536,16>::AddEvent(
//                     lpOutput->GetGameEventQueue(), &record, 42, 1);
//                   mbNeedToSendEndingMessage = false; }                [LIVE]
//   }
//   0x827D3C8C  UnlockForRead ; UnlockForWrite
// =================================================================================================
void CrashModule::PostPhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                     CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                     const CrashIO::InputBuffer_PostPhysics* lpInput,
                                     CrashIO::OutputBuffer_PostPhysics* lpOutput,
                                     BrnUpdateSet lUpdateSet )
{
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    if( ( lUpdateSet & 1 ) == 0 )
    {
        ProcessCrashedRaceCarEvents( lpInput, lpOutput );

        mRecycledTrafficQueue.Clear();
        mRecycledTrafficQueue.Append(*lpInput->GetVehicleManagerOutputInterface()->GetRemovedTrafficEventQueue());
        const CrashIO::TrafficInputInterface* traffic = lpInput->GetTrafficInputInterface();
        mTrafficRenderedLastFrame = *traffic->GetRenderingBits();
        mTrafficFarFromCameraLastFrame = *traffic->GetFarFromCameraBits();
        ProcessSlammedTrafficEvents(lpInput);
        HandleNewCrashingTraffic(lpInput);
        HandleRecoveredSlammedTraffic(lpInput);
        HandleCleanedUpTrafficEvents(lpInput);
        if (mbIsOnlineGameMode)                                 // 0x827D3CA8 lbz 0x1529 ; beq 0x827D3CC4
        {
            GenerateOwnedTrafficUpdates(lpInput, lpOutput);     // 0x827D3CC0 (r4 = in, r5 = out)
        }

        if( mbNeedToSendEndingMessage )
        {
            // ARTIST 827D3CD0..827D3CE8: publish the empty signal, then consume the latch.
            BrnGameState::GameStateModuleIO::PlayerCrashEndingEvent lEvent;
            lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent),
                BrnGameState::GameStateModuleIO::E_EVENT_PLAYER_CRASH_ENDING, sizeof(lEvent));
            mbNeedToSendEndingMessage = false;
        }
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

}   // namespace BrnWorld
