#pragma once

#include "GameSource/Gui/Flow/Overlay/States/BrnBaseOverlayState.h"

// BrnGui::BaseOkCancelOverlayState - the two-button "OK / Cancel" popup family base:
// help item 1 shows the button-1 label with the SELECT glyph, help item 2 the
// button-2 label with the BACK glyph; the popup leaves on the OK action, the Cancel
// action, or the matching wait-finish request. DWARF home
// BrnBaseOkCancelOverlayState.h (adds no members). FillInPopupType stays with the
// concrete CrashNav/InGame/Online states.
namespace BrnGui
{
    struct BaseOkCancelOverlayState : public BaseOverlayState
    {
        // @0x824B1C78 -- base dressing + both button labels. Ledger-wise this was
        // DWARF-misattributed to CgsGuiStateInterface.h and marked reviewed there with
        // no committed body; recovered HERE at its real home.
        virtual void SetupOverlay(const GuiOverlayFullInfoResponse* lpResponse);

        // @0x824B2840 (this TU) -- leave on OK (49), Cancel (50), or the wait-finish
        // request.
        virtual bool UpdateRunning();
    };
}
