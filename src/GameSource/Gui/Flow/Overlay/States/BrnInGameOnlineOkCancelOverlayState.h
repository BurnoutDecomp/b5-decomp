#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkCancelOverlayState.h"

// BrnGui::InGameOnlineOkCancelOverlayState - the OK / Cancel popup dressed on the
// "InGameOnline" flash frame. DWARF home BrnInGameOnlineOkCancelOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
namespace BrnGui
{
    struct InGameOnlineOkCancelOverlayState : public BaseOkCancelOverlayState
    {
        // @0x824B1E40 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_IN_GAME_FRAME_LABEL[13];   // DWARF; == "InGameOnline"
    };
}
