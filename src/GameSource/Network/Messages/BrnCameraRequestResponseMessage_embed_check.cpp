// Compile-only embed check for BrnNetwork::CameraRequestResponseMessage
// (TU: GameSource/Network/Messages/BrnCameraRequestResponseMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD90. Not linked into the game.
#include "GameSource/Network/Messages/BrnCameraRequestResponseMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::CameraRequestResponseMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, CameraRequestResponseMessage>::value,
                  "CameraRequestResponseMessage : CgsNetwork::ReliableMessage");

    // Response enum bounds (DWARF BrnCameraRequestResponseMessage.h:48).
    static_assert(CameraRequestResponseMessage::E_RESPONSE_COUNT == 2, "E_RESPONSE_COUNT == 2");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        CameraRequestResponseMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Camera Request Response Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    void TouchMember(CameraRequestResponseMessage& lMessage)
    {
        lMessage.meCameraRequestResponse = CameraRequestResponseMessage::E_RESPONSE_ACCEPT;
        (void)CheckGetName();
    }
}
