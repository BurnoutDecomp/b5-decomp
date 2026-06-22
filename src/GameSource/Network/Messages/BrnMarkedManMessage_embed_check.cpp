// Compile-only embed check for BrnNetwork::MarkedManMessage
// (TU: GameSource/Network/Messages/BrnMarkedManMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD40. Not linked into the game.
#include "GameSource/Network/Messages/BrnMarkedManMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::MarkedManMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, MarkedManMessage>::value,
                  "MarkedManMessage : CgsNetwork::ReliableMessage");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        MarkedManMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Marked Man Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
