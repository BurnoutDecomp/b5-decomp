// ============================================================================
// b5-decomp/src/GameSource/Game/BrnGameModule_wN1_02.cpp
//
// Network wave N1, partfile 02 of BrnGameModule.cpp: the two NETWORK legs of the per-frame
// update cascade and the network -> game-state / world bridges they feed.
//
//   BrnGame::BrnGameModule::DoUpdate_NetworkPreSim
//   BrnGame::BrnGameModule::DoUpdate_NetworkPostSim
//   BrnGame::BrnGameModule::BridgeNetworkToGameState      (original home GameBridgeNetworkToX.cpp)
//   BrnGame::BrnGameModule::BridgeNetworkToWorld          (original home GameBridgeNetworkToX.cpp)
//   BrnGame::BrnGameModule::TranslateNetworkEventsToWorld (original home GameBridgeNetworkToX.cpp)
//
// The update legs drive mNetworkModule, so this file needs the REAL BrnNetwork::BrnNetworkModule.
// ⛔ It does NOT compile while BrnGameModule.hpp still defines its empty placeholder
// `class BrnNetworkModule` (C2011: two definitions of one class in this TU). It compiles as
// soon as the swap wave replaces that placeholder line with
// `#include "GameSource/Network/BrnNetworkModule.h"`. Do not mount it before then.
// BridgeNetworkToWorld also needs BrnWorldModuleIO.h's TrafficNetworkInputInterface to be the
// real BrnTraffic::BrnTrafficIO type (its SetTrafficNetworkInterface takes the network output's).
//
// On the console DoUpdate calls NetworkPreSim right after DoUpdate_InputPreWorld and
// NetworkPostSim right after DoUpdate_GameStatePostWorld, immediately before UpdateTimers.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"         // CgsModule::IOHelper<T>
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"            // CgsModule::LockBuffersForIO / UnlockBuffersForIO
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // VariableEventQueue<18432,16>::Append
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"    // CgsInput::InputIO::OutputBuffer / PadOutputInformation
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"               // CgsGui::CgsGuiModuleIO::OutputBuffer
#include "GameSource/GameState/BrnGameStateModuleIO.h"               // BrnGameState::GameStateModuleIO::OutputBuffer
#include "GameSource/World/BrnWorldModuleIO.h"                       // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/Network/BrnNetworkModule.h"                     // BrnNetwork::BrnNetworkModule (the real module)
#include "GameSource/Network/BrnNetworkModuleIO.h"                   // Pre/PostSimulationInputBuffer, OutputBuffer
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"           // the network OUT-event records the world translator reads
#include "GameSource/GameState/BrnGameActions.h"                     // LocalPlayer*Action / RestartTrafficAction
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"        // BrnNetHarnessPC::Update (the LAN test harness hook)

#include <cstring>   // std::memcpy (the restart-traffic hull copy)

namespace BrnGame
{
    // The online latch DoUpdate_NetworkPreSim publishes next to mbOnline. Its reader is
    // QueryRequestDoStepFrame, which skips the debug step-frame pad chord while it is set;
    // that function is not in the tree yet, so today this is written and not read.
    static bool _gbHACKIsOnline = false;

    // =========================================================================================
    // DoUpdate_NetworkPreSim
    //
    // Console body: the two network perf monitors bracket everything but the buffer's release;
    // an IOHelper carves this sub-step's PreSimulationInputBuffer "NetworkPreSim" off the INPUT
    // stack; under LockBuffersForIO(pre-sim input <- input output) it stages player 0's
    // controller port and pad-idle byte (only when player 0 has a pad), the game-timer status
    // snapshot and the system-menu flag; then the module's ProcessBeforeSimulation runs with
    // both stacks, the buffer, the network OUTPUT buffer and the update set, and its "in a
    // network game" answer is latched into mbOnline and _gbHACKIsOnline.
    // =========================================================================================
    void BrnGameModule::DoUpdate_NetworkPreSim(CgsModule::IOBufferStack* lpInputBufferStack,
                                               CgsModule::IOBufferStack* lpOutputBufferStack,
                                               const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
                                               BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutputBuffer,
                                               BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_NetworkAIRaceCar);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Network);

        CgsModule::IOHelper<BrnNetwork::BrnNetworkModuleIO::PreSimulationInputBuffer> lNetworkInput(
            lpInputBufferStack, "NetworkPreSim");
        BrnNetwork::BrnNetworkModuleIO::PreSimulationInputBuffer* lpNetworkInput = lNetworkInput;

        CgsModule::LockBuffersForIO(lpNetworkInput, lpInputOutputBuffer);

        s32 liControllerPort = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPadInfo =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liControllerPort);
        if (lpPadInfo != 0)
        {
            lpNetworkInput->SetControllerPort(liControllerPort);
            // +0x3A0 of the pad record is the reference's mbPadIdle (the tree spells it
            // mbDisconnected).
            lpNetworkInput->SetPadIdle(lpPadInfo->mbDisconnected != 0);
        }
        lpNetworkInput->SetTimerStatusInterface(&mTimerStatusInterface);
        lpNetworkInput->SetSysMenuOnScreen(mbIsSysMenuShowing);

        CgsModule::UnlockBuffersForIO(lpNetworkInput, lpInputOutputBuffer);

        mbOnline = mNetworkModule.ProcessBeforeSimulation(lpInputBufferStack, lpOutputBufferStack,
                                                          lpNetworkInput, lpNetworkOutputBuffer, lUpdateSet);
        _gbHACKIsOnline = mbOnline;

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Network);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_NetworkAIRaceCar);
    }

    // =========================================================================================
    // DoUpdate_NetworkPostSim
    //
    // Console body: the same monitor pair; an IOHelper carves a PostSimulationInputBuffer
    // "NetworkPostSim" off the INPUT stack; under LockBuffersForIO(post-sim input <- game-state,
    // world and GUI outputs):
    //   BridgeWorldToNetwork (skipped while the update set carries 0x20, the boot-video frames),
    //   BridgeGameStateToNetwork,
    //   the GUI out-event queue bulk-appended into the buffer's GUI queue,
    //   TranslateGuiEventsToNetworkEvents (GUI out-events -> the buffer's network queue);
    // all locks released, then the module's ProcessAfterSimulation with both stacks, the buffer
    // and the update set.
    // =========================================================================================
    void BrnGameModule::DoUpdate_NetworkPostSim(CgsModule::IOBufferStack* lpInputBufferStack,
                                                CgsModule::IOBufferStack* lpOutputBufferStack,
                                                const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutputBuffer,
                                                const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer,
                                                const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
                                                BrnUpdateSet lUpdateSet)
    {
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_NetworkAIRaceCar);
        CgsDev::PerfMonCpu::StartMonitor(mCpuMonitors.miUT_Network);

        CgsModule::IOHelper<BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer> lNetworkInput(
            lpInputBufferStack, "NetworkPostSim");
        BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer* lpNetworkInput = lNetworkInput;

        CgsModule::LockBuffersForIO(lpNetworkInput, lpGameStateOutputBuffer, lpWorldOutputBuffer, lpGuiOutputBuffer);

        if ((lUpdateSet & 0x20) == 0)
        {
            BridgeWorldToNetwork(lpNetworkInput, lpWorldOutputBuffer);
        }
        BridgeGameStateToNetwork(lpNetworkInput, lpGameStateOutputBuffer);

        lpNetworkInput->GetGuiEventQueue()->Append(*lpGuiOutputBuffer->GetOutEventQueue());
        // [PC HARNESS, not console code] the two-instance LAN test harness posts its next GUI event into
        // the same queue; inert unless BRN_NET_HOST or BRN_NET_JOIN is set.
        BrnNetHarnessPC::Update(lpNetworkInput->GetGuiEventQueue());
        TranslateGuiEventsToNetworkEvents(lpNetworkInput->GetNetworkEventQueue(),
                                          lpGuiOutputBuffer->GetOutEventQueue());

        CgsModule::UnlockBuffersForIO(lpNetworkInput, lpGameStateOutputBuffer, lpWorldOutputBuffer, lpGuiOutputBuffer);

        mNetworkModule.ProcessAfterSimulation(lpInputBufferStack, lpOutputBufferStack, lpNetworkInput, lUpdateSet);

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Network);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_NetworkAIRaceCar);
    }

    // =========================================================================================
    // BridgeNetworkToGameState
    //
    // Called by DoUpdate_GameStatePreWorld inside the pre-world input's write lock and a read
    // lock on the network output. Console body, in order: the two argument asserts; append the
    // network output's game-event queue and its takedown queue into the pre-world input (each
    // after asserting its getter); assert the three interface getters; merge the
    // NetworkToGameStateInterface, install the player-status and player-results interfaces;
    // store the invites-open byte; then TranslateNetworkEventsToGameEvents.
    // =========================================================================================
    void BrnGameModule::BridgeNetworkToGameState(BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpGameStateInput,
                                                 const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput)
    {
        CGS_ASSERT(lpGameStateInput, "lpGameStateInput");
        CGS_ASSERT(lpNetworkOutput, "lpNetworkOutput");

        CGS_ASSERT(lpNetworkOutput->GetGameEventQueue(), "lpNetworkOutput->GetGameEventQueue()");
        lpGameStateInput->GetGameEventQueue()->Append(*lpNetworkOutput->GetGameEventQueue());

        CGS_ASSERT(lpNetworkOutput->GetTakedownEventOutputQueue(), "lpNetworkOutput->GetTakedownEventOutputQueue()");
        lpGameStateInput->GetTakedownEventInputQueue()->Append(*lpNetworkOutput->GetTakedownEventOutputQueue());

        CGS_ASSERT(lpNetworkOutput->GetNetworkToGameStateInterface(), "lpNetworkOutput->GetNetworkToGameStateInterface()");
        CGS_ASSERT(lpNetworkOutput->GetInGamePlayerStatusInterface(), "lpNetworkOutput->GetInGamePlayerStatusInterface()");
        CGS_ASSERT(lpNetworkOutput->GetPlayerResultsInterface(), "lpNetworkOutput->GetPlayerResultsInterface()");

        lpGameStateInput->AppendGetNetworkToGameStateInterface(lpNetworkOutput->GetNetworkToGameStateInterface());
        lpGameStateInput->SetPlayerStatusInterface(lpNetworkOutput->GetInGamePlayerStatusInterface());
        lpGameStateInput->SetNetworkPlayerResultsInterface(lpNetworkOutput->GetPlayerResultsInterface());
        lpGameStateInput->SetInvitesOpen(lpNetworkOutput->AreInvitesOpen());

        TranslateNetworkEventsToGameEvents(lpGameStateInput, lpNetworkOutput);
    }

    // =========================================================================================
    // BridgeNetworkToWorld
    //
    // Called by DoUpdate_World between BridgeControllerToWorld and BridgeGameStateToWorld, with
    // the network output read-locked. No asserts on the console.
    // =========================================================================================
    void BrnGameModule::BridgeNetworkToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                             const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput)
    {
        TranslateNetworkEventsToWorld(lpWorldInput, lpNetworkOutput);
        lpWorldInput->AppendVehicleDriverInputInterface(lpNetworkOutput->GetVehicleDriverInputInterface());
        lpWorldInput->AppendVehicleInputInterface(lpNetworkOutput->GetVehicleInputInterface());
        lpWorldInput->SetCrashNetworkInterface(lpNetworkOutput->GetCrashNetworkInputInterface());
        lpWorldInput->SetTrafficNetworkInterface(lpNetworkOutput->GetTrafficNetworkInputInterface());
    }

    // =========================================================================================
    // TranslateNetworkEventsToWorld
    //
    // Called first by BridgeNetworkToWorld: walk the network output's event queue and hand the
    // world-bound OUT events on -- car colour / paint finish (19), lost / regained contact (23)
    // and car-select status (48) into the world input's per-car arrays; player-left-game (24),
    // local-player-disconnected (40) and restart-traffic (43) onto its game-action queue.
    // =========================================================================================
    void BrnGameModule::TranslateNetworkEventsToWorld(BrnWorldIO::UpdateInputBuffer* lpWorldInput,
                                                      const BrnNetwork::BrnNetworkModuleIO::OutputBuffer* lpNetworkOutput)
    {
        namespace NetIO = BrnNetwork::BrnNetworkModuleIO;
        namespace GsIO  = BrnGameState::GameStateModuleIO;

        const NetIO::NetworkEventQueue* lpNetworkEventQueue = lpNetworkOutput->GetNetworkEventQueue();
        CGS_ASSERT(lpNetworkEventQueue, "lpNetworkEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventType = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventType = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventType)
            {
                case NetIO::NetworkOutPlayerChangedCarColourEvent::KI_EVENT_TYPE:           // 19
                {
                    const NetIO::NetworkOutPlayerChangedCarColourEvent* lpPlayerChangedCarColourEvent =
                        reinterpret_cast<const NetIO::NetworkOutPlayerChangedCarColourEvent*>(lpEvent);
                    CGS_ASSERT(lpPlayerChangedCarColourEvent, "lpPlayerChangedCarColourEvent");
                    const EActiveRaceCarIndex leActiveRaceCarIndex = lpPlayerChangedCarColourEvent->GetActiveRaceCarIndex();
                    if (static_cast<u32>(leActiveRaceCarIndex) <= 7u)
                    {
                        lpWorldInput->SetRaceCarColourIndex(leActiveRaceCarIndex,
                                                            lpPlayerChangedCarColourEvent->GetCarColourIndex());
                        lpWorldInput->SetRaceCarPaintFinishIndex(leActiveRaceCarIndex,
                                                                 lpPlayerChangedCarColourEvent->GetPaintFinishIndex());
                    }
                    break;
                }
                case NetIO::NetworkPlayerDisconnectedEvent::KI_EVENT_TYPE:                  // 23
                {
                    const NetIO::NetworkPlayerDisconnectedEvent* lpPlayerDisconnectedEvent =
                        reinterpret_cast<const NetIO::NetworkPlayerDisconnectedEvent*>(lpEvent);
                    if (lpPlayerDisconnectedEvent->GetDisconnectStatus() ==
                        NetIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_LOST_CONTACT)
                    {
                        lpWorldInput->SetLostContact(lpPlayerDisconnectedEvent->GetActiveRaceCarIndex());
                    }
                    else if (lpPlayerDisconnectedEvent->GetDisconnectStatus() ==
                             NetIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_CONNECTED)
                    {
                        lpWorldInput->SetRegainedContact(lpPlayerDisconnectedEvent->GetActiveRaceCarIndex());
                    }
                    break;
                }
                case NetIO::NetworkOutPlayerLeftGame::KI_EVENT_TYPE:                        // 24
                {
                    GsIO::LocalPlayerLeftGameAction lLeftGameAction;
                    lpWorldInput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lLeftGameAction),
                        GsIO::E_ACTION_LOCAL_PLAYER_LEFT_GAME, static_cast<s32>(sizeof(lLeftGameAction)));
                    break;
                }
                case NetIO::NetworkOutLocalPlayerDisconnected::KI_EVENT_TYPE:               // 40
                {
                    GsIO::LocalPlayerDisconnectedAction lDisconnectedAction;
                    lpWorldInput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lDisconnectedAction),
                        GsIO::E_ACTION_LOCAL_PLAYER_DISCONNECTED, static_cast<s32>(sizeof(lDisconnectedAction)));
                    break;
                }
                case NetIO::NetworkOutRestartTrafficEvent::KI_EVENT_TYPE:                   // 43
                {
                    const NetIO::NetworkOutRestartTrafficEvent* lpRestartTrafficEvent =
                        reinterpret_cast<const NetIO::NetworkOutRestartTrafficEvent*>(lpEvent);
                    CGS_ASSERT(lpRestartTrafficEvent, "lpRestartTrafficEvent");
                    GsIO::RestartTrafficAction lRestartTrafficAction;
                    std::memcpy(lRestartTrafficAction.mau16ActveHulls, lpRestartTrafficEvent->mau16ActveHulls,
                                sizeof(lRestartTrafficAction.mau16ActveHulls));
                    lpWorldInput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lRestartTrafficAction),
                        GsIO::E_ACTION_RESTART_TRAFFIC, static_cast<s32>(sizeof(lRestartTrafficAction)));
                    break;
                }
                case NetIO::NetworkOutPlayerCarSelectStatus::KI_EVENT_TYPE:                 // 48
                {
                    const NetIO::NetworkOutPlayerCarSelectStatus* lpPlayerCarSelectStatus =
                        reinterpret_cast<const NetIO::NetworkOutPlayerCarSelectStatus*>(lpEvent);
                    CGS_ASSERT(lpPlayerCarSelectStatus, "lpPlayerCarSelectStatus");
                    lpWorldInput->SetCarSelectStatus(lpPlayerCarSelectStatus->meActiveRaceCarIndex,
                                                     lpPlayerCarSelectStatus->mbInCarSelect);
                    break;
                }
                default:
                    break;
            }
        }
    }
}
