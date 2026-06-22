// Embed/exercise check for BrnDirector::Camera::Utils::DebugController (compile-only).
// Embeds the type by value and exercises the five bodied functions. Not shipped.

#include "GameSource/Director/Camera/Utils/BrnDebugController.h"

namespace
{
    using BrnDirector::Camera::Utils::DebugController;

    struct Holder
    {
        DebugController mController;   // embed by value
    };

    bool Use()
    {
        Holder h;
        h.mController.Clear();                                       // @0x8220BFD8

        DebugController::DebugControllerInfo info;
        info.Clear();                                               // @0x821F8738

        bool down = h.mController.GetIsPressed(DebugController::E_CONTROL_UP_DPAD);          // @0x821F8708
        down ^= h.mController.GetJustPressed(DebugController::E_CONTROL_RIGHT_STICK_BUTTON); // @0x821F8718
        down ^= h.mController.GetJustReleased(DebugController::E_CONTROL_LOWER_TRIGGER_LEFT);// @0x821F8728
        return down;
    }
}
