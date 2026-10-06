#pragma once

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"

namespace BrnGui
{
    // ARTIST AddGuiEvent<GuiEventSetBlackBars> @0x823CEE50 publishes one f32,
    // event 221. DecFIGS BrnGuiEventTypeDefs.h:2105 names the payload.
    struct GuiEventSetBlackBars : CgsGui::GuiEvent<221>
    {
        f32 lfSingleBarSize;
    };
}
