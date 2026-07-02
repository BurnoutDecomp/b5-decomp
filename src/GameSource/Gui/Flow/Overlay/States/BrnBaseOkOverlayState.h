#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOverlayState.h"

// BrnGui::BaseOkOverlayState - the single-button "OK" popup family base: help item 1
// is blanked, help item 2 shows the (possibly formatted) button-1 label with the
// SELECT glyph, and the popup leaves on the OK action or the matching wait-finish
// request. DWARF home BrnBaseOkOverlayState.h (adds no members). FillInPopupType
// stays with the concrete CrashNav/InGame/Online states.
namespace BrnGui
{
    struct BaseOkOverlayState : public BaseOverlayState
    {
        // @0x824B1BC0 -- base dressing + the OK button label. Ledger-wise this was
        // DWARF-misattributed to CgsGuiStateInterface.h and marked reviewed there with
        // no committed body; recovered HERE at its real home.
        virtual void SetupOverlay(const GuiOverlayFullInfoResponse* lpResponse);

        // @0x824B2770 (this TU) -- leave on the OK action or the wait-finish request.
        virtual bool UpdateRunning();
    };
}
