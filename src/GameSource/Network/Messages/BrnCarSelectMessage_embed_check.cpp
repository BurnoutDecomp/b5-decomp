// Compile-only embed check for BrnNetwork::CarSelectMessage
// (TU: GameSource/Network/Messages/BrnCarSelectMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFBA8. Not linked into the game.
#include "GameSource/Network/Messages/BrnCarSelectMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::CarSelectMessage;

    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, CarSelectMessage>::value,
                  "CarSelectMessage : CgsNetwork::ReliableMessage");

    bool CheckGetName()
    {
        CarSelectMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Car Select Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
