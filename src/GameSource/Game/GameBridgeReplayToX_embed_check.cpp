// Embed-check for the BrnGame::BrnGameModule replay-bridge family. Forces the bridge
// method + the GuiReplayStatusEvent layout to be referenced so the gate compiles them.
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeReplayToX.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"

namespace
{
    // Exercise the GuiReplayStatusEvent (the real 1560-byte record boxing the replay
    // status interface) the bridge synthesises by name.
    void ExerciseReplayStatusEvent(BrnGui::GuiReplayStatusEvent& rEvent)
    {
        volatile s32 li = rEvent.GetEventType()              // event id 524
                          + rEvent.mInterface.miCurrentRecordReel
                          + rEvent.mInterface.miCurrentPlaybackReel;
        (void)li;
    }

    // Take the address of the bridge method so it is emitted + type-checked.
    void ReferenceBridges()
    {
        void (BrnGame::BrnGameModule::*lpGui)(
            CgsGui::CgsGuiModuleIO::InputBuffer*,
            const BrnReplays::ReplayIO::OutputBuffer_PreSim*) =
            &BrnGame::BrnGameModule::BridgeReplayToGui;
        (void)lpGui;
    }
}

extern "C" void GameBridgeReplayToX_embed_check()
{
    ReferenceBridges();
    (void)&ExerciseReplayStatusEvent;
}
