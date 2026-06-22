// Compile-only embed check for BrnNetwork::CameraRequestMessage
// (TU: GameSource/Network/Messages/BrnCameraRequestMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD80. Not linked into the game.
#include "GameSource/Network/Messages/BrnCameraRequestMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::CameraRequestMessage;

    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, CameraRequestMessage>::value,
                  "CameraRequestMessage : CgsNetwork::ReliableMessage");

    bool CheckGetName()
    {
        CameraRequestMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Camera Request Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }
}
