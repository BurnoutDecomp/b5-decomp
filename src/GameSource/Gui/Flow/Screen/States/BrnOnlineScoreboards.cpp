// BrnGui::OnlineScoreboards -- static-member definitions (X360 rodata values, dumped:
// scratchpad/waveI/scoreboards_rodata.txt) and the constructor.
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

namespace BrnGui
{
    // the scoreboards screen.
    const CgsGui::sResourceTuple OnlineScoreboards::maResourcesToLoad[] =
        { { 182, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineScoreboards::muNumResourcesToLoad = 1;

    // @0x8205F648 -- the observed in-queue event set (ARTIST count 12; Dec-07 DWARF said 10).
    // 14/21 resource-completion / apt ONLOAD (cache layer), 6 controller, 26 frame tick,
    // 44 network-disconnected, 64 gui-cache, 116..119 scoreboard responses, 122 target-score
    // response, 123 downloaded-challengeable.
    const s32 OnlineScoreboards::maiEventToObserve[12] =
        { 14, 21, 6, 44, 26, 64, 116, 117, 118, 119, 122, 123 };
    const s32 OnlineScoreboards::miNumEventsObserved = 12;   // @0x8205F678

    // @0x82F2687C -- the three filter toggle titles (rows 0/1/2).
    const char* const OnlineScoreboards::KAPC_FILTER_TITLE_STRINGS[3] =
        { "$SCOREBOARD_EVENT", "$SCOREBOARD_FILTER", "$SCOREBOARD_ROAD" };

    // The apt component names (the OnEnter constructor literals).
    const char OnlineScoreboards::KAC_FILTER_GROUP_NAME[10]         = "filter_mc";
    const char OnlineScoreboards::KAC_LEADERBOARD_TABLE_NAME[9]     = "Table_mc";
    const char OnlineScoreboards::KAC_LOADING_ICON_NAME[11]         = "Loading_mc";
    const char OnlineScoreboards::KAC_BUTTON_ANIMATOR_NAME[19]      = "ButtonPrompts_anim";
    const char OnlineScoreboards::KAC_TABLE_EMPTY_ANIMATOR_NAME[16] = "TableEmpty_anim";

    // The table-request debounce armed by the variation-list response (the 0x3F000000
    // literal in UpdatePermanent's case 118).
    const f32 OnlineScoreboards::KF_DELAY_TO_REQUEST_TABLE = 0.5f;

    // member construction only: the console body installs the state vtable,
    // runs the filter group's constructor and installs the vtables of the table (and its ten
    // columns), the loading icon and the two animators.
    OnlineScoreboards::OnlineScoreboards()
    {
    }
}
