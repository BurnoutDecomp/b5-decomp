// Compile-only embed check for BrnNetwork::TrafficHashMessage
// (TU: GameSource/Network/Messages/BrnTrafficHashMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DE098. Not linked into the game.
#include "GameSource/Network/Messages/BrnTrafficHashMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::TrafficHashMessage;

    // The message derives from the committed CgsNetwork::Message base (by name).
    static_assert(std::is_base_of<CgsNetwork::Message, TrafficHashMessage>::value,
                  "TrafficHashMessage : CgsNetwork::Message");

    bool CheckGetName()
    {
        TrafficHashMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Traffic Hash Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    void TouchType()
    {
        (void)CheckGetName();
    }
}
