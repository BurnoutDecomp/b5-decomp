// Compile-only embed check for BrnNetwork::RoadRulesMessage
// (TU: GameSource/Network/Messages/BrnRoadRulesMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD50. Not linked into the game.
#include "GameSource/Network/Messages/BrnRoadRulesMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::RoadRulesMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, RoadRulesMessage>::value,
                  "RoadRulesMessage : CgsNetwork::ReliableMessage");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        RoadRulesMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Road Rules Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
