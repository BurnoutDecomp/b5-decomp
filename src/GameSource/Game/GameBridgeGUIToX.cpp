#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGame::BrnGameModule::BridgeGuiToReplay_PostSim @ 0x823CCA00
//   BrnGame::BrnGameModule::BridgeGuiToSound          @ 0x823C0A58
//
// GUI-output bridge family: each per-frame bridge walks the GUI output buffer's out-event
// queue (VariableEventQueue<18432,16> @ +0x814, via GetGuiOutEventQueue) and re-publishes
// the queued GUI events into a downstream subsystem's INPUT buffer. Two of the four family
// members are bodied here; the other two (BridgeGuiToGameState / TranslateGuiEventsToNetworkEvents)
// land once re-verified.

namespace BrnGame
{
    // @ 0x823CCA00 -- forward replay-bound GUI events to the replay module's post-sim GUI
    // event queue (ids 349 / 525..533 / 594), and register the case-595 serialiser with the
    // replay request interface.
    void BrnGameModule::BridgeGuiToReplay_PostSim(
        BrnReplays::ReplayIO::InputBuffer_PostSim* lpReplayModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpReplayModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   "lpReplayModuleInputBuffer && lpGuiOutputBuffer");

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, "lpGuiEventQueue");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            switch (liType)
            {
                case 349: case 525: case 526: case 527: case 528: case 529:
                case 530: case 531: case 532: case 533: case 594:
                {
                    BrnReplays::ReplayIO::InputBuffer_PostSim::GuiEventQueue* lpDestQueue =
                        lpReplayModuleInputBuffer->GetGuiEventQueue();
                    lpDestQueue->AddEvent(lpEvent, liType, liSize);
                    break;
                }
                case 595:
                {
                    BrnReplays::BaseSerialiser* lpSerialiser =
                        *reinterpret_cast<BrnReplays::BaseSerialiser* const*>(lpEvent);
                    BrnReplays::ReplayIO::RequestInterface* lpRequestInterface =
                        lpReplayModuleInputBuffer->GetRequestInterface();
                    lpRequestInterface->RegisterSerialiser(lpSerialiser);
                    break;
                }
                default: break;
            }
            liType = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // @ 0x823C0A58 -- hand the GUI output buffer's out-event queue to the sound root input
    // buffer (SetGuiEventQueue) so the sound module can drain GUI events.
    void BrnGameModule::BridgeGuiToSound(
        BrnSound::Module::Io::RootInputBuffer* lpSoundModuleInputBuffer,
        const CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer)
    {
        CGS_ASSERT(lpSoundModuleInputBuffer != 0 && lpGuiOutputBuffer != 0,
                   "lpSoundModuleInputBuffer && lpGuiOutputBuffer");

        const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue =
            GetGuiOutEventQueue(lpGuiOutputBuffer);
        CGS_ASSERT(lpGuiEventQueue != 0, "lpGuiEventQueue");

        lpSoundModuleInputBuffer->SetGuiEventQueue(
            reinterpret_cast<const BrnSound::Module::Io::RootInputBuffer::GuiEventQueue*>(
                lpGuiEventQueue));
    }
}
