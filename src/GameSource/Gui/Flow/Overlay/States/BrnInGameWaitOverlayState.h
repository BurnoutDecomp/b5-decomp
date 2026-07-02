#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseWaitOverlayState.h"

// BrnGui::InGameWaitOverlayState - the wait popup dressed on the
// "InGame" flash frame. DWARF home BrnInGameWaitOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
namespace BrnGui
{
    struct InGameWaitOverlayState : public BaseWaitOverlayState
    {
        // @0x824B1DF0 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_IN_GAME_FRAME_LABEL[7];   // DWARF; == "InGame"
    };
}
