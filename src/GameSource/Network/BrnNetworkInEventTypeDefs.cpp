#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include <cstddef>   // offsetof

// NetworkInSelectScoreboardEvent's three out-of-line selectors. Each asserts its heading is
// non-negative, then stores exactly two words: its own heading and meType. The other headings
// keep whatever the caller left there (Prepare()'s KI_INVALID_HEADING in the GUI bridge, stack
// contents in the scoreboard debug component).

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/BrnNetworkInEventTypeDefs.h".

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    void NetworkInSelectScoreboardEvent::_AssertLayout()
    {
        CGS_STATIC_ASSERT((offsetof(NetworkInSelectScoreboardEvent, miCategory) == 0x00), "miCategory @+0x00");
        CGS_STATIC_ASSERT((offsetof(NetworkInSelectScoreboardEvent, miIndex) == 0x04), "miIndex @+0x04");
        CGS_STATIC_ASSERT((offsetof(NetworkInSelectScoreboardEvent, miVariation) == 0x08), "miVariation @+0x08");
        CGS_STATIC_ASSERT((offsetof(NetworkInSelectScoreboardEvent, meType) == 0x0C), "meType @+0x0C");
        CGS_STATIC_ASSERT((sizeof(NetworkInSelectScoreboardEvent) == 16), "queued as 16 bytes");
    }

    void NetworkInSelectScoreboardEvent::GetIndexes(s32 liCategory)
    {
        CGS_ASSERT(liCategory >= 0,
            CGS_ASSERT_MESSAGE_AT("liCategory >= 0", __FILE__, 1031 /* ARTIST 0x823A637C */));
        miCategory = liCategory;
        meType     = E_TYPE_GET_INDEX;
    }

    void NetworkInSelectScoreboardEvent::GetVariations(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0,
            CGS_ASSERT_MESSAGE_AT("liIndex >= 0", __FILE__, 1048 /* ARTIST 0x823A63E4 */));
        miIndex = liIndex;
        meType  = E_TYPE_GET_VARIATION;
    }

    void NetworkInSelectScoreboardEvent::GetScoreboard(s32 liVariation)
    {
        CGS_ASSERT(liVariation >= 0,
            CGS_ASSERT_MESSAGE_AT("liVariation >= 0", __FILE__, 1065 /* ARTIST 0x823A644C */));
        miVariation = liVariation;
        meType      = E_TYPE_GET_SCOREBOARD;
    }
}
} // namespace BrnNetwork
