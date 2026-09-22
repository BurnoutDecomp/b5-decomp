// ============================================================================
// b5-decomp/src/GameSource/Game/BrnGameModule_wN1_02.cpp
//
// Network wave N1, partfile 02 of BrnGameModule.cpp: the two NETWORK legs of the per-frame
// update cascade.
//
//   BrnGame::BrnGameModule::DoUpdate_NetworkPreSim
//   BrnGame::BrnGameModule::DoUpdate_NetworkPostSim
//
// Both drive mNetworkModule, so this file needs the REAL BrnNetwork::BrnNetworkModule.
// ⛔ It does NOT compile while BrnGameModule.hpp still defines its empty placeholder
// `class BrnNetworkModule` (C2011: two definitions of one class in this TU). It compiles as
// soon as the swap wave replaces that placeholder line with
// `#include "GameSource/Network/BrnNetworkModule.h"`. Do not mount it before then.
//
// On the console DoUpdate calls NetworkPreSim right after DoUpdate_InputPreWorld and
// NetworkPostSim right after DoUpdate_GameStatePostWorld, immediately before UpdateTimers.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (IOHelper)
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
        TranslateGuiEventsToNetworkEvents(lpNetworkInput->GetNetworkEventQueue(),
                                          lpGuiOutputBuffer->GetOutEventQueue());

        CgsModule::UnlockBuffersForIO(lpNetworkInput, lpGameStateOutputBuffer, lpWorldOutputBuffer, lpGuiOutputBuffer);

        mNetworkModule.ProcessAfterSimulation(lpInputBufferStack, lpOutputBufferStack, lpNetworkInput, lUpdateSet);

        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_Network);
        CgsDev::PerfMonCpu::StopMonitor(mCpuMonitors.miUT_NetworkAIRaceCar);
    }
}
