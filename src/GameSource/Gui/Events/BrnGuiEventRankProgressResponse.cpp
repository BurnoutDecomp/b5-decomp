// BrnGuiEventRankProgressResponse.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiEventRankProgressResponse::GetPlayerRank @ 0x8240EA28
//
//   lwz r11,0x20(this) ; cmpwi r11,-1 ; bne skip
//     <fire "miCurrentRank != KI_PLAYER_HAS_FINISHED_LAST_RANK" assert>
//   skip: lwz r3,0x20(this) ; return r3
//
// Reads miCurrentRank (s32 @ +0x20), runs a non-fatal guard that it is not the
// "finished last rank" sentinel (-1), and returns it (the X360 returns the raw value
// even when the guard fails). The X360-baked assert file/line are discarded per project
// convention; the stringized condition matches the X360 assert message text.

#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h"

namespace BrnGui
{

// @ 0x8240EA28
s32 GuiEventRankProgressResponse::GetPlayerRank() const
{
    CGS_ASSERT( miCurrentRank != KI_PLAYER_HAS_FINISHED_LAST_RANK,
                "miCurrentRank != KI_PLAYER_HAS_FINISHED_LAST_RANK" );
    return miCurrentRank;
}

} // namespace BrnGui
