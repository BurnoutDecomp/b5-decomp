// b5-decomp/src/GameSource/GameState/GameStateModule_gUI_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the rest
// of the module's committed bodies are in BrnGameStateModule.cpp).
//
// THE gateui WAVE'S GameState-SIDE PLUMBING -- the bodies that carry a broken prop from the
// world module all the way to a game action the GUI bridge can translate:
//
//     world OutputBuffer::GetGameEventQueue()            (produced by PropEntityModule::
//                                                         ProcessContacts -> the bridge legs)
//        -> GameStateModule::PostWorldUpdate             (GameStateModule_wW_01.cpp: refreshes
//                                                         mLastActiveRaceCarInterface, appends
//                                                         into mGameEventCarryQueue, runs the
//                                                         mode manager's scorers and HUD pump)
//        -> PreWorldUpdateStuntBringUp                   (X360 PreWorldUpdate @0x823A5328)
//             * ProcessGameEvents                        (GameStateModule_ProcessGameEvents.cpp,
//                                                         case 111 -> StuntManager::OnPropHit)
//             * TriggerQueryManager::UpdateTriggers      (arms maActiveTriggers for NEXT frame)
//             * StuntManager::Update                     (consumes the latch ->
//                                                         ProcessStuntElement -> action 58)
//
// Each function's console attestation, and each deliberate deviation, is written out at its
// declaration in BrnGameStateModule.h and again at its body below. Nothing here is fabricated:
// every reduction is named as a reduction.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv ([UI-gate] diag)
#include <string.h>                                                     // memcpy / memset (the case-20 payload view)
#include <stdio.h>                                                      // optional harness ready-file gate

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16>

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer (lock + GetGameActionQueue)
#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h" // current player-status snapshot
#include "GameSource/GameState/BrnGameStateModule.h"             // mTakedownEventQueue (the post-world scoring leg)
#include "GameSource/GameState/BrnGameEvents.h"                         // RecordPropHitEvent / E_EVENT_RECORD_PROP_HIT / E_EVENT_CHANGE_WORLD_REGION
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // WorldRegionChangeEvent (the case-115 payload)
#include "GameSource/GameState/Offences/BrnStuntManager.h"              // StuntManager::OnPropHit / Update
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // UpdateTriggers / GetActiveTrigger*
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // the accessor's return type
#include "GameSource/GameState/BrnGameActions.h"                        // RankInfoResponseAction (the case-80 record)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"            // GameStats (the case-79 record)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetGameStats
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::GetProgressionRankCount
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::GetNumRankWinsForGameMode
#include "GameSource/GameState/Progression/BrnDerivedCars.h"

#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::GetRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"                       // TriggerRegion::GetType
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // GenericRegion::Type (SMASH / OVERDRIVE_BOOST)

// ---- [D4 stuntrace WAVE D -- THE PUMP] ------------------------------------------------------
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::Pre/PostWorldUpdate, StartGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::GetCurrentState / GetIntroDurationSeconds
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // StartGameModeParams (the case-20 local)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatusInterface (the pump's new argument)
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDCompress ([car] BRN_DEBUG_PLAYER_CAR)
#include "SharedClasses/DataLists/VehicleList.h"                        // VehicleList::GetVehicleCount/GetVehicleData ([car])
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // VehicleListEntry::GetId/GetName/GetDefaultWheelName ([car])
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"          // CgsResource::ID::HashString ([car-audio] audit)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"   // CarData::GetId ([car-audio] junkyard pick)
#include "SharedClasses/DataLists/WheelList.h"                          // WheelList::FindWheelIndexFromName/GetWheelData ([car])
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h" // [FX-GS2 G10-D11] GameStateToGuiInterface::AddOnTailEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // [FX-FLOW NEW-EMMTAIL] GameStateToNetworkInterface::SetActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // [FX-GS2 G10-D11] GetUsedCarsBitArray / GetRaceCar
#include "rw/math/vpu/vector3_operation.h"                              // [FX-GS2 G10-D11] Magnitude (the player's speed)
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // the case-17/18 arms
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::ForceUnpause (case 17)
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"     // TakedownManager::ClearAllTakedowns (case 18)
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"       // MugshotManager::OnRoundStart (case 18)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"       // PaybackManager::OnRoundStart (case 18)
namespace BrnGameState
{

// ARTIST 0x82397568: update progression, reset the model at the current location,
// then apply the selected car's saved palette and colour.
void GameStateModule::HandleChangePlayerCarEvent(
    const GameStateModuleIO::ChangePlayerCarEvent* lpEvent,
    GameStateModuleIO::GameActionQueue* lpActions)
{
    OnPlayerCarChange(lpEvent->mCarModelId, lpEvent->mWheelModelId, lpActions, true);
    GameStateModuleIO::ResetPlayerCarAction lReset = GameStateModuleIO::ResetPlayerCarAction();
    lReset.mCarModelId = lpEvent->mCarModelId;
    lReset.mWheelModelId = lpEvent->mWheelModelId;
    lReset.mePlayerScoringIndex = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
    lReset.muReserved0x42 = lpEvent->mbResetPlayerCamera;
    lReset.mbKeepResetSection = lpEvent->mbKeepResetSection;
    lReset.mfDeformationAmount = mProgressionManager.GetProfile()->GetPlayerBaseDeformAmount(lpEvent->mCarModelId);
    lReset.miBaseDeformationType = lReset.mfDeformationAmount > 0.0f ? 1 : -1;
    lpActions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReset),
                       GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR, sizeof(lReset));
    GameStateModuleIO::CarSelectChangeColourAction lColour;
    s32 liColour, liPalette;
    mProgressionManager.GetCarColourAndPalette(lpEvent->mCarModelId, &liColour, &liPalette);
    lColour.muPaletteIndex = liPalette;
    lColour.muColourIndex = liColour;
    lpActions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lColour),
                       GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR, sizeof(lColour));
}

// ============================================================================
// â­ [gateui] GetDeveloperChallengeManager -- the body behind the declaration the StreetManager
// wave added with no member behind it.
//
// The console never emits an accessor for this subobject: every call site reaches it through the
// inlined `mpGameStateModule + 185712` pointer adjust, and both of them assert it non-null with
// the accessor spelled out --
//   StreetManager::ProcessNewRoadScore      @0x823496C8 ("mpGameStateModule->GetDeveloperChallengeManager()")
//   StuntManager::ProcessStuntElement       @0x8239CDB0 (same string, BrnStuntManager.cpp)
// De-inlined here over the real embedded member (BrnGameStateModule.h,
// mDeveloperChallengeManager), so no reconstructed body has to poke a byte offset.
// ============================================================================
DeveloperChallengeManager* GameStateModule::GetDeveloperChallengeManager()
{
    return &mDeveloperChallengeManager;
}

// ============================================================================
// â­ [P1 sim-pause] PostWorldInput -- the free-function accessor BridgeGuiToGameState posts
// through (console: returns the module's post-world input GameEventQueue). PC body: the CARRY
// QUEUE, the named reduction spelled out at the declaration (BrnGameStateModule.h).
// ============================================================================
namespace GameStateModuleIO
{
    CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule)
    {
        return &lpModule->mGameEventCarryQueue;
    }
}

// ==============================================================================================
// [FX-GS2 2026-09-23, crash-parity G10-D11] The rival-tailing clock and its per-frame check.
//
// GetRivalTailingTime / SetRivalTailingTime: CheckForTailingRivals keeps one f32 per race-car
// slot at gsm+0x32D90 + 4 * slot (r27, stepped by `addi r27, r27, 4` @0x82376384 for EVERY slot,
// the player's included). The DWARF array is f32[7] (BrnGameStateModule.h:792), so slot 7 lands on
// the next word, muNetworkGameRandomSeed (gsm+0x32DAC). That aliasing is the console's; it is
// routed here through the seed word's bits rather than by indexing past the array.
// ==============================================================================================
f32 GameStateModule::GetRivalTailingTime(s32 liRaceCarIndex) const
{
    if (liRaceCarIndex < 7)
    {
        return mafRivalTailingTimes[liRaceCarIndex];
    }
    f32 lfTime;
    memcpy(&lfTime, &muNetworkGameRandomSeed, sizeof(lfTime));   // slot 7 == gsm+0x32DAC
    return lfTime;
}

void GameStateModule::SetRivalTailingTime(s32 liRaceCarIndex, f32 lfTime)
{
    if (liRaceCarIndex < 7)
    {
        mafRivalTailingTimes[liRaceCarIndex] = lfTime;
        return;
    }
    memcpy(&muNetworkGameRandomSeed, &lfTime, sizeof(lfTime));   // slot 7 == gsm+0x32DAC
}

// ----------------------------------------------------------------------------------------------
// CheckForTailingRivals @0x82375F90 (DWARF BrnGameStateModule.h:808; sole caller PreWorldUpdate
// @0x823A5328, `bl` @0x823A57A0). For each rival sitting just behind the player at speed, run a
// clock; at 3 s, post the GUI "on your tail" record and restart it.
//     0x82375FBC..0x82375FEC  mode type (ModeManager +0xD94) == 0 (offline race), or the current
//                             mode (+0xD98) is online (+0xAC) -- else return
//     0x82375FF0..0x82376000  meControllerState (gsm+0x38B64) == 3 -- else return
//     0x82376004..0x823760F8  the player's used-car bit (the bit array's "invalid index" assert)
//     0x823760FC..0x8237619C  |RaceCarState.mLinearVelocity (+0x330)| > 50.0 (flt_820138DC):
//                             vmsum3fp128 + vrsqrtefp + two Newton steps (0 when the square is 0);
//                             the exact root here, this tree's convention (BrnMathUtils.h)
//     0x823761A0..0x823761DC  assert "lpScoringSystem != NULL" (line 0x1949); the player's
//                             GetRaceCarDistanceToFinish
//     loop slot 0..7 (the enum ++ carries BurnoutConstants.h:39's range assert):
//       0x82376220  the player's slot: skipped (its clock untouched)
//       0x82376234  player < rival (fcmpu/bge) and !((player + 20.0 (flt_820054CC)) < rival)
//                   (fcmpu/blt) -- the rival is BEHIND the player by at most 20 m
//       0x823762DC  the rival's used-car bit ; 0x82376318 RaceCarState.mfSpeedMPH (+0x3CC) > 50.0
//       0x82376328  all true: clock += the time step
//       0x82376338  otherwise `fsel f0, f0, 0.0, f0`: a clock >= 0 resets to 0, a negative or NaN
//                   one is kept
//       0x82376344  !(clock < 3.0 (flt_8202AC20)) (a NaN passes too): AddOnTailEvent(GetRivalId(slot)
//                   from the module's active-car snapshot (gsm+0x397E0), slot) through the
//                   write-locked GetGameStateToGuiInterface, then the clock = 0.0 (0x8237637C)
// Because of the slot-7 aliasing, ClearData's 0xFFFFFFFF seed reads as a NaN clock: on the first
// gated frame in which the player is not in slot 7 the console posts one record for slot 7 and the
// seed word becomes 0.0f. That is the console's arithmetic and it is kept.
// ----------------------------------------------------------------------------------------------
void GameStateModule::CheckForTailingRivals(GameStateModuleIO::OutputBuffer* lpOutput,
                                            const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput,
                                            f32 lfTimeStep)
{
    if (mModeManager.GetCurrentGameModeType() != GameStateModuleIO::E_MODE_OFFLINE_RACE)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode == 0 || !lpCurrentGameMode->IsOnline())
        {
            return;
        }
    }

    if (meControllerState != E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE)
    {
        return;
    }

    if (!lpVehicleOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(GetPlayerActiveRaceCarIndex())))
    {
        return;
    }

    const f32 lfPlayerSpeed = rw::math::vpu::Magnitude(
        lpVehicleOutput->GetRaceCar(static_cast<u32>(GetPlayerActiveRaceCarIndex()))->mLinearVelocity);
    if (!(lfPlayerSpeed > 50.0f))
    {
        return;
    }

    const ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
    CGS_ASSERT(lpScoringSystem != NULL, "lpScoringSystem != NULL");

    const f32 lfPlayerDistanceToFinish =
        lpScoringSystem->GetRaceCarDistanceToFinish(GetPlayerActiveRaceCarIndex());

    for (::EActiveRaceCarIndex leIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        if (GetPlayerActiveRaceCarIndex() == leIndex)
        {
            continue;
        }

        const f32 lfDistanceToFinish = lpScoringSystem->GetRaceCarDistanceToFinish(leIndex);
        f32 lfTime = GetRivalTailingTime(static_cast<s32>(leIndex));

        if (lfPlayerDistanceToFinish < lfDistanceToFinish
            && !((lfPlayerDistanceToFinish + 20.0f) < lfDistanceToFinish)
            && lpVehicleOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(leIndex))
            && lpVehicleOutput->GetRaceCar(static_cast<u32>(leIndex))->mfSpeedMPH > 50.0f)
        {
            lfTime = lfTime + lfTimeStep;
        }
        else
        {
            lfTime = (lfTime >= 0.0f) ? 0.0f : lfTime;   // fsel
        }
        SetRivalTailingTime(static_cast<s32>(leIndex), lfTime);

        if (!(lfTime < 3.0f))
        {
            lpOutput->GetGameStateToGuiInterface()->AddOnTailEvent(
                mLastActiveRaceCarInterface.GetRivalId(leIndex), leIndex);
            SetRivalTailingTime(static_cast<s32>(leIndex), 0.0f);

        }
    }
}

}
