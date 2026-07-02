#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkCancelOverlayState.h"

// BrnGui::InGameOkCancelOverlayState - the OK / Cancel popup dressed on the
// "InGame" flash frame. DWARF home BrnInGameOkCancelOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
// (Upgraded from the pre-hierarchy offset-cast shim; same X360 body.)
namespace BrnGui
{
    struct InGameOkCancelOverlayState : public BaseOkCancelOverlayState
    {
        // @0x824B1E10 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_IN_GAME_FRAME_LABEL[7];   // DWARF; == "InGame"
    };
}
