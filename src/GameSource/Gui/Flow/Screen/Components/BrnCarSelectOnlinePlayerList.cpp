// ===================================================================================
// BrnGui::CarSelectOnlinePlayerList  -- implementation
//   class:BrnGui::CarSelectOnlinePlayerList
//
// IsShowing @ 0x82482950
//   Range-checks the row index (X360: signed cmpwi index,0 / cmpwi index,8 -- the assert
//   fires when index < 0 or index >= 8; the Hex-Rays "a2 >= 8" unsigned compare was a
//   misread of the two signed compares) then returns the per-row showing byte at
//   row[index] +0x378 (lbz, an 8-bit flag). The original streamed an "Invalid player
//   index" message into the assert buffer; CGS_ASSERT carries the plain condition.
//   Reconstructed from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x82482950
    bool CarSelectOnlinePlayerList::IsShowing(s32 liPlayerIndex) const
    {
        CGS_ASSERT(liPlayerIndex >= 0 && liPlayerIndex < KI_MAX_PLAYERS,
                   "Invalid player index");                                   // @0x82482968/0x82482970

        return maPlayerRows[liPlayerIndex].mbShowing;                          // lbz 0x378(index*0x2F0 + this)
    }
}
