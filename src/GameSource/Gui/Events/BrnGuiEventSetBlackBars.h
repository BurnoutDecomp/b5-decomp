#pragma once

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"

namespace BrnGui
{
    // ARTIST AddGuiEvent<GuiEventSetBlackBars> @0x823CEE50 publishes one f32,
    // event 221. DecFIGS BrnGuiEventTypeDefs.h:2105 names the payload.
    class GuiEventSetBlackBars : public CgsGui::GuiEvent<221>
    {
    public:
        f32 lfSingleBarSize;
    };
}
