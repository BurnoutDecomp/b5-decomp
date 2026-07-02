#pragma once

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h"

// BrnGui::OnlineFBurnQuickCustomCreate - the freeburn flavour of the online
// "quick / custom / create match" menu state: it only remaps the three menu
// options onto its own FSM state events. DWARF home
// BrnOnlineFBurnQuickCustomCreate.h:42.
namespace BrnGui
{
    struct OnlineFBurnQuickCustomCreate : public OnlineQuickCustomCreate
    {
    private:
        // @0x82487458 (this TU, DWARF cpp:51) -- send the picked option's state event.
        virtual void ProcessSelectedMenuOption(EMainMenuOptions leOption);

        // DWARF cpp:27 -- the option -> state-event table (XEX .data @0x82F26904:
        // "TO_QWK_MAT" / "TO_CUST_MAT" / "ADVANCE").
        static const char* const KAPC_MAIN_MENU_FBURN_STATE_ACTIONS_TEXT[E_MAIN_MENU_OPTIONS_COUNT];
    };
}
