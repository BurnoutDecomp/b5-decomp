// BrnGuiHudMessagesDebugComponent.cpp
// The two out-of-line virtual identity overrides of BrnGui::GuiHudMessagesDebugComponent the
// X360 ARTIST build emits:
//
//   GetName @ 0x827DD220 -> "Hud Messages"
//   GetPath @ 0x824ED348 -> "Gui"
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (both are constant-string returns). The rest of
// the component (Construct/Destruct/TriggerMessage/...) reaches the HudMessageDirector and is
// out of this slice's scope.

#include "GameSource/Gui/BrnGuiHudMessagesDebugComponent.h"

namespace BrnGui
{
    // @ 0x827DD220
    const char* GuiHudMessagesDebugComponent::GetName() const
    {
        return "Hud Messages";
    }

    // @ 0x824ED348
    const char* GuiHudMessagesDebugComponent::GetPath() const
    {
        return "Gui";
    }
}
