// Compile-only embed check for BrnNetwork::LiveRevengeSyncMessage
// (TU: GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD00. Not linked into the game.
#include "GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::LiveRevengeSyncMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, LiveRevengeSyncMessage>::value,
                  "LiveRevengeSyncMessage : CgsNetwork::ReliableMessage");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        LiveRevengeSyncMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Live Revenge Sync Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
