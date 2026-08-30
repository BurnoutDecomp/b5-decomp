// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGUIToX_Sound.cpp
//
// SIBLING SPLIT of GameBridgeGUIToX.cpp (the established precedent: that parent
// TU cannot be mounted -- its two event-translating members reference six symbols
// with no home in the linked set, one of which, TelemetryData::AddParameter, has
// no reconstructed home anywhere; see the parent banner and the build bat's
// GameBridgeGUIToX_GameState.cpp note). BridgeGuiToSound gained its caller with
// the faithful-audio-engine phase C4 spine wiring (LoadingScriptedState::Update
// drives it each frame @0x823F2A2C), so it is MOVED -- not copied -- here so it
// can be mounted on its own. Folding it back later is a delete.
//
// Bodied here (1 ledger function):
//   BrnGame::BrnGameModule::BridgeGuiToSound @ 0x823C0A58  (DWARF home
//   GameSource/Game/GameBridgeGUIToX.cpp)
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGUIToX.h"                 // typed GetGuiOutEventQueue bridge
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"     // RootInputBuffer + GuiEventQueue

namespace BrnGame
{
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
