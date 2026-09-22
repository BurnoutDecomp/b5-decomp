// BrnWorld::CrashModule -- three out-of-line members the X360 ARTIST build emitted for the world
// crash module (World/CrashModule/BrnCrashModule.cpp). See BrnCrashModule.h for the class banner
// and the X360 member-offset map.
//
// Reconstructed methods (X360 spine authoritative):
//   CrashModule()                                   @ 0x827DE960
//   FindCrashForRaceCar(EActiveRaceCarIndex) const  @ 0x827C6AD8
//   WillTrafficVehicleBeRecycledNextFrame(u16)      @ 0x827BBB10
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
        CGS_ASSERT(lpInput, "lpInput");
        const auto* lpQueue = lpInput->GetGameActionQueue();
        const CgsModule::Event* lpEvent = nullptr;
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
                const auto* lpAction = reinterpret_cast<const PrepareForModeAction*>(lpEvent);
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
            static const bool lbTrace = std::getenv("BRN_CRASH_ACTION_DIAG") != nullptr;
            static u32 luTraceCount = 0;
            if (lbTrace && luTraceCount++ < 64)
            {
                char lacMessage[192];
                std::snprintf(lacMessage, sizeof(lacMessage),
                    "[crash-action] id=%d cleanup=%d fastAI=%d extensions=%d playerTime=%.3f online=%d showtime=%d\n",
                    liAction, mbClearUpEnabled, mbFastCrashesForAI, miNumCrashExtensions,
                    mfPlayerCrashTime, mbIsOnlineGameMode, mbIsShowtimeGameMode);
                CgsDev::Log::WriteToLog(lacMessage);
            }
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
                if (maCrashingTrafficForPlayers[liPlayer].Find(luVehicle) != Set<u16, 160>::KU_INVALID)
                {
                    maCrashingTrafficForPlayers[liPlayer].Erase(luVehicle);
                    break;
                }
        }
        else if (maCrashingTrafficForPlayers[leOwner].Find(luVehicle) != Set<u16, 160>::KU_INVALID)
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
        CGS_ASSERT(luVehicle < 0x258u, "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

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
