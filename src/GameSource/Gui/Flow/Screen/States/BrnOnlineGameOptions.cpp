// BrnGui::OnlineGameOptions -- static-member definitions (X360 rodata values, dumped in
// wave I: scratchpad/waveI/ogo_rodata.txt / ogo_rodata2.txt).
//
// maResourceTuplesToLoad / miNumResourcesToLoad are deliberately NOT defined here -- they
// are already defined in BrnScreenStatesDataLinkStubs.cpp, and a second definition would be
// a link duplicate that cl /c cannot see.
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

namespace BrnGui
{
    // @0x8205EFD4 -- the observed in-queue event set; @0x8205F000 == 11.
    const s32 OnlineGameOptions::maiEventToObserve[11] =
        { 14, 21, 6, 64, 244, 44, 50, 51, 213, 189, 412 };
    const s32 OnlineGameOptions::miNumEventsObserved = 11;

    // ---- apt component names (the OnEnter/SetupOptions constructor literals) ----------
    const char OnlineGameOptions::KAC_MENU_OPTIONS_COMPONENT[9]          = "MenuItem";
    const char OnlineGameOptions::KAC_CREATE_GAME_TOGGLE_COMPONENT[17]   = "CreateGameToggle";
    const char OnlineGameOptions::KAC_ROUTE_INFO_NAME[10]                = "RouteInfo";
    const char OnlineGameOptions::KAC_UP_ARROW_COMPONENT[13]             = "ArrowUp_anim";
    const char OnlineGameOptions::KAC_DOWN_ARROW_COMPONENT[15]           = "ArrowDown_anim";
    const char OnlineGameOptions::KAC_LOAD_HEADER_COMPONENT[16]          = "LoadHeader_anim";
    const char OnlineGameOptions::KAC_LOAD_HEADER_TEXT_COMPONENT[15]     = "LoadHeaderText";
    const char OnlineGameOptions::KAC_TITLE_TEXT_COMPONENT[11]           = "Title_text";
    const char OnlineGameOptions::KAC_HELP_BAR_COMPONENT[7]              = "Button";
    const char OnlineGameOptions::KAC_MAP_BORDER_ANIMATION_COMPONENT[10] = "Dirt_anim";

    // ---- localisation ids ------------------------------------------------------------
    const char OnlineGameOptions::KAC_GAME_OPTIONS_TITLE_STRING_ID[27]   = "$PAGE_HEADING_CREATE_EVENT";
    const char OnlineGameOptions::KAC_LOAD_OPTIONS_TITLE_STRING_ID[32]   = "$PAGE_HEADING_LOAD_GAME_OPTIONS";
    const char OnlineGameOptions::KAC_CREATED_OPTIONS_STRING_ID[28]      = "$ONLINE_GAME_OPTION_CREATED";
    const char OnlineGameOptions::KAC_RECENT_OPTIONS_STRING_ID[27]       = "$ONLINE_GAME_OPTION_RECENT";
    // The loc KEY (no leading '$') and the SPrintf FORMAT are two different strings.
    const char OnlineGameOptions::KPC_SLOT_STRING_FORMAT_ID[24]          = "ONLINE_GAME_OPTION_SLOT";
    const char OnlineGameOptions::KPC_SLOT_STRING_ID[28]                 = "$ONLINE_GAME_OPTION_SLOT_%d";

    // ---- animation state tables ------------------------------------------------------
    const char* const OnlineGameOptions::KPC_ARROW_ANIMATION_STATES[3] =         // @0x82F2683C
        { "invisible", "visible", "animate" };
    const char* const OnlineGameOptions::KPC_LOAD_HEADER_ANIMATION_STATES[3] =   // @0x82F26848
        { "invisible", "withoutbuttons", "withbuttons" };
    const char* const OnlineGameOptions::KPC_MAP_BORDER_ANIMATION_STATES[2] =    // @0x82F26854
        { "invisible", "visible" };

    // ---- create-match option lists (s32 ids, each E_OPTION_TERMINATOR-terminated) -----
    const CreateMatchOption::EOption OnlineGameOptions::KAE_COMMON_OPTIONS[2] =              // @0x8205F14C
        { CreateMatchOption::E_OPTION_GAME_MODE, CreateMatchOption::E_OPTION_TERMINATOR };
    const CreateMatchOption::EOption OnlineGameOptions::KAE_RACE_MODE_OPTIONS[5] =           // @0x8205F154
        { CreateMatchOption::E_OPTION_ROUNDS, CreateMatchOption::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOption::E_OPTION_VEHICLE_CLASS, CreateMatchOption::E_OPTION_TRAFFIC,
          CreateMatchOption::E_OPTION_TERMINATOR };
    const CreateMatchOption::EOption OnlineGameOptions::KAE_ROAD_RAGE_MODE_OPTIONS[6] =      // @0x8205F168
        { CreateMatchOption::E_OPTION_ROUNDS_EVEN, CreateMatchOption::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOption::E_OPTION_VEHICLE_CLASS, CreateMatchOption::E_OPTION_TRAFFIC,
          CreateMatchOption::E_OPTION_TRAFFIC_CHECKING, CreateMatchOption::E_OPTION_TERMINATOR };
    const CreateMatchOption::EOption OnlineGameOptions::KAE_BURNING_HOME_RUN_MODE_OPTIONS[8] = // @0x8205F180
        { CreateMatchOption::E_OPTION_ROUNDS, CreateMatchOption::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOption::E_OPTION_VEHICLE_CLASS, CreateMatchOption::E_OPTION_TRAFFIC,
          CreateMatchOption::E_OPTION_INFINITE_BOOST, CreateMatchOption::E_OPTION_RUNNER_CRASH_LIMIT,
          CreateMatchOption::E_OPTION_TIME_LIMIT, CreateMatchOption::E_OPTION_TERMINATOR };
    // FLAG: no DWARF name for the fourth list -- named for the role KAP_GAME_MODE_OPTION_DATA
    // gives it (slots 3/4/5 all point at this one array).
    const CreateMatchOption::EOption OnlineGameOptions::KAE_DEFAULT_MODE_OPTIONS[5] =        // @0x8205F1A0
        { CreateMatchOption::E_OPTION_ROUNDS, CreateMatchOption::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOption::E_OPTION_VEHICLE_CLASS, CreateMatchOption::E_OPTION_TRAFFIC,
          CreateMatchOption::E_OPTION_TERMINATOR };

    // @0x82F26824 -- indexed by GetSelectedGameMode(). Slots 3/4/5 are the SAME pointer,
    // which is why three game modes share one option list; that is the dump, not a typo.
    const CreateMatchOption::EOption* const OnlineGameOptions::KAP_GAME_MODE_OPTION_DATA[6] =
        { KAE_RACE_MODE_OPTIONS, KAE_ROAD_RAGE_MODE_OPTIONS, KAE_BURNING_HOME_RUN_MODE_OPTIONS,
          KAE_DEFAULT_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS };

    // @0x8205F1B4 -- DWARF-attested static (DWARF BrnOnlineGameOptions.cpp:223). The X360
    // reaches SetupHelpBar's four {text, button} pairs through folded string/immediate
    // literals rather than loading this table, so it has no xrefs in the image. NO XREFS IS
    // NOT EVIDENCE THE TABLE IS DEAD -- it is the declared source of those pairs and a later
    // sweep must not drop it.
    const OnlineGameOptions::HelpBarItem OnlineGameOptions::KA_HELPBAR_ITEMS[4] =
        { { "$CAPS_BUTTON_BACK_UP",  ButtonIconComponent::EPadButton(5) },
          { "$CAPS_BUTTON_CONTINUE", ButtonIconComponent::EPadButton(4) },
          { "$CAPS_BUTTON_LOAD",     ButtonIconComponent::EPadButton(4) },
          { "$CAPS_BUTTON_LOAD",     ButtonIconComponent::EPadButton(6) } };
}
