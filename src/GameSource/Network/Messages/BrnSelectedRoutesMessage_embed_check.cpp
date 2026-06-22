// Compile-only embed check for BrnNetwork::SelectedRoutesMessage
// (TU: GameSource/Network/Messages/BrnSelectedRoutesMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD30. Not linked into the game.
#include "GameSource/Network/Messages/BrnSelectedRoutesMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::SelectedRoutesMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, SelectedRoutesMessage>::value,
                  "SelectedRoutesMessage : CgsNetwork::ReliableMessage");

    bool CheckGetName()
    {
        SelectedRoutesMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Selected Routes Message";
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
