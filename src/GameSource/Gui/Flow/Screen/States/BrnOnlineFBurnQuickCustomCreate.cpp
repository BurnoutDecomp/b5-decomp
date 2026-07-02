#include "GameSource/Gui/Flow/Screen/States/BrnOnlineFBurnQuickCustomCreate.h"

// BrnGui::OnlineFBurnQuickCustomCreate -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Gui/Flow/Screen/States/BrnOnlineFBurnQuickCustomCreate.cpp):
//   OnlineFBurnQuickCustomCreate::ProcessSelectedMenuOption @0x82487458
//
// The option -> state-event table is read from the XEX .data relocations
// @0x82F26904 (the same recovery as the shot-selector name tables).

namespace BrnGui
{
    const char* const OnlineFBurnQuickCustomCreate::KAPC_MAIN_MENU_FBURN_STATE_ACTIONS_TEXT
        [E_MAIN_MENU_OPTIONS_COUNT] =
    {
        "TO_QWK_MAT",    // E_MAIN_MENU_OPTIONS_QUICK_MATCH
        "TO_CUST_MAT",   // E_MAIN_MENU_OPTIONS_CUSTOM_MATCH
        "ADVANCE",       // E_MAIN_MENU_OPTIONS_CREATE_MATCH
    };

    // @ 0x82487458 -- a tail-call through the table (lwzx + b SendStateEvent).
    void OnlineFBurnQuickCustomCreate::ProcessSelectedMenuOption(EMainMenuOptions leOption)
    {
        SendStateEvent(KAPC_MAIN_MENU_FBURN_STATE_ACTIONS_TEXT[leOption]);
    }
}
