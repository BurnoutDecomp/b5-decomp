// Compile-only embed check for BrnNetwork::CameraStatusMessage
// (TU: GameSource/Network/Messages/BrnCameraStatusMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DE0B8. Not linked into the game.
#include "GameSource/Network/Messages/BrnCameraStatusMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::CameraStatusMessage;

    // The message derives from the committed CgsNetwork::Message base (reused by name).
    static_assert(std::is_base_of<CgsNetwork::Message, CameraStatusMessage>::value,
                  "CameraStatusMessage : CgsNetwork::Message");

    // Status enum bounds (DWARF BrnCameraStatusMessage.h:48).
    static_assert(BrnNetwork::E_CAMERA_STATUS_COUNT == 3, "E_CAMERA_STATUS_COUNT == 3");

    // Exercise the bodied ledger func: returns the X360 rodata literal.
    bool CheckGetName()
    {
        CameraStatusMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Camera Status Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    void TouchStatus()
    {
        BrnNetwork::ECameraStatus leStatus = BrnNetwork::E_CAMERA_STATUS_AVAILABLE;
        (void)leStatus;
        (void)CheckGetName();
    }
}
