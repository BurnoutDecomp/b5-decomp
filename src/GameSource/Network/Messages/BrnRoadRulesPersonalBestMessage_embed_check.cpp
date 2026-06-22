// Compile-only embed check for BrnNetwork::RoadRulesPersonalBestMessage
// (TU: GameSource/Network/Messages/BrnRoadRulesPersonalBestMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD60. Not linked into the game.
#include "GameSource/Network/Messages/BrnRoadRulesPersonalBestMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::RoadRulesPersonalBestMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, RoadRulesPersonalBestMessage>::value,
                  "RoadRulesPersonalBestMessage : CgsNetwork::ReliableMessage");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        RoadRulesPersonalBestMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Road Rules Personal Best Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
