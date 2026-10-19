// ARTIST 0x823EF1A0..0x823EF1DC: the valid flag is the only gate.
#include "GameSource/Game/GameBridgeGameStateToX.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include <cstring>

namespace BrnGame
{
void BridgeGameStateToGui_EventStarts(
    const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
    CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    if (lpGameStateOutput->GetSetUpAllEventStartsInterfaceIsValid())
    {
        BrnGui::GuiEventUpdateEventStarts lEvent;
        const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface& lrInterface =
            lpGameStateOutput->GetSetUpAllEventStartsInterface();
        std::memcpy(&lEvent.mEventStarts, &lrInterface, sizeof(lEvent.mEventStarts));
        PushGuiEvent(lEvent, lpGuiInput); // 203 / 8416, including a valid empty/same-count table
    }
}
}
