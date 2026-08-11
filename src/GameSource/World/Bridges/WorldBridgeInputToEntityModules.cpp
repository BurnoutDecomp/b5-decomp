#include "GameSource/World/Bridges/WorldBridgeInputToEntityModules.h"

#include "GameSource/World/BrnWorldModule.h"                                                                  // BrnWorld::WorldModule (the typed r3 seat: GetLastCameraInput)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface (real aggregate)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"                        // VehicleDriverInputInterface::UpdateDriverEventQueue
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h"         // WorldEntityIO::RequestInterface (real type)

// WorldModule::BridgeInputToEntityModules / CheckForNetworkDriverControlsReceived --
// reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827ADF88 / 0x827A8C58 (this TU's
// DWARF home is WorldBridgeInputToEntityModules.cpp; signatures/locals/callee names
// verbatim from the PS3 DecFIGS BrnWorldBridgesUnity.cpp dump).
//
// Cross-home casts (FLAG): several UpdateInputBuffer payloads are modelled in
// BrnWorldModuleIO.h as local sized slices while the receiving setters take the
// canonical types (both model the SAME X360 type); each hand-off below
// reinterpret_casts slice -> canonical and is marked. Adopt the canonical types in
// BrnWorldModuleIO.h additively when its payload TUs land.

namespace WorldModule
{

namespace
{
    // The two WorldModule reads this bridge does through the r3 seat resolve BY NAME
    // (converted 2026-08-11 from raw X360 byte offsets -- the RaceCar-bridge bug class:
    // console offsets 6167744/6168064 do NOT land on these members under the x64
    // layout, boot-measured 67,504 bytes adrift for this region):
    //   +0x5E1CC0 (6167744) == mLastCameraInput            -> GetLastCameraInput()
    //   +0x5E1E00 (6168064) == mLastCameraInput + 0x140    -> its Camera::mState_uFlags
    // The "flag word" was never a WorldModule member at all: 0x5E1E00 - 0x5E1CC0 ==
    // 0x140 == the camera-state current-flag set inside the embedded camera (the low
    // word the committed Camera.h exposes as the mState_uFlags alias; the PS3 DWARF
    // accordingly shows NO member between mLastCameraInput and mEnvironmentMap).
    const s32 KI_CAMERA_STATE_FLAG_IN_HARD_STOP_CAMERA = 0x100;   // bit 0x100 of mState_uFlags

    // GameActionQueue event-type ids the bridge reacts to (the game-action enum's
    // own home is not reconstructed; ids pinned by the X360 jump table
    // @0x827AE5C8, actions named by the PS3 DWARF locals).
    const s32 KI_GAME_ACTION_PREPARE_FOR_MODE      = 23;    // PrepareForModeAction (lpPrepareForModeEvent)
    const s32 KI_GAME_ACTION_STOP_MODE             = 39;    // StopModeAction (lpStopModeEvent)
    const s32 KI_GAME_ACTION_SEND_PROP_PROGRESSION = 194;   // FLAG: action type name unrecovered (drives SendingPropProgression())
    const s32 KI_GAME_ACTION_PROP_SMASH_REPORT     = 199;   // PropSmashReportAction (lpPropHitReport)

    // The vehicle-driver update-queue event id CheckForNetworkDriverControlsReceived
    // reacts to (payload = BrnNetworkDriverControls, PS3 DWARF local :200).
    const s32 KI_DRIVER_UPDATE_EVENT_NETWORK_DRIVER_CONTROLS = 2;
}

// @ 0x827A8C58 -- WorldBridgeInputToEntityModules.cpp:183. Walks the world input's
// vehicle-driver update queue (VariableEventQueue<5040,16>; the X360 calls
// GetVehicleDriverInputInterface @0x827A3660 and the queue's out-of-line
// GetFirstEvent @0x822AFDD0 / GetNextEvent @0x822CADC8) and, for each
// network-driver-controls event, latches the per-race-car "received" flag.
// FLAG: BrnNetworkDriverControls' own home is not reconstructed; the only field
// this reads is its leading word (the EActiveRaceCarIndex), per the PS3 asm
// (`racecar byte[34 + word0] = 1` == SetReceivedNetworkDriverControls(word0)).
void CheckForNetworkDriverControlsReceived(
    void* lpWorldModule,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarInputBuffer_PreScene,
    const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    (void)lpWorldModule;

    const BrnPhysics::Vehicle::VehicleDriverInputInterface::UpdateDriverEventQueue* lpDriverQueue =
        lpWorldInput->GetVehicleDriverInputInterface()->GetUpdateDriverQueue();

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liId = lpDriverQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        if (liId == KI_DRIVER_UPDATE_EVENT_NETWORK_DRIVER_CONTROLS)
        {
            // lpControls (PS3 DWARF :200): leading word == the active-race-car index.
            const s32* lpControls = reinterpret_cast<const s32*>(lpEvent);
            lpRaceCarInputBuffer_PreScene->SetReceivedNetworkDriverControls(
                static_cast<EActiveRaceCarIndex>(*lpControls));
        }
        liId = lpDriverQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

// @ 0x827ADF88 -- WorldBridgeInputToEntityModules.cpp:46. The X360 tail returns the
// timer-status pointer as a register artifact; the logical return type is void.
void BridgeInputToEntityModules(
    void* lpWorldModule,
    BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene*  lpTriggerInput_PreScene,
    BrnWorld::TriggerEntityModuleIO::InputBuffer_PostScene* lpTriggerInput_PostScene,
    BrnTraffic::BrnTrafficIO::InputBuffer_PreScene*         lpTrafficInputBuffer_PreScene,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene*  lpRaceCarInputBuffer_PreScene,
    BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
    BrnWorld::WorldEntityIO::InputBuffer_PreScene*          lpWorldEntityInputBuffer_PreScene,
    BrnWorld::PropEntityIO::InputBuffer_PreScene*           lpPropEntityInputBuffer_PreScene,
    const BrnWorldIO::UpdateInputBuffer*                    lpWorldInput)
{
    // ---- traffic ----------------------------------------------------------------
    // FLAG cross-home cast: BrnWorldIO models the timer-status payload as a local 48-byte POD
    // (BrnWorldIO::TimerStatusInterface), while the traffic pre-scene buffer's member is the real
    // CgsSystem::TimerStatusInterface (two CgsSystem::TimerStatus, 48B). Same X360 payload, distinct
    // reconstructed homes; reinterpret across them (mirrors the network-interface cast below).
    lpTrafficInputBuffer_PreScene->SetTimerStatusInterface(
        reinterpret_cast<const CgsSystem::TimerStatusInterface*>(
            lpWorldInput->GetTimerStatusInterface()));
    // FLAG cross-home cast: BrnWorldIO models the traffic-network payload locally.
    lpTrafficInputBuffer_PreScene->SetTrafficNetworkInputInterface(
        reinterpret_cast<const BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface*>(
            lpWorldInput->GetTrafficNetworkInterface()));

    // ---- race car (pre-scene scalars/interfaces) ---------------------------------
    // FLAG cross-home cast: RaceCarEntityModuleIO's TimerStatusInterface is a
    // pointer-held incomplete type; BrnWorldIO models the same X360 payload locally.
    lpRaceCarInputBuffer_PreScene->SetTimerStatusInterface(
        reinterpret_cast<const BrnWorld::RaceCarEntityModuleIO::TimerStatusInterface*>(
            lpWorldInput->GetTimerStatusInterface()));
    // FLAG cross-home cast: BrnWorld::PlayerVehicleControls vs the BrnWorldIO slice.
    lpRaceCarInputBuffer_PreScene->SetPlayerVehicleControls(
        reinterpret_cast<const BrnWorld::PlayerVehicleControls*>(
            lpWorldInput->GetPlayerVehicleControls()));
    lpRaceCarInputBuffer_PreScene->SetActivePaybackType(lpWorldInput->GetActivePaybackType());
    lpRaceCarInputBuffer_PreScene->SetActivePaybackAggressor(lpWorldInput->GetActivePaybackAggressor());
    lpRaceCarInputBuffer_PreScene->SetReplayStatusInterface(lpWorldInput->GetReplayStatusInterface());

    // ---- prop (replay status) -----------------------------------------------------
    lpPropEntityInputBuffer_PreScene->SetReplayStatusInterface(lpWorldInput->GetReplayStatusInterface());

    // ---- race car (per-active-race-car colour/paint/contact/select latches) --------
    // The range asserts live inside the header-inline getters (BrnWorldModuleIO.h)
    // and setters (BrnRaceCarEntityModuleIO.h); the loop-guard assert
    // ("leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT", BurnoutConstants.h:39) lives
    // in EActiveRaceCarIndex's committed range-guarded operator++.
    for (EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++)
    {
        if (lpWorldInput->IsRaceCarColourIndexValid(leActiveRaceCarIndex))
        {
            lpRaceCarInputBuffer_PreScene->SetRaceCarColourIndex(
                leActiveRaceCarIndex, lpWorldInput->GetRaceCarColourIndex(leActiveRaceCarIndex));
        }
        if (lpWorldInput->IsRaceCarPaintFinishValid(leActiveRaceCarIndex))
        {
            lpRaceCarInputBuffer_PreScene->SetRaceCarPaintFinishIndex(
                leActiveRaceCarIndex, lpWorldInput->GetRaceCarPaintFinishIndex(leActiveRaceCarIndex));
        }
        if (lpWorldInput->GetLostContact(leActiveRaceCarIndex))
            lpRaceCarInputBuffer_PreScene->SetLostContact(leActiveRaceCarIndex);
        if (lpWorldInput->GetRegainedContact(leActiveRaceCarIndex))
            lpRaceCarInputBuffer_PreScene->SetRegainedContact(leActiveRaceCarIndex);
        if (lpWorldInput->IsCarSelectStatusValid(leActiveRaceCarIndex))
        {
            lpRaceCarInputBuffer_PreScene->SetCarSelectStatus(
                leActiveRaceCarIndex, lpWorldInput->GetCarSelectStatus(leActiveRaceCarIndex));
        }
    }

    // ---- race car (camera + network driver controls) -------------------------------
    // The director camera is the WorldModule's mLastCameraInput (X360 +0x5E1CC0),
    // reached by name through the typed seat (see the namespace note above).
    const BrnWorld::WorldModule* lpModule = static_cast<const BrnWorld::WorldModule*>(lpWorldModule);
    lpRaceCarInputBuffer_PreScene->SetCameraInput(lpModule->GetLastCameraInput());
    CheckForNetworkDriverControlsReceived(lpWorldModule, lpRaceCarInputBuffer_PreScene, lpWorldInput);

    // ---- race car (pre-physics) -----------------------------------------------------
    // FLAG cross-home casts: the takedown/scoring/online-scoring payloads are local
    // BrnWorldIO slices; the receiving typedefs are the canonical homes.
    lpRaceCarInputBuffer_PrePhysics->SetTakedownEventQueue(
        reinterpret_cast<const BrnWorld::RaceCarEntityModuleIO::TakedownEventQueue*>(
            lpWorldInput->GetTakedownEventQueue()));
    lpRaceCarInputBuffer_PrePhysics->SetScoringInterface(
        reinterpret_cast<const BrnGameState::GameStateModuleIO::ScoringOutputInterface*>(
            lpWorldInput->GetScoringInterface()));
    lpRaceCarInputBuffer_PrePhysics->SetOnlineScoringInterface(
        reinterpret_cast<const BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface*>(
            lpWorldInput->GetOnlineScoringInterface()));
    lpRaceCarInputBuffer_PrePhysics->SetControllerActive(lpWorldInput->GetControllerActive());
    // The hard-stop flag is bit 0x100 of the embedded camera's state-flag low word
    // (X360 +0x5E1E00 == mLastCameraInput + 0x140 == Camera::mState_uFlags; see the
    // namespace note above).
    lpRaceCarInputBuffer_PrePhysics->SetInHardStopCamera(
        (lpModule->GetLastCameraInput()->mState_uFlags & KI_CAMERA_STATE_FLAG_IN_HARD_STOP_CAMERA) != 0);

    // ---- trigger management (add + remove queues) -------------------------------------
    // Both sides are now the real BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface
    // (the world input buffer's member and the trigger pre-scene buffer's member were both
    // retyped onto it -- see their Construct notes), so the two cross-home reinterpret_casts
    // this bridge used to need are gone. The X360 Appends the embedded
    // VariableEventQueue<131072,16> then the InRemoveTriggerEvent queue @ +131088, which is
    // exactly the aggregate's own Append.
    lpTriggerInput_PreScene->GetInputInterface()->Append(
        *lpWorldInput->GetTriggerManagementInputInterface());

    // ---- trigger query queue ------------------------------------------------------------
    lpTriggerInput_PostScene->GetQueryInputInterface()->Append(
        *lpWorldInput->GetTriggerQueryInputInterface());

    // ---- world entity request interface --------------------------------------------------
    // The world input buffer's member is now the committed
    // BrnWorld::WorldEntityIO::RequestInterface itself (X360 GetWorldEntityRequestI
    // @0x823B4D48 returns this+320273 and Construct @0x827C9E90 zeroes exactly the two
    // flag bytes there), so the cross-home cast is retired.
    lpWorldEntityInputBuffer_PreScene->AppendRequestInterface(
        *lpWorldInput->GetWorldEntityRequestInterface());

    // ---- prop (game-action fan-out) --------------------------------------------------------
    // FLAG: the action payloads are read at raw offsets (the action records' own homes
    // are not reconstructed): PrepareForModeAction word0 == the prepare stage (only
    // stages 0/1 latch); GetGameModeParams(): game-mode id @ +0x178 (ids >= 10 are the
    // online modes -> lbOnlineMode), u64 flags @ +0x890 (bit 0x10000000 ->
    // lbEasySmashProps, bit 0x100000000 -> lbDisablePropProgession). StopModeAction /
    // PropSmashReportAction read only their leading word.
    // (no queue local -- the X360 re-fetches GetGameActionQueue() around each
    // GetFirst/GetNextEvent call, matching the DWARF local set :108-:110)
    const CgsModule::Event* lpGameAction = 0;     // :108
    s32 liEventSize = 0;                          // :109
    s32 liType = lpWorldInput->GetGameActionQueue()->GetFirstEvent(&lpGameAction, &liEventSize);   // :110
    while (lpGameAction)
    {
        switch (liType)
        {
        case KI_GAME_ACTION_PREPARE_FOR_MODE:     // :148 lpPrepareForModeEvent
        {
            const u8* lpPrepareForModeEvent = reinterpret_cast<const u8*>(lpGameAction);
            const s32 liPrepareStage = *reinterpret_cast<const s32*>(lpPrepareForModeEvent);
            if (liPrepareStage == 0 || liPrepareStage == 1)
            {
                const bool lbOnlineMode =                                              // :152
                    *reinterpret_cast<const s32*>(lpPrepareForModeEvent + 0x178) >= 10;
                const u64 luModeFlags = *reinterpret_cast<const u64*>(lpPrepareForModeEvent + 0x890);
                const bool lbEasySmashProps        = (luModeFlags & 0x10000000ull) != 0;   // :155
                const bool lbDisablePropProgession = (luModeFlags & 0x100000000ull) != 0;  // :158
                lpPropEntityInputBuffer_PreScene->SetIsOnline(lbOnlineMode);
                lpPropEntityInputBuffer_PreScene->SetEasySmashProps(lbEasySmashProps);
                lpPropEntityInputBuffer_PreScene->SetPropProgressionEnabled(!lbDisablePropProgession);
                lpPropEntityInputBuffer_PreScene->ResetProps();
            }
            break;
        }
        case KI_GAME_ACTION_STOP_MODE:            // :133 lpStopModeEvent
        {
            // NOTE (asm-pinned, the pseudocode mis-decompiled this case): IsOnline is
            // cleared ONLY when the stopped mode id is an online one (>= 10);
            // EasySmashProps clears and PropProgressionEnabled sets unconditionally.
            const s32* lpStopModeEvent = reinterpret_cast<const s32*>(lpGameAction);
            if (*lpStopModeEvent >= 10)
                lpPropEntityInputBuffer_PreScene->SetIsOnline(false);
            lpPropEntityInputBuffer_PreScene->SetEasySmashProps(false);
            lpPropEntityInputBuffer_PreScene->SetPropProgressionEnabled(true);
            break;
        }
        case KI_GAME_ACTION_SEND_PROP_PROGRESSION:
            lpPropEntityInputBuffer_PreScene->SendingPropProgression();
            break;
        case KI_GAME_ACTION_PROP_SMASH_REPORT:    // :125 lpPropHitReport
        {
            const u32* lpPropHitReport = reinterpret_cast<const u32*>(lpGameAction);
            lpPropEntityInputBuffer_PreScene->SetHitPropsBitArray(*lpPropHitReport);
            break;
        }
        default:
            break;
        }
        liType = lpWorldInput->GetGameActionQueue()->GetNextEvent(lpGameAction, &lpGameAction, &liEventSize);
    }

    // ---- prop (current timestep) --------------------------------------------------------
    // X360 tail (@0x827AE9B4): timer-status f32[7] * f32[8] == the sim timer's
    // timestep * multiplier (PS3: GetSimTimerStatus().GetCurrentTimeStep()).
    const BrnWorldIO::TimerStatusInterface* lpTimerStatus = lpWorldInput->GetTimerStatusInterface();
    lpPropEntityInputBuffer_PreScene->SetCurrentTimestep(
        lpTimerStatus->maData[8] * lpTimerStatus->maData[7]);
}

}
