#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseWaitOverlayState.h"

// BrnGui::InGameOnlineWaitOverlayState - the wait popup dressed on the
// "InGameOnline" flash frame. DWARF home BrnInGameOnlineWaitOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
namespace BrnGui
{
    struct InGameOnlineWaitOverlayState : public BaseWaitOverlayState
    {
        // @0x824B1E20 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_IN_GAME_FRAME_LABEL[13];   // DWARF; == "InGameOnline"
    };
}
