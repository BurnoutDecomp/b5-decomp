// ---- GameSource/Network/Managers/BrnNetworkScoreboard.cpp ----
// Definition home for BrnNetwork::Scoreboard (and its column/row sub-objects).
//
// BrnNetwork::Scoreboard::GetRow  @ 0x8240E9A8
//   (called by BrnGui::LeaderboardTableComponent::SetCell)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The X360 body:
//   * assert liRowNumber >= 0                 ("liRowNumber >= 0",        :309)
//   * assert liRowNumber < miNumberOfRows      ("liRowNumber < miNumberOfRows", :310)
//       miNumberOfRows is read as a sign-extended byte (lbz + extsb at +0xB68), i.e. the int8
//       member; the comparison is signed (cmpw) against that sign-extended value.
//   * return &maRows[liRowNumber]  (mulli 0x137 == row stride 311, add this, addi 0x190 == maRows
//     base offset 400).
// Both streamed file-line asserts are reduced to the project CGS_ASSERT convention. Member access
// is by name; the row stride / base offset are produced by the committed Scoreboard layout, not by
// raw-offset arithmetic.

#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnNetwork
{
    const ScoreboardRow* Scoreboard::GetRow(s32 liRowNumber) const
    {
        CGS_ASSERT(liRowNumber >= 0, "liRowNumber >= 0");
        CGS_ASSERT(liRowNumber < miNumberOfRows, "liRowNumber < miNumberOfRows");
        return &maRows[liRowNumber];
    }
}
