#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseWaitOverlayState.h"

// BrnGui::InGameOnlineEnterFreeBurnOverlayState - the wait popup dressed on the
// "EnterFreeBurn" flash frame. DWARF home BrnInGameOnlineEnterFreeBurnOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
namespace BrnGui
{
    struct InGameOnlineEnterFreeBurnOverlayState : public BaseWaitOverlayState
    {
        // @0x824B1E50 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_IN_GAME_FRAME_LABEL[14];   // DWARF; == "EnterFreeBurn"
    };
}
