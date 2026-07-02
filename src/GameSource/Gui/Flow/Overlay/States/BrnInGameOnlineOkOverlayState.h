#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkOverlayState.h"

// BrnGui::InGameOnlineOkOverlayState - the single-button OK popup dressed on the
// "InGameOnline" flash frame. DWARF home BrnInGameOnlineOkOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
namespace BrnGui
{
    struct InGameOnlineOkOverlayState : public BaseOkOverlayState
    {
        // @0x824B1E30 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_IN_GAME_FRAME_LABEL[13];   // DWARF; == "InGameOnline"
    };
}
