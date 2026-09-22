#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"

#include <cstddef>   // offsetof

// NetworkInSelectScoreboardEvent's three out-of-line selectors. Each asserts its heading is
// non-negative, then stores exactly two words: its own heading and meType. The other headings
// keep whatever the caller left there (Prepare()'s KI_INVALID_HEADING in the GUI bridge, stack
// contents in the scoreboard debug component).

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    void NetworkInSelectScoreboardEvent::_AssertLayout()
    {
        static_assert(offsetof(NetworkInSelectScoreboardEvent, miCategory) == 0x00, "miCategory @+0x00");
        static_assert(offsetof(NetworkInSelectScoreboardEvent, miIndex) == 0x04, "miIndex @+0x04");
        static_assert(offsetof(NetworkInSelectScoreboardEvent, miVariation) == 0x08, "miVariation @+0x08");
        static_assert(offsetof(NetworkInSelectScoreboardEvent, meType) == 0x0C, "meType @+0x0C");
        static_assert(sizeof(NetworkInSelectScoreboardEvent) == 16, "queued as 16 bytes");
    }

    void NetworkInSelectScoreboardEvent::GetIndexes(s32 liCategory)
    {
        CGS_ASSERT(liCategory >= 0, "liCategory >= 0");
        miCategory = liCategory;
        meType     = E_TYPE_GET_INDEX;
    }

    void NetworkInSelectScoreboardEvent::GetVariations(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        miIndex = liIndex;
        meType  = E_TYPE_GET_VARIATION;
    }

    void NetworkInSelectScoreboardEvent::GetScoreboard(s32 liVariation)
    {
        CGS_ASSERT(liVariation >= 0, "liVariation >= 0");
        miVariation = liVariation;
        meType      = E_TYPE_GET_SCOREBOARD;
    }
}
} // namespace BrnNetwork
