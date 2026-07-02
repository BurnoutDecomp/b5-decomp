#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseWaitOverlayState.h"

// BrnGui::CrashNavOnlineWaitOverlayState - the wait popup dressed on the
// "CrashNavOnline" flash frame. DWARF home BrnCrashNavOnlineWaitOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
// (Upgraded from the pre-hierarchy offset-cast shim; same X360 body.)
namespace BrnGui
{
    struct CrashNavOnlineWaitOverlayState : public BaseWaitOverlayState
    {
        // @0x824B1DC0 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_CRASH_NAV_FRAME_LABEL[15];   // DWARF; == "CrashNavOnline"
    };
}
