#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/BrnGameActions.h"                                   // OnPlayerTakedownAction / ShutdownAction / SetPlayerCarDriverAction
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                       // ModeManager (mode type, IsOnlineGameMode, the mode-params flags)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"          // GameModeParams::KU_FLAG_*
#include "GameSource/GameState/Progression/BrnProgressionManager.h"                // ProgressionManager::OnPursuitWon (the free-burn rival shutdown)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // RaceCarCrashEvent / RaceCarState
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // MarkedManInterface
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                      // CgsSceneManager::EntityId (owner / entity-index decode of the packed crasher word)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                           // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstring>                                                                  // std::memset (the residue-zeroed action records)

// ============================================================================================
// BrnGameState::TakedownManager -- Update and the detect / process chain (BrnTakedownManager.cpp
// on the console; split out of the lifecycle / camera / reset half that lives in
// BrnTakedownManager.cpp). Reconstructed from BURNOUT_X360_ARTIST.XEX, 2026-09-02 (takedown wave,
// agent T2):
//
//   Update                                  @0x8239FAC0
//   DetectTakedowns                         @0x8239D808
//   DetectInstantTakedown                   @0x82399638
//   DetectStandardTakedown                  @0x8237A3C0   (the core classifier)
//   DetectNetworkTakedowns                  @0x823997A0
//   ProcessQueuedTakedowns                  @0x82399A98
//   ProcessTakedownEvent                    @0x82393D40
//   GetTakedownTypeFromTrafficVehicleIndex  @0x82366288
//
// The ids the console packs into a crash event: mRaceCarVolumeInstanceID's entity word carries the
// crashed car's ACTIVE race-car index in the 14-bit entity field (`ld; srdi 32; extrwi 14,8`), and
// mCrasherEntityID is an EntityId whose owner byte says who hit it (2 == a traffic vehicle) and whose
// entity field is that aggressor's active index (race car) or traffic vehicle index (traffic).
//
// Mode-manager reads: mm+3476 is meCurrentGameModeType, mm+3480 mpCurrentGameMode (its +172 byte is
// mbIsOnline -- IsOnlineGameMode()), and the `ldx mm+0x8BE0` reads are mCurrentGameModeParams.muFlags
// (GetCurrentGameModeParams()->GetFlag); the three bits tested are named at their sites.
//
// Speeds: RaceCarState::mfSpeedMPH is in mph; every takedown-side compare first scales it by the
// console's 0.44704 (flt_82F31928) into m/s, the unit the KF_ speed tunables are in.
// ============================================================================================

namespace BrnGameState
{
    using BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
    using BrnPhysics::Vehicle::RaceCarCrashEvent;
    using BrnPhysics::Vehicle::RaceCarState;

    namespace
    {
        // DWARF BrnTakedownManager.cpp:38 (file-scope const of the console TU). X360 flt_82001DA0 ==
        // 0.5: Update posts the "player drives again" action once mfPlayerControlTimer passes it.
        const f32 KF_PLAYER_CONTROL_RETURN_DELAY_TIME = 0.5f;

        // flt_82F31928 -- the engine-wide mph -> m/s scale (0.44704), applied to mfSpeedMPH before the
        // m/s tunables are compared against it.
        const f32 KF_MPH_TO_METRES_PER_SECOND = 0.447039992f;

        // The (inlined) TakedownEvent::Construct the three event builders run first (DWARF
        // BrnTakedownManagerTypes.h:83; no out-of-line X360 symbol). Attested field set, from
        // DetectStandardTakedown @0x8237A4E8..0x8237A504: both car ids 0, type NONE, both counts 0,
        // the three flags false. The two indices are written by every builder straight after, so the
        // console shows no Construct-time store for them.
        void ConstructTakedownEvent(TakedownEvent& lrEvent)
        {
            lrEvent.mAggressorCarID         = 0;
            lrEvent.mVictimCarID            = 0;
            lrEvent.meType                  = E_TAKEDOWN_NONE;
            lrEvent.miMultipleTakedownCount = 0;
            lrEvent.miTakedownChainCount    = 0;
            lrEvent.mbMarkedManTakeDown     = false;
            lrEvent.mbRemote                = false;
            lrEvent.mbSettledScore          = false;
        }

        // The active-race-car index the physics side packed into a crash event's volume-instance id.
        EActiveRaceCarIndex GetCrashedActiveRaceCarIndex(const RaceCarCrashEvent* lpCrashEvent)
        {
            return static_cast<EActiveRaceCarIndex>(lpCrashEvent->mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());
        }

        // The crasher's packed EntityId word, as the real CgsSceneManager::EntityId so the owner byte
        // and the 14-bit entity index come out through the named accessors (the BrnCommonTypes.h
        // EntityId is storage only). Same idiom as BrnChallengeManager_wC_05.cpp.
        CgsSceneManager::EntityId GetCrasherEntityId(const RaceCarCrashEvent* lpCrashEvent)
        {
            return CgsSceneManager::EntityId(lpCrashEvent->mCrasherEntityID.muValue);
        }

        // The active-race-car output interface speaks the GLOBAL ::EActiveRaceCarIndex
        // (BurnoutConstants.h); this manager's DWARF shape speaks the same-valued BrnGameState::
        // EActiveRaceCarIndex (BrnTakedownManagerTypes.h, which already notes the split). Bridge at
        // the interface boundary only.
        inline ::EActiveRaceCarIndex GlobalIndex(EActiveRaceCarIndex leIndex)   { return static_cast<::EActiveRaceCarIndex>(leIndex); }
        inline EActiveRaceCarIndex   LocalIndex(::EActiveRaceCarIndex leIndex)  { return static_cast<EActiveRaceCarIndex>(leIndex); }
    }

    // ----------------------------------------------------------------------------------------
    // Update @0x8239FAC0 -- BrnTakedownManager.cpp:171
    // ----------------------------------------------------------------------------------------
    void TakedownManager::Update(RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                 f32 lfDeltaTime,
                                 RaceCarCrashEventQueue* lpRaceCarCrashEventQueue,
                                 CrashingRaceCarInterface* lpCrashingVehicleInterface,
                                 const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                 GameStateModuleIO::OutputBuffer* lpOutput,
                                 TrafficTypeResponseQueue* lpLastTrafficTypeResponseQueue)
    {
        CGS_ASSERT(lpActiveCarInterface != nullptr,           "lpActiveCarInterface != NULL");            // :180
        CGS_ASSERT(lpRaceCarCrashEventQueue != nullptr,       "lpRaceCarCrashEventQueue != NULL");        // :181
        CGS_ASSERT(lpCrashingVehicleInterface != nullptr,     "lpCrashingVehicleInterface != NULL");      // :182
        CGS_ASSERT(lpInput != nullptr,                        "lpInput != NULL");                         // :183
        CGS_ASSERT(lpOutput != nullptr,                       "lpOutput != NULL");                        // :184
        CGS_ASSERT(lpLastTrafficTypeResponseQueue != nullptr, "lpLastTrafficTypeResponseQueue != NULL");  // :185
        // (lpCrashingVehicleInterface is asserted and then never read -- as on the console.)

        UpdateTakedownTimes(lpActiveCarInterface, lfDeltaTime);

        // Showtime never runs the takedown camera / reset: kill a live camera, else tick the reset logic.
        const GameStateModuleIO::EGameModeType leGameModeType = mpModeManager->GetCurrentGameModeType();
        if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
            leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            if (IsInTakedownCamera())
            {
                EndTakedownCamera(lpOutput->GetGameActionQueue(), LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()));
            }
        }
        else
        {
            UpdatePlayerResetStatus(lpActiveCarInterface, lpOutput->GetGameActionQueue(), lfDeltaTime);
        }

        // Local crash classification runs only for an active player car whose engine is running
        // (offline; online skips the engine test) in a mode that has not disabled takedowns
        // (`rlwinm r11,r11,0,9,9` @0x8239FD38 == muFlags bit 22 == KU_FLAG_DISABLE_ALL_TDS).
        bool lbDetectTakedowns = lpActiveCarInterface->IsPlayerCarActive();
        if (lbDetectTakedowns && !mpModeManager->IsOnlineGameMode())
        {
            lbDetectTakedowns = lpActiveCarInterface->GetPlayerEngineState() == E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
        }
        if (lbDetectTakedowns && mpModeManager->GetCurrentGameMode() != nullptr)
        {
            lbDetectTakedowns = !mpModeManager->GetCurrentGameModeParams()->GetFlag(GameModeParams::KU_FLAG_DISABLE_ALL_TDS);
        }
        if (lbDetectTakedowns)
        {
            DetectTakedowns(lpInput, lpOutput, lpRaceCarCrashEventQueue, lpActiveCarInterface, lpLastTrafficTypeResponseQueue);
        }

        DetectNetworkTakedowns(lpInput, lpActiveCarInterface, lpOutput);
        ProcessQueuedTakedowns(lpInput, lpActiveCarInterface, lpOutput, lfDeltaTime);

        if (IsInTakedownCamera())
        {
            UpdateTakedownCamera(lfDeltaTime, lpOutput, lpActiveCarInterface);
        }

        // Hand the car back to the player KF_PLAYER_CONTROL_RETURN_DELAY_TIME after the camera released
        // it: action 7 with only the driver word (1 == entity module) and the drive-thru flag (0)
        // written (@0x8239FE1C..0x8239FE3C); the rest of the record is stack residue on the console,
        // zeroed here for reproducibility.
        if (mbPlayerWaitingForControl)
        {
            mfPlayerControlTimer += lfDeltaTime;
            if (mfPlayerControlTimer > KF_PLAYER_CONTROL_RETURN_DELAY_TIME)
            {
                GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction;
                std::memset(&lSetPlayerCarDriverAction, 0, sizeof(lSetPlayerCarDriverAction));
                lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
                lSetPlayerCarDriverAction.mbIsDriveThru = false;
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                    GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                    static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));
                mbPlayerWaitingForControl = false;
            }
        }
    }

    // ----------------------------------------------------------------------------------------
    // DetectTakedowns @0x8239D808 -- BrnTakedownManager.cpp:395
    // Walk this frame's primary crashes; offline every crash is a candidate, online only the
    // local player's own crash is (the rest arrive through DetectNetworkTakedowns).
    // ----------------------------------------------------------------------------------------
    void TakedownManager::DetectTakedowns(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                          GameStateModuleIO::OutputBuffer* lpOutput,
                                          const RaceCarCrashEventQueue* lpCrashQueue,
                                          RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                          TrafficTypeResponseQueue* lpLastTrafficTypeResponseQueue)
    {
        if (!lpActiveCarInterface->IsPlayerCarActive())
        {
            return;
        }

        const EActiveRaceCarIndex lePlayerActiveRaceCarIndex = LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex());

        for (s32 liCrashIndex = 0; liCrashIndex < lpCrashQueue->GetLength(); ++liCrashIndex)
        {
            const RaceCarCrashEvent* lpCrashEvent = &lpCrashQueue->GetEvent(liCrashIndex);
            if (!lpCrashEvent->mbIsPrimaryCrash)
            {
                continue;
            }

            const EActiveRaceCarIndex leCrashedActiveRaceCarIndex = GetCrashedActiveRaceCarIndex(lpCrashEvent);
            if (mpModeManager->IsOnlineGameMode() && leCrashedActiveRaceCarIndex != lePlayerActiveRaceCarIndex)
            {
                continue;
            }

            // The crashed car is the victim of whatever follows: its own pending takedown (if it was
            // the aggressor of an unconfirmed one) is dropped.
            RaceCarData* lpVictimCarData = GetRaceCarData(leCrashedActiveRaceCarIndex);
            lpVictimCarData->mfTimeSinceVictimCrashed = 0.0f;
            lpVictimCarData->mbWaitingOnTakedown      = false;

            if (!DetectInstantTakedown(lpInput, lpOutput, lpActiveCarInterface, lpCrashEvent))
            {
                DetectStandardTakedown(lpInput, lpOutput, lpActiveCarInterface, lpCrashEvent, lpLastTrafficTypeResponseQueue);
            }
        }
    }

    // ----------------------------------------------------------------------------------------
    // DetectInstantTakedown @0x82399638 -- BrnTakedownManager.cpp:451
    // The physics side already classified this crash (meInstantTakedownType != NONE): the crasher
    // is the aggressor and the takedown is processed immediately, no confirmation wait.
    // ----------------------------------------------------------------------------------------
    bool TakedownManager::DetectInstantTakedown(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                GameStateModuleIO::OutputBuffer* lpOutput,
                                                RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                const RaceCarCrashEvent* lpCrashEvent)
    {
        const EActiveRaceCarIndex leCrashedActiveRaceCarIndex = GetCrashedActiveRaceCarIndex(lpCrashEvent);
        RaceCarData* lpCrashedCarData = GetRaceCarData(leCrashedActiveRaceCarIndex);

        if (lpCrashEvent->meInstantTakedownType == E_TAKEDOWN_NONE)
        {
            return false;
        }
        // Online, a car taken down within the last KF_ONLINE_TAKEDOWN_IGNORE_TIME (flt_82CDBDBC == 5.05 s)
        // is ignored.
        if (mpModeManager->IsOnlineGameMode() &&
            lpCrashedCarData->mfTimeSinceLastTakenDown <= KF_ONLINE_TAKEDOWN_IGNORE_TIME)
        {
            return false;
        }

        const EActiveRaceCarIndex leInstantAggressorActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(GetCrasherEntityId(lpCrashEvent).GetEntityIndex());
        RaceCarData* lpInstantAggresorCarData = GetRaceCarData(leInstantAggressorActiveRaceCarIndex);

        if (!lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leInstantAggressorActiveRaceCarIndex)))
        {
            return false;
        }
        if (lpActiveCarInterface->GetRaceCarState(GlobalIndex(leInstantAggressorActiveRaceCarIndex))->mbCrashing)
        {
            return false;
        }

        TakedownEvent* lpAggressorTakedownEvent = &lpInstantAggresorCarData->mPendingTakedownEvent;
        ConstructTakedownEvent(*lpAggressorTakedownEvent);
        lpAggressorTakedownEvent->meAggressorIndex = leInstantAggressorActiveRaceCarIndex;
        lpAggressorTakedownEvent->meVictimIndex    = leCrashedActiveRaceCarIndex;
        lpAggressorTakedownEvent->mAggressorCarID  = lpActiveCarInterface->GetCarModelId(GlobalIndex(leInstantAggressorActiveRaceCarIndex));
        lpAggressorTakedownEvent->mVictimCarID     = lpActiveCarInterface->GetCarModelId(GlobalIndex(leCrashedActiveRaceCarIndex));
        lpAggressorTakedownEvent->meType           = lpCrashEvent->meInstantTakedownType;

        ProcessTakedownEvent(lpInput, lpActiveCarInterface, lpOutput, lpAggressorTakedownEvent);

        lpInstantAggresorCarData->mbWaitingOnTakedown      = false;
        lpInstantAggresorCarData->mfTimeSinceVictimCrashed = 0.0f;
        return true;
    }

    // ----------------------------------------------------------------------------------------
    // DetectStandardTakedown @0x8237A3C0 -- BrnTakedownManager.cpp:520
    // The core classifier: the crashed car is the victim; IsBeingAttacked names the aggressor from
    // the victim's shunt state; the takedown is ARMED on the aggressor (mbWaitingOnTakedown) and
    // confirmed later by ProcessQueuedTakedowns. Type: DOUBLE when the aggressor already has a
    // multiple in flight, REVENGE when the victim owed one (and the mode allows it), INTO_CAR /
    // INTO_VAN / INTO_BUS when the victim was checked into traffic, else STANDARD.
    // ----------------------------------------------------------------------------------------
    void TakedownManager::DetectStandardTakedown(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                 GameStateModuleIO::OutputBuffer* lpOutput,
                                                 RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                 const RaceCarCrashEvent* lpCrashEvent,
                                                 TrafficTypeResponseQueue* lpLastTrafficTypeResponseQueue)
    {
        (void)lpInput;
        (void)lpOutput;

        const EActiveRaceCarIndex leVictimActiveRaceCarIndex = GetCrashedActiveRaceCarIndex(lpCrashEvent);
        if (!lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leVictimActiveRaceCarIndex)))
        {
            return;
        }

        const RaceCarState* lpVictimRaceCarState = lpActiveCarInterface->GetRaceCarState(GlobalIndex(leVictimActiveRaceCarIndex));
        EActiveRaceCarIndex leAggressorActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        if (!IsBeingAttacked(lpVictimRaceCarState, &leAggressorActiveRaceCarIndex))
        {
            return;
        }

        CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex");                 // :533
        CGS_ASSERT(leAggressorActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
                   "leAggressorActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");            // :534

        if (!lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leAggressorActiveRaceCarIndex)))
        {
            return;
        }
        if (lpActiveCarInterface->GetRaceCarState(GlobalIndex(leAggressorActiveRaceCarIndex))->mbCrashing)
        {
            return;
        }

        RaceCarData* lpAggressorCarData = GetRaceCarData(leAggressorActiveRaceCarIndex);
        lpAggressorCarData->mfTimeSinceVictimCrashed = 0.0f;
        lpAggressorCarData->mbWaitingOnTakedown      = true;

        TakedownEvent* lpAggressorTakedownEvent = &lpAggressorCarData->mPendingTakedownEvent;
        ConstructTakedownEvent(*lpAggressorTakedownEvent);
        lpAggressorTakedownEvent->meAggressorIndex = leAggressorActiveRaceCarIndex;
        lpAggressorTakedownEvent->meVictimIndex    = leVictimActiveRaceCarIndex;
        lpAggressorTakedownEvent->mAggressorCarID  = lpActiveCarInterface->GetCarModelId(GlobalIndex(leAggressorActiveRaceCarIndex));
        lpAggressorTakedownEvent->mVictimCarID     = lpActiveCarInterface->GetCarModelId(GlobalIndex(leVictimActiveRaceCarIndex));

        if (lpAggressorCarData->miMultipleTakedownLength <= 1)
        {
            if (lpAggressorCarData->mabRevengeRelationships[leVictimActiveRaceCarIndex] &&
                mpModeManager->CurrentGameModeAllowsRevengeTakedowns())
            {
                lpAggressorCarData->mabRevengeRelationships[leVictimActiveRaceCarIndex] = false;
                lpAggressorTakedownEvent->meType = E_TAKEDOWN_REVENGE;
            }
            else if (static_cast<BrnWorld::EEntityTypeID>(GetCrasherEntityId(lpCrashEvent).GetOwner()) ==
                     BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)
            {
                lpAggressorTakedownEvent->meType = GetTakedownTypeFromTrafficVehicleIndex(
                    lpLastTrafficTypeResponseQueue, GetCrasherEntityId(lpCrashEvent).GetEntityIndex());
            }
            else
            {
                lpAggressorTakedownEvent->meType = E_TAKEDOWN_STANDARD;
            }
        }
        else
        {
            lpAggressorTakedownEvent->meType = E_TAKEDOWN_DOUBLE;
        }

        mTakedownManagerDebugComponent.RecordTakedown(leAggressorActiveRaceCarIndex, leVictimActiveRaceCarIndex);
    }

    // ----------------------------------------------------------------------------------------
    // DetectNetworkTakedowns @0x823997A0 -- BrnTakedownManager.cpp:603
    // Takedowns the network module queued onto the pre-world input (remote players' events) are
    // rebuilt as remote TakedownEvents and processed like a confirmed local one, unless the victim
    // was taken down within the last KF_ONLINE_TAKEDOWN_IGNORE_TIME (flt_82CDBDBC == 5.05 s).
    // ----------------------------------------------------------------------------------------
    void TakedownManager::DetectNetworkTakedowns(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                 RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                 GameStateModuleIO::OutputBuffer* lpOutput)
    {
        // FLAG cross-home cast: GameStateModuleIO::TakedownEventInputQueueType is still a forward-declared
        // incomplete class; the DWARF names this member InputBuffer::TakedownEventQueue ==
        // EventQueue<TakedownEvent,8> (BrnAIModuleIO.h:54), the same cast GameStateModule_gUI_00.cpp
        // carries for the output-side twin.
        const CgsModule::EventQueue<TakedownEvent, 8>* lpOnlineTakedownQueue =
            reinterpret_cast<const CgsModule::EventQueue<TakedownEvent, 8>*>(lpInput->GetTakedownEventInputQueue());

        for (s32 liTakedown = 0; liTakedown < lpOnlineTakedownQueue->GetLength(); ++liTakedown)
        {
            const TakedownEvent& lrQueuedEvent = lpOnlineTakedownQueue->GetEvent(liTakedown);
            const EActiveRaceCarIndex leVictimActiveRaceCarIndex = lrQueuedEvent.meVictimIndex;

            if (GetRaceCarData(leVictimActiveRaceCarIndex)->mfTimeSinceLastTakenDown <= KF_ONLINE_TAKEDOWN_IGNORE_TIME)
            {
                continue;
            }

            TakedownEvent lTakedownEvent;
            ConstructTakedownEvent(lTakedownEvent);
            lTakedownEvent.meAggressorIndex    = lrQueuedEvent.meAggressorIndex;
            lTakedownEvent.meVictimIndex       = lrQueuedEvent.meVictimIndex;
            lTakedownEvent.mbMarkedManTakeDown = lrQueuedEvent.mbMarkedManTakeDown;
            lTakedownEvent.mbRemote            = true;
            lTakedownEvent.mbSettledScore      = lrQueuedEvent.mbSettledScore;
            lTakedownEvent.meType              = lrQueuedEvent.meType;

            CGS_ASSERT(lTakedownEvent.meAggressorIndex != lTakedownEvent.meVictimIndex,
                       "lTakedownEvent.meAggressorIndex != lTakedownEvent.meVictimIndex");          // :633
            CGS_ASSERT(lTakedownEvent.meAggressorIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lTakedownEvent.meAggressorIndex >= E_ACTIVE_RACE_CAR_INDEX_0");             // :634
            CGS_ASSERT(lTakedownEvent.meAggressorIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lTakedownEvent.meAggressorIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");          // :635
            CGS_ASSERT(lTakedownEvent.meVictimIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "lTakedownEvent.meVictimIndex >= E_ACTIVE_RACE_CAR_INDEX_0");                // :636
            CGS_ASSERT(lTakedownEvent.meVictimIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "lTakedownEvent.meVictimIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");             // :637

            ProcessTakedownEvent(lpInput, lpActiveCarInterface, lpOutput, &lTakedownEvent);

            RaceCarData* lpAggressorCarData = GetRaceCarData(lTakedownEvent.meAggressorIndex);
            lpAggressorCarData->mbWaitingOnTakedown      = false;
            lpAggressorCarData->mfTimeSinceVictimCrashed = 0.0f;
        }
    }

    // ----------------------------------------------------------------------------------------
    // ProcessQueuedTakedowns @0x82399A98 -- BrnTakedownManager.cpp:661
    // Confirm the takedowns DetectStandardTakedown armed: once the aggressor has waited out the
    // confirmation time without crashing itself (and, offline, is still moving) the pending event
    // is processed; either way the arm is cleared.
    // ----------------------------------------------------------------------------------------
    void TakedownManager::ProcessQueuedTakedowns(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                                 RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                                 GameStateModuleIO::OutputBuffer* lpOutput, f32 lfSimTimestep)
    {
        for (s32 liRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0; liRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCarIndex)
        {
            const EActiveRaceCarIndex leRaceCarIndex = static_cast<EActiveRaceCarIndex>(liRaceCarIndex);
            RaceCarData* lpRaceCarData = GetRaceCarData(leRaceCarIndex);
            if (!lpRaceCarData->mbWaitingOnTakedown)
            {
                continue;
            }

            lpRaceCarData->mfTimeSinceVictimCrashed += lfSimTimestep;
            // 1.0 s online, 0.1 s offline on the console.
            const f32 lfTakedownConfirmationTime = mpModeManager->IsOnlineGameMode()
                                                       ? KF_ONLINE_TAKEDOWN_CONFIRMATION_TIME
                                                       : KF_TAKEDOWN_CONFIRMATION_TIME;
            if (lpRaceCarData->mfTimeSinceVictimCrashed <= lfTakedownConfirmationTime)
            {
                continue;
            }

            if (lpActiveCarInterface->IsRaceCarActive(GlobalIndex(leRaceCarIndex)))
            {
                const RaceCarState* lpRaceCarState = lpActiveCarInterface->GetRaceCarState(GlobalIndex(leRaceCarIndex));
                if (!lpRaceCarState->mbCrashing)
                {
                    if (lpRaceCarState->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND > KF_MIN_TAKEDOWN_SPEED ||
                        mpModeManager->IsOnlineGameMode())
                    {
                        ProcessTakedownEvent(lpInput, lpActiveCarInterface, lpOutput, &lpRaceCarData->mPendingTakedownEvent);
                    }
                }
            }

            lpRaceCarData->mfTimeSinceVictimCrashed = 0.0f;
            lpRaceCarData->mbWaitingOnTakedown      = false;
        }
    }

    // ----------------------------------------------------------------------------------------
    // ProcessTakedownEvent @0x82393D40 -- BrnTakedownManager.cpp:718
    // A confirmed takedown: marked-man tagging, the takedown camera (player aggressor, offline, mode
    // not enforcing soft takedowns), the OnPlayerTakedown action, the aggressor's chain / multiple
    // bookkeeping, then either the scoring queue (inside a mode) or the free-burn rival shutdown.
    // ----------------------------------------------------------------------------------------
    void TakedownManager::ProcessTakedownEvent(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                               RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
                                               GameStateModuleIO::OutputBuffer* lpOutput, TakedownEvent* lpTakedownEvent)
    {
        CGS_ASSERT(lpTakedownEvent->meType != E_TAKEDOWN_NONE, "lpTakedownEvent->meType != E_TAKEDOWN_NONE");  // :721

        const EActiveRaceCarIndex leAggressorActiveRaceCarIndex = lpTakedownEvent->meAggressorIndex;
        const EActiveRaceCarIndex leVictimActiveRaceCarIndex    = lpTakedownEvent->meVictimIndex;
        RaceCarData* lpAggressorRaceCarData = GetRaceCarData(leAggressorActiveRaceCarIndex);
        RaceCarData* lpVictimRaceCarData    = GetRaceCarData(leVictimActiveRaceCarIndex);

        CGS_ASSERT(leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex,
                   "leAggressorActiveRaceCarIndex != leVictimActiveRaceCarIndex");                     // :728

        // Was the victim the aggressor's marked man? (Not in the online lobby / online showtime.)
        BrnNetwork::BrnNetworkModuleIO::MarkedManInterface lMarkedManInterface;
        lMarkedManInterface.Construct();
        lMarkedManInterface.SetFromPlayerStatusInterface(*lpInput->GetPlayerStatusInterface());

        const GameStateModuleIO::EGameModeType leGameModeType = mpModeManager->GetCurrentGameModeType();
        if (leGameModeType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
            leGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            if (lMarkedManInterface.CheckForMarkedManTakedown(static_cast<::EActiveRaceCarIndex>(leAggressorActiveRaceCarIndex),
                                                              static_cast<::EActiveRaceCarIndex>(leVictimActiveRaceCarIndex)))
            {
                lpTakedownEvent->mbMarkedManTakeDown = true;
            }
        }

        CGS_ASSERT(mpModeManager != nullptr, "mpModeManager != NULL");                                   // :745

        // The takedown camera: player aggressor, offline, and the mode is not enforcing soft takedowns
        // (`extldi r12,r12,64,33` @0x82393EB8 == muFlags bit 33 == KU_FLAG_ENFORCE_SOFT_TAKEDOWNS).
        const bool lbEnforceSoftTakedowns =
            mpModeManager->GetCurrentGameMode() != nullptr &&
            mpModeManager->GetCurrentGameModeParams()->GetFlag(GameModeParams::KU_FLAG_ENFORCE_SOFT_TAKEDOWNS);
        if (!lbEnforceSoftTakedowns && !mpModeManager->IsOnlineGameMode())
        {
            if (leAggressorActiveRaceCarIndex == LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()))
            {
                StartTakedownCamera(lpOutput->GetGameActionQueue(), leVictimActiveRaceCarIndex, lpTakedownEvent->meType);
                mfPlayerSpeedAtTakedown = lpActiveCarInterface->GetPlayerRaceCarState()->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND;
            }
        }

        if (leAggressorActiveRaceCarIndex == LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()))
        {
            GameStateModuleIO::OnPlayerTakedownAction lPlayerTakedownAction;
            lPlayerTakedownAction.meVictimGlobalRaceCarIndex = lpActiveCarInterface->GetGlobalRaceCarIndex(GlobalIndex(leVictimActiveRaceCarIndex));
            lpOutput->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lPlayerTakedownAction),
                GameStateModuleIO::E_ACTION_ON_PLAYER_TAKEDOWN,
                static_cast<s32>(sizeof(lPlayerTakedownAction)));
        }

        // The aggressor's chain / multiple counters, mirrored into the event.
        lpAggressorRaceCarData->mfTimeSinceLastTakedown = 0.0f;
        lpAggressorRaceCarData->miTakedownChainLength++;
        lpAggressorRaceCarData->miMultipleTakedownLength++;
        lpTakedownEvent->miTakedownChainCount    = lpAggressorRaceCarData->miTakedownChainLength;
        lpTakedownEvent->miMultipleTakedownCount = lpAggressorRaceCarData->miMultipleTakedownLength;

        CGS_ASSERT(mpModeManager != nullptr, "mpModeManager != NULL");                                   // :780

        if (mpModeManager->GetCurrentGameMode() != nullptr)
        {
            // Inside a mode: a second takedown inside the multiple window is a DOUBLE; the event goes
            // to the scoring side through the output buffer's takedown queue.
            if (lpAggressorRaceCarData->miMultipleTakedownLength > 1)
            {
                lpTakedownEvent->meType = E_TAKEDOWN_DOUBLE;
            }
            // FLAG cross-home cast (see DetectNetworkTakedowns): TakedownEventOutputQueueType is the
            // forward-declared name of EventQueue<TakedownEvent,8> -- the same cast GameStateModule_gUI_00.cpp
            // and BrnGameModule.cpp carry for this member.
            CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue =
                reinterpret_cast<CgsModule::EventQueue<TakedownEvent, 8>*>(lpOutput->GetTakedownEventOutputQueue());
            lpTakedownEventQueue->AddEvent(*lpTakedownEvent);
        }
        else
        {
            // Free burn: the player taking down a rival shuts that rival down (and wins the pursuit).
            if (leVictimActiveRaceCarIndex != LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()) &&
                leAggressorActiveRaceCarIndex == LocalIndex(lpActiveCarInterface->GetPlayerActiveRaceCarIndex()))
            {
                // The console writes only the victim car id and index (mRivalID is stack residue on the
                // wire); zeroed here for reproducibility.
                GameStateModuleIO::ShutdownAction lShutdown;
                std::memset(&lShutdown, 0, sizeof(lShutdown));
                lShutdown.mVictimCarID  = lpTakedownEvent->mVictimCarID;
                lShutdown.meVictimIndex = leVictimActiveRaceCarIndex;
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lShutdown),
                    GameStateModuleIO::E_ACTION_SHUTDOWN,
                    static_cast<s32>(sizeof(lShutdown)));

                const CgsID lRivalId = lpActiveCarInterface->GetRivalId(GlobalIndex(leVictimActiveRaceCarIndex));
                // 0x823940E4..0x823940F8: r5 = lpOutput->GetGameActionQueue() (the write-locked queue
                // accessor), r4 = the rival id just read, r3 = `lwz 0x290(r30)` == mpProgressionManager,
                // bl ProgressionManager::OnPursuitWon @0x82389F40 -- the pursuit win: FindRivalIndexFromId
                // on the progression data, DefeatRivalAndUnlockCar, the 197 rival-state-change record and
                // the forced autosave. Body: BrnProgressionManager_Rivals.cpp. (mpProgressionManager is
                // wired by TakedownManager::Construct from GameStateModule_gTD_00.cpp, as on the console.)
                mpProgressionManager->OnPursuitWon(lRivalId, lpOutput->GetGameActionQueue());
            }
        }

        // Either way the victim is freshly taken down and now owes the aggressor a revenge takedown.
        lpVictimRaceCarData->mfTimeSinceLastTakenDown = 0.0f;
        lpVictimRaceCarData->mabRevengeRelationships[leAggressorActiveRaceCarIndex] = true;
    }

    // ----------------------------------------------------------------------------------------
    // GetTakedownTypeFromTrafficVehicleIndex @0x82366288 -- BrnTakedownManager.cpp:823
    // Look the traffic vehicle up in the last traffic-type response batch and map its class to the
    // INTO_* takedown type (bigrig / anything else falls back to INTO_CAR).
    // ----------------------------------------------------------------------------------------
    ETakedownType TakedownManager::GetTakedownTypeFromTrafficVehicleIndex(const TrafficTypeResponseQueue* lpResponseQueue,
                                                                          u16 luTrafficVehicleIndex)
    {
        BrnTraffic::VehicleClass lVehicleClass = BrnTraffic::E_VEHICLECLASS_CAR;
        s32 liResponseIndex = 0;
        for (; liResponseIndex < lpResponseQueue->GetLength(); ++liResponseIndex)
        {
            const BrnTraffic::BrnTrafficIO::TrafficTypeResponse& lResponse = lpResponseQueue->GetEvent(liResponseIndex);
            if (lResponse.muVehicleIndex == luTrafficVehicleIndex)
            {
                lVehicleClass = lResponse.meType;   // `lwz r28, 4(r3)` @0x823662DC -- the class word, not IDA's +2
                break;
            }
        }

        CGS_ASSERT(liResponseIndex < lpResponseQueue->GetLength(), "Missing traffic vehicle check!");   // :842
        CGS_ASSERT(lVehicleClass >= 0 && lVehicleClass < BrnTraffic::E_VEHICLECLASS_COUNT,
                   "lVehicleClass >= 0 && lVehicleClass < BrnTraffic::E_VEHICLECLASS_COUNT");          // :845

        ETakedownType leTakedownType;
        switch (lVehicleClass)
        {
        case BrnTraffic::E_VEHICLECLASS_VAN: leTakedownType = E_TAKEDOWN_INTO_VAN; break;
        case BrnTraffic::E_VEHICLECLASS_BUS: leTakedownType = E_TAKEDOWN_INTO_BUS; break;
        case BrnTraffic::E_VEHICLECLASS_CAR:
        default:                             leTakedownType = E_TAKEDOWN_INTO_CAR; break;
        }
        return leTakedownType;
    }
}
