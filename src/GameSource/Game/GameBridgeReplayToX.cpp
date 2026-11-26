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
// The GUI event sink is the shared BrnGame::PushGuiEvent, which queues the whole record at
// offset 0 with sizeof(T) as its size -- what the console publisher does. The replay OUTPUT
// buffer (BrnReplays::ReplayIO::OutputBuffer_PreSim) + its StatusInterface and GUI event
// queue are the committed types. See GameBridgeReplayToX.h for details.
//
// The GuiReplayStatusEvent the bridge synthesises is the real 1560-byte record (the replay
// StatusInterface at +0x00, event id 524) -- nothing fabricated.
// ============================================================================
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeReplayToX.cpp"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeReplayToX.h"

#include "GameSource/Game/GameBridgeGameStateToX.h"    // BrnGame::PushGuiEvent (the shared GUI event push)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"       // BrnReplays::ReplayIO::OutputBuffer_PreSim
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // VariableEventQueue<32768,16>::Append<4096,16>

namespace BrnGame
{
    // =========================================================================
    // BridgeReplayToGui  (X360 0x823E7210)
    //
    // Republish the replay module's pre-sim GUI output into the live GUI module:
    //   1. snapshot the replay status interface into a GuiReplayStatusEvent (type 524)
    //      and push it through the GUI module's AddGuiEvent sink, and
    //   2. bulk-append the replay output buffer's queued GUI events into the GUI input
    //      buffer's inbound event queue.
    // =========================================================================
    void BrnGameModule::BridgeReplayToGui(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput,
        const BrnReplays::ReplayIO::OutputBuffer_PreSim* lpReplayOutput)
    {
        // GameBridgeReplayToX.cpp:89 / :90 -- both buffer pointers must be valid.
        CGS_ASSERT_AT(lpGuiInput != 0, "lpGuiInput", CGS_ARTIST_FILE, 89);
        CGS_ASSERT_AT(lpReplayOutput != 0, "lpReplayOutput", CGS_ARTIST_FILE, 90);

        // ---- (1) status-interface -> GuiReplayStatusEvent --------------------------
        // GameBridgeReplayToX.cpp:92 -- the replay output buffer's status interface.
        const BrnReplays::ReplayIO::StatusInterface* lpStatusInterface =
            lpReplayOutput->GetStatusInterface();
        CGS_ASSERT_AT(lpStatusInterface != 0, "lpStatusInterface", CGS_ARTIST_FILE, 93); // :93

        // Build the GUI status event and copy the status interface into it
        // (BrnReplays::ReplayIO::StatusInterface::operator=), then queue the whole record.
        BrnGui::GuiReplayStatusEvent lEvent;
        lEvent.mInterface = *lpStatusInterface;
        PushGuiEvent(lEvent, lpGuiInput);

        // ---- (2) replay GUI event queue -> GUI input event queue -------------------
        // The replay output buffer's small (4096) GUI event queue is bulk-appended into
        // the GUI input buffer's large (32768) inbound queue
        // (VariableEventQueue<32768,16>::Append<4096,16>).
        const BrnReplays::ReplayIO::OutputBuffer_PreSim::GuiEventQueue* lpReplayGuiQueue =
            lpReplayOutput->GetGuiEventQueue();
        lpGuiInput->GetGuiEvents()->Append(*lpReplayGuiQueue);
    }
}

#undef CGS_ARTIST_FILE
