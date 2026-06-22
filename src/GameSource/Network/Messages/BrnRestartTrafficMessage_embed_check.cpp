// Compile-only embed check for BrnNetwork::RestartTrafficMessage
// (TU: GameSource/Network/Messages/BrnRestartTrafficMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFCF0. Not linked into the game.
#include "GameSource/Network/Messages/BrnRestartTrafficMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::RestartTrafficMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, RestartTrafficMessage>::value,
                  "RestartTrafficMessage : CgsNetwork::ReliableMessage");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        RestartTrafficMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Restart Traffic Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
