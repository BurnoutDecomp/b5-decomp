#include "GameSource/World/Bridges/WorldBridgeInputToEntityModules.h"

#include "GameSource/World/BrnWorldModule.h"                                                                  // BrnWorld::WorldModule (the typed r3 seat: GetLastCameraInput)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface (real aggregate)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"                        // VehicleDriverInputInterface::UpdateDriverEventQueue
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h"         // WorldEntityIO::RequestInterface (real type)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"             // BrnWorld::PlayerVehicleControls
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                                    // gpDebugPrint ([UI-gate] opt-in probe)
#include "GameSource/GameState/BrnGameActions.h"                                                              // [gateui] PrepareForModeAction (pulls BrnGameModeParams.h -> GameModeParams::GetFlag/GetGameModeType)

#include <stdlib.h>                                                                                           // getenv (BRN_PROP_DIAG, host-side diagnostic only)

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
    {
        const BrnWorld::PlayerVehicleControls* lpPlayerControls =
            reinterpret_cast<const BrnWorld::PlayerVehicleControls*>(
                lpWorldInput->GetPlayerVehicleControls());

        lpRaceCarInputBuffer_PreScene->SetPlayerVehicleControls(lpPlayerControls);
    }
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
    // The action-type ids come from the X360 jump table: 0x827AE5A4 `addi r11, r3, -0x17`
    // then a 177-entry table @0x827AE5C8, so case N == action id 23 + N (case 0 -> 23
    // PREPARE_FOR_MODE, case 16 -> 39 STOP_MODE, case 171 -> 194, case 176 -> 199).
    // ⚠️ FLAG (reported, not fixed -- shared header, other consumers): BrnGameActions.h spells
    // `E_ACTION_PREPARE_FOR_MODE = 19`, which is the DecFIGS/PS3 value. The X360 build's own
    // dispatcher pins 23 here, and RaceCarEntityModule's action table agrees
    // (BrnRaceCarEntityModule.cpp's "23 HandlePrepareForModeAction" comment). The X360 ids in
    // this range run DWARF+4. Nothing in this TU uses the enum, so the local constants stand.
    //
    // StopModeAction / PropSmashReportAction read only their leading word.
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
            // ⭐ [gateui] ROUND-8 FIX -- A LATENT OUT-OF-BOUNDS READ, repaired. Scope corrected
            // by the r8 verifier: this case has NO PRODUCER in the tree yet (all 2,428 AddEvent
            // call sites tabulated: nothing posts action 23/PREPARE_FOR_MODE), so it is DEAD at
            // run time today and is NOT a candidate explanation for the first-gate miss
            // (defect A) -- on the failing run-9 drive clause 3 was provably TRUE (the third
            // prop passed LEG 1, and nothing can flip the latch in between). The surviving
            // LEG-1 candidates are clause 1 (whole-word entity-id vs a race-car PART id),
            // KU_MOVED_BIT, and GetRespawnType()==E_RESPAWN (which would be console-CORRECT
            // rejection, not a defect); the ProcessContacts LEG1-REJECT probe names the culprit.
            //
            // What WAS wrong here, and stays fixed: the case read the action payload at RAW
            // X360 BYTE OFFSETS
            //     *(s32*)(action + 0x178)   the game-mode type
            //     *(u64*)(action + 0x890)   the 64-bit mode-flag word
            // On the x64 host PrepareForModeAction's layout deliberately diverges (its own
            // banner: parity is by NAMED MEMBER) and sizeof(PrepareForModeAction) is 1792 --
            // the +0x890 read was 400 BYTES PAST THE END of the record (verifier-measured).
            // The moment a producer lands, the old reads would have latched all three flags
            // from noise. AGENTS.md recurring-bug #1, the same class as the RaceCar-bridge
            // offsets this TU already converted at the top of the file.
            //
            // Re-routed through named members. THE MEMBERS, VERIFIED AGAINST THE ASM THIS PASS
            // (WorldModule::BridgeInputToEntityModules @0x827ADF88; r31 == the action record,
            // r19 == 1, r29 == 0, both pinned at 0x827AE09C / 0x827AE5A0):
            //
            //   0x827AE8C0  lwz   r11, 0(r31)          the prepare stage (first member, +0x00 on
            //   0x827AE8C4  cmpwi cr6, r11, 0          BOTH targets -- an empty GameAction<T> base
            //   0x827AE8C8  beq   -> true              contributes no bytes, so this ONE read is
            //   0x827AE8CC  cmpwi cr6, r11, 1          host-correct as-is and is left alone)
            //   0x827AE8D4  bne   -> false
            //
            //   0x827AE8E8  lwz   r11, 0x178(r31)      == mGameModeParams(+0x30) + 0x148
            //   0x827AE8EC  cmpwi cr6, r11, 0xA        SIGNED (E_MODE_NONE is -1)
            //   0x827AE8F4  bge   -> r11 = 1           -> GetGameModeType() >= E_MODE_ONLINE_MODE_START
            //   0x827AE908  stb   r19, 0x78D(r30)      SetIsOnline(true) / :910 stb r29 -> false
            //     (+0x148 is where GameModeParams::Construct @0x8231C374 stores its
            //      EGameModeType argument, which is what identifies the member)
            //
            //   0x827AE914  ld     r11, 0x890(r31)     == mGameModeParams(+0x30) + 0x860
            //   0x827AE918  rlwinm r11, r11, 0,3,3     32-bit mask, bit 3 from the MSB == 0x10000000
            //   0x827AE938  stb    r19, 0x78E(r30)     SetEasySmashProps(true) / :940 stb r29
            //     -> GetFlag(KU_FLAG_EASY_SMASH_PROPS == 0x10000000ull)
            //
            //   0x827AE944  li     r12, 1
            //   0x827AE948  ld     r11, 0x890(r31)     the SAME word, re-loaded
            //   0x827AE94C  extldi r12, r12, 64,32     r12 = 1 << 32 == 0x100000000
            //   0x827AE950  and    r11, r11, r12
            //   0x827AE96C  bne    -> 0x827AE978 stb r29, 0x78F(r30)   flag SET   -> store FALSE
            //               fallthrough 0x827AE970 stb r19, 0x78F(r30) flag CLEAR -> store TRUE
            //     -> SetPropProgressionEnabled(!GetFlag(KU_FLAG_DISABLE_PROP_PROGRESSION))
            //        (the polarity inversion is the console's, not ours)
            //
            //   0x827AE97C  stb    r19, 0x792(r30)     ResetProps(), unconditional
            //
            // +0x860 is GetFlag's own load offset (GameModeParams::GetFlag @0x821F2C88:
            // `ld r11, 0x860(r3) ; and r11, r11, r4`), so calling GetFlag IS the console's own
            // mechanism for both tests, not a re-spelling of them.
            //
            // reinterpret_cast (not static_cast): the tree models GameAction<T> as an empty
            // template spine that does NOT derive from CgsModule::Event, so the two are
            // unrelated types -- the same idiom the PROP_SMASH_REPORT case below already uses.
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPrepareForModeEvent =
                reinterpret_cast<const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(lpGameAction);

            // The stage word is the record's first member on both targets (see the asm block
            // above); mePrepareForModeStage is private and the X360 build inlined every stage
            // accessor, so it stays a leading-word read rather than gain an invented getter.
            const s32 liPrepareStage = *reinterpret_cast<const s32*>(lpGameAction);
            if (liPrepareStage == 0 || liPrepareStage == 1)
            {
                const BrnGameState::GameModeParams* lpGameModeParams =
                    lpPrepareForModeEvent->GetGameModeParams();

                const bool lbOnlineMode =                                              // :152
                    lpGameModeParams->GetGameModeType()
                        >= BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_START;
                const bool lbEasySmashProps =                                          // :155
                    lpGameModeParams->GetFlag(
                        BrnGameState::GameModeParams::KU_FLAG_EASY_SMASH_PROPS);
                const bool lbDisablePropProgession =                                   // :158
                    lpGameModeParams->GetFlag(
                        BrnGameState::GameModeParams::KU_FLAG_DISABLE_PROP_PROGRESSION);
                lpPropEntityInputBuffer_PreScene->SetIsOnline(lbOnlineMode);
                lpPropEntityInputBuffer_PreScene->SetEasySmashProps(lbEasySmashProps);
                lpPropEntityInputBuffer_PreScene->SetPropProgressionEnabled(!lbDisablePropProgession);
                lpPropEntityInputBuffer_PreScene->ResetProps();

                // [DIAG] NOT IN THE X360 BINARY. One-shot behind BRN_PROP_DIAG, the same idiom
                // and env guard as the rest of the `[UI-gate]` ladder. This is the rung that
                // tells the next reader whether the fix above actually changed anything: it
                // names the three latch values the console derives here, and prop progression
                // being FALSE at this rung is the direct explanation for a LEG-1 clause-3
                // rejection in ProcessContacts.
                {
                    // The env latch is evaluated ONCE (the house idiom -- getenv per event would
                    // be a syscall on a gameplay path).
                    static const bool sbPropDiag = (getenv("BRN_PROP_DIAG") != 0);
                    static bool sbLoggedFirstPrepareForMode = false;
                    if (sbPropDiag && !sbLoggedFirstPrepareForMode
                        && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLoggedFirstPrepareForMode = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[UI-gate] prepare-for-mode stage=" << liPrepareStage
                            << " modeType=" << static_cast<s32>(lpGameModeParams->GetGameModeType())
                            << " online="          << (lbOnlineMode ? 1 : 0)
                            << " easySmash="       << (lbEasySmashProps ? 1 : 0)
                            << " propProgression=" << (lbDisablePropProgession ? 0 : 1)
                            << "\n";
                    }
                }
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
            // ⭐ RETYPED 2026-08-12 (prop-spawn wave, agent B5) -- CONSEQUENTIAL EDIT.
            // This used to read the action's leading word as a `u32` and hand it to a
            // `SetHitPropsBitArray(u32)`. The word is a POINTER: BrnGameActions.h:5156
            // types the PropSmashReport action's first member
            // `const Profile::HitPropsBitArray* mpabHitPropBitArray`, the X360 bridge
            // stores it verbatim (`lwz r11,0(r31) ; stw r11,0x780(r30)`), and
            // PropEntityModule::PreSceneUpdate @0x82309A40 DEREFERENCES it
            // (`memcpy(&mZoneManager.maPreviouslyHitProps, *(lpInput+0x780), 37504)`)
            // behind the buffer's own "mpabHitPropBitArray != NULL" tripwire. On x64 a
            // u32 seat truncates the pointer, so the setter is now the DWARF's
            // `SetHitPropsBitArray(const HitPropsBitArray&)` and the word is read at
            // pointer width. Same raw-offset action-record access as the neighbouring
            // cases (the action records' own homes are still unreconstructed -- see the
            // FLAG above the switch).
            typedef BrnWorld::PropEntityIO::InputBuffer_PreScene::HitPropsBitArray HitPropsBitArray;
            const HitPropsBitArray* const* lppPropHitReport =
                reinterpret_cast<const HitPropsBitArray* const*>(lpGameAction);
            lpPropEntityInputBuffer_PreScene->SetHitPropsBitArray(**lppPropHitReport);
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
