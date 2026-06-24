#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetIndexes    @ 0x823A6350
//   BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetVariations @ 0x823A63B8
//   BrnNetwork::BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::GetScoreboard @ 0x823A6420
//     (each called by BrnGame::BrnGameModule::TranslateGuiEventsToNetworkEvents and the
//      matching BrnNetwork::ScoreboardDebugComponent builder)
//
// Three factory builders for the select-scoreboard IN-event. Each takes one non-negative
// selection value (asserted >= 0) and writes EXACTLY two 32-bit words into the constructed
// event: its own value field plus the meSelectType discriminator (2 / 3 / 4). The remaining
// value fields are deliberately left unwritten -- the X360 bodies emit only the two `stw`
// stores per builder, so the other fields are not initialised here either.

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    NetworkInSelectScoreboardEvent NetworkInSelectScoreboardEvent::GetIndexes(s32 liCategory)
    {
        CGS_ASSERT(liCategory >= 0, "liCategory >= 0");

        NetworkInSelectScoreboardEvent lEvent;
        lEvent.miCategory   = liCategory;            // stw r30, 0(this)
        lEvent.meSelectType = E_SELECT_INDEXES;       // li r11, 2; stw r11, 0xC(this)
        return lEvent;
    }

    NetworkInSelectScoreboardEvent NetworkInSelectScoreboardEvent::GetVariations(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");

        NetworkInSelectScoreboardEvent lEvent;
        lEvent.miIndex      = liIndex;                // stw r30, 4(this)
        lEvent.meSelectType = E_SELECT_VARIATIONS;    // li r11, 3; stw r11, 0xC(this)
        return lEvent;
    }

    NetworkInSelectScoreboardEvent NetworkInSelectScoreboardEvent::GetScoreboard(s32 liVariation)
    {
        CGS_ASSERT(liVariation >= 0, "liVariation >= 0");

        NetworkInSelectScoreboardEvent lEvent;
        lEvent.miVariation  = liVariation;            // stw r30, 8(this)
        lEvent.meSelectType = E_SELECT_SCOREBOARD;    // li r11, 4; stw r11, 0xC(this)
        return lEvent;
    }
}
} // namespace BrnNetwork
