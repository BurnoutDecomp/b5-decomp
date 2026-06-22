// Compile-only embed check for BrnNetwork::AggressiveDrivingMessage
// (TU: GameSource/Network/Messages/BrnAggressiveDrivingMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DE0A8. Not linked into the game.
#include "GameSource/Network/Messages/BrnAggressiveDrivingMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::AggressiveDrivingMessage;
    using BrnNetwork::AggressiveMoveData;

    // The message derives from the committed CgsNetwork::Message base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::Message, AggressiveDrivingMessage>::value,
                  "AggressiveDrivingMessage : CgsNetwork::Message");

    // The record array is sized by the committed move-count bound.
    static_assert(BrnNetwork::KI_MAX_AGGRESSIVE_MOVES == 5, "KI_MAX_AGGRESSIVE_MOVES == 5");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        AggressiveDrivingMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Aggressive Driving Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    // Touch a payload field by name so the AggressiveMoveData shape is compiled too.
    void TouchPayload(AggressiveMoveData& lData)
    {
        lData.meAggressiveMoveType = BrnNetwork::E_AGGRESSIVE_MOVE_TAKE_DOWN;
        (void)lData.miNumberOfTimesTriedToSend;
    }
}
