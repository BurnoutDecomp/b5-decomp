// ===========================================================================
// BrnWorldModule_wG_Bridges_02.cpp -- two WorldModule per-frame output bridges:
// the traffic module's pre-scene score-target list posted as a GUI event, and the
// crash module's post-physics network interface + game events merged into the
// update output buffer.
//
// Source getter first, then destination getter, then the transfer -- the getters
// are lock tripwires, so the order is observable. Neither console body carries a
// null compare or an assert.
// FLAG: the console brackets the traffic bridge in a CPU perf monitor taken out of
// the world-module context, which arrives here as an untyped pointer; the monitor is
// not modelled, the standing disposition of every landed sibling in Bridges/.
// ===========================================================================

#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"   // both declarations

#include "GameSource/World/BrnWorldModuleIO.h"                                            // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"                       // BrnWorld::CrashIO::OutputBuffer_PostPhysics
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"  // BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficGuiInterface.h" // ScoringVehicleArray
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                          // VariableEventQueue / Event

namespace WorldModule
{

// ---------------------------------------------------------------------------
// WorldModule::BridgeTrafficEntityInfoToOutput_PreScene
//
// Copy the traffic module's score-target array out of its pre-scene output buffer
// (a bare-displacement read on the console, no lock check) into a local, then post
// the local as one GUI traffic-car-info event on the update output buffer's GUI event
// queue (write-lock getter). The event carries no header: the payload IS the array,
// and the console's byte count is the array's size (656).
// The record id 208 is the console's immediate; the same event type in the
// bare-array form the GUI cache reads its score targets from.
// ---------------------------------------------------------------------------
void BridgeTrafficEntityInfoToOutput_PreScene(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutput_PreScene)
{
    (void)lpWorldModule;   // read only for the perf-monitor handle, which is not modelled

    BrnTraffic::BrnTrafficIO::ScoringVehicleArray lScoreTargets =
        *lpTrafficOutput_PreScene->GetPotentialScorees();

    lpOutputBuffer->GetGuiEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lScoreTargets),
        208,
        static_cast<s32>(sizeof(lScoreTargets)));
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeCrashModuleToOutput
//
// Two transfers from the crash module's post-physics output buffer into the update
// output buffer: latch the crash network output interface (the setter clears the
// destination queue and appends the source's owned crashing-traffic updates), then
// append the crash module's game-event queue into the world's.
// ---------------------------------------------------------------------------
void BridgeCrashModuleToOutput(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::CrashIO::OutputBuffer_PostPhysics* lpCrashOutput_PostPhysics)
{
    (void)lpWorldModule;   // copied out of its register and never read

    // Source getter (read-lock) first, destination setter (write-lock) second.
    const BrnWorld::CrashIO::NetworkOutputInterface* lpNetworkOutput =
        lpCrashOutput_PostPhysics->GetNetworkOutputInterface();
    lpOutputBuffer->SetCrashNetworkOutputInterface(lpNetworkOutput);

    // Same order for the queue leg: source getter, then destination getter, then Append.
    const BrnWorld::CrashIO::OutputBuffer_PostPhysics::GameEventQueue* lpSourceEvents =
        lpCrashOutput_PostPhysics->GetGameEventQueue();
    lpOutputBuffer->GetGameEventQueue()->Append(*lpSourceEvents);
}

}
