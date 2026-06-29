// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeReplayToX.cpp
//
// The BrnGame::BrnGameModule replay-bridge family. Each per-frame bridge reads the
// replay module's pre/post-sim OUTPUT buffer (BrnReplays::ReplayIO::OutputBuffer_*,
// the committed homes) and republishes its contents into a downstream subsystem's
// INPUT buffer -- the mirror image of the controller bridges in
// GameBridgeControllerToX.cpp.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   BridgeReplayToGui   0x823E7210
//
// HONEST PLACEHOLDERS (FLAGGED): the CgsGui::GuiModule event sink (mpCgsGuiModule)
// and its AddGuiEvent<T>(event, inputBuffer) template are the committed by-name
// placeholders from GameBridgeControllerToX.h; the GUI input-event queue is reached by
// its proven +4 byte offset (the empty CgsGui::CgsGuiModuleIO::InputBuffer placeholder
// in BrnGameModule.hpp blocks #including the real CgsGuiModuleIO.h -- ODR). The replay
// OUTPUT buffer (BrnReplays::ReplayIO::OutputBuffer_PreSim) + its StatusInterface and
// GUI event queue are the REAL committed types. See GameBridgeReplayToX.h for details.
//
// The GuiReplayStatusEvent the bridge synthesises is the real GuiEvent<514> boxing the
// replay StatusInterface (DWARF BrnGuiEventTypeDefs.h:6261) -- nothing fabricated.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeReplayToX.h"
#include "GameSource/Game/GameBridgeControllerToX.h"   // CgsGui::GuiModule placeholder + AddGuiEvent<T>

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameSource/Replays/BrnReplayModuleIO.h"       // BrnReplays::ReplayIO::OutputBuffer_PreSim
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // VariableEventQueue<32768,16>::Append<4096,16>

namespace BrnGame
{
    // =========================================================================
    // BridgeReplayToGui  (X360 0x823E7210)
    //
    // Republish the replay module's pre-sim GUI output into the live GUI module:
    //   1. snapshot the replay status interface into a GuiReplayStatusEvent (type 514)
    //      and push it through the GUI module's AddGuiEvent sink, and
    //   2. bulk-append the replay output buffer's queued GUI events into the GUI input
    //      buffer's inbound event queue.
    // =========================================================================
    void BrnGameModule::BridgeReplayToGui(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutput)
    {
        // GameBridgeReplayToX.cpp:89 / :90 -- both buffer pointers must be valid.
        CGS_ASSERT(lpGuiInput != 0, "lpGuiInput");
        CGS_ASSERT(lpReplayOutput != 0, "lpReplayOutput");

        // ---- (1) status-interface -> GuiReplayStatusEvent --------------------------
        // GameBridgeReplayToX.cpp:92 -- the replay output buffer's status interface.
        const BrnReplays::ReplayIO::StatusInterface* lpStatusInterface =
            lpReplayOutput->GetStatusInterface();
        CGS_ASSERT(lpStatusInterface != 0, "lpStatusInterface"); // :93

        // GameBridgeReplayToX.cpp:95 -- build the GUI status event and copy the status
        // interface into it (BrnReplays::ReplayIO::StatusInterface::operator=), then push
        // it through the GUI module's event sink (X360 AddGuiEvent<GuiReplayStatusEvent>).
        BrnGui::GuiReplayStatusEvent lEvent;
        lEvent.mInterface = *lpStatusInterface;
        mpCgsGuiModule->AddGuiEvent(&lEvent, lpGuiInput);

        // ---- (2) replay GUI event queue -> GUI input event queue -------------------
        // The replay output buffer's small (4096) GUI event queue is bulk-appended into
        // the GUI input buffer's large (32768) inbound queue
        // (VariableEventQueue<32768,16>::Append<4096,16>).
        const BrnReplays::ReplayIO::OutputBuffer_PreSim::GuiEventQueue* lpReplayGuiQueue =
            lpReplayOutput->GetGuiEventQueue();
        CgsModule::VariableEventQueue<32768, 16>* lpGuiInputQueue =
            GetGuiInputEventQueue(lpGuiInput);
        lpGuiInputQueue->Append(*lpReplayGuiQueue);
    }
}
