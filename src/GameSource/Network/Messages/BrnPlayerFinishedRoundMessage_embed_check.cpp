// Compile-only embed check for BrnNetwork::PlayerFinishedRoundMessage
// (TU: GameSource/Network/Messages/BrnPlayerFinishedRoundMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFCD0. Not linked into the game.
#include "GameSource/Network/Messages/BrnPlayerFinishedRoundMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::PlayerFinishedRoundMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, PlayerFinishedRoundMessage>::value,
                  "PlayerFinishedRoundMessage : CgsNetwork::ReliableMessage");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        PlayerFinishedRoundMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Player Finished Round Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
