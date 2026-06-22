// Compile-only embed check for BrnNetwork::DirtyTrickMessage
// (TU: GameSource/Network/Messages/BrnDirtyTrickMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD10. Not linked into the game.
#include "GameSource/Network/Messages/BrnDirtyTrickMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::DirtyTrickMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, DirtyTrickMessage>::value,
                  "DirtyTrickMessage : CgsNetwork::ReliableMessage");

    bool CheckGetName()
    {
        DirtyTrickMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Dirty Trick Message";
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
