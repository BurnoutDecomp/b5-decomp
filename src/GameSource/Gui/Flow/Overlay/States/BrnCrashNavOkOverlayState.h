#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOkOverlayState.h"

// BrnGui::CrashNavOkOverlayState - the single-button OK popup dressed on the
// "CrashNav" flash frame. DWARF home BrnCrashNavOkOverlayState.h; the class adds only
// FillInPopupType (the family bases carry all behaviour).
// (Upgraded from the pre-hierarchy offset-cast shim; same X360 body.)
namespace BrnGui
{
    struct CrashNavOkOverlayState : public BaseOkOverlayState
    {
        // @0x824B1DA0 -- stamp the popup family's flash file id.
        virtual void FillInPopupType();

    private:
        static const char KPC_CRASH_NAV_FRAME_LABEL[9];   // DWARF; == "CrashNav"
    };
}
