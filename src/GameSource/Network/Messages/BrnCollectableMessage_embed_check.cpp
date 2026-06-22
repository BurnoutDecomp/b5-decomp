// Compile-only embed check for BrnNetwork::CollectableMessage
// (TU: GameSource/Network/Messages/BrnCollectableMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFBB8. Not linked into the game.
#include "GameSource/Network/Messages/BrnCollectableMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::CollectableMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, CollectableMessage>::value,
                  "CollectableMessage : CgsNetwork::ReliableMessage");

    // Collectable-type enum bounds (DWARF BrnCollectableMessage.h:47).
    static_assert(CollectableMessage::E_COLLECTABLE_TYPE_MAX == 3, "E_COLLECTABLE_TYPE_MAX == 3");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        CollectableMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Collectable Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    void TouchType()
    {
        CollectableMessage::ECollectableType leType = CollectableMessage::E_COLLECTABLE_TYPE_BILLBOARD;
        (void)leType;
        (void)CheckGetName();
    }
}
