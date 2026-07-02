#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkCancelOverlayState.h"

// BrnGui::CrashNavOnlineOkCancelOverlayState - the OK / Cancel popup dressed on the
// "CrashNavOnline" flash frame. DWARF home BrnCrashNavOnlineOkCancelOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
// (Upgraded from the pre-hierarchy offset-cast shim; same X360 body.)
namespace BrnGui
{
    struct CrashNavOnlineOkCancelOverlayState : public BaseOkCancelOverlayState
    {
        // @0x824B1DE0 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_CRASH_NAV_FRAME_LABEL[15];   // DWARF; == "CrashNavOnline"
    };
}
