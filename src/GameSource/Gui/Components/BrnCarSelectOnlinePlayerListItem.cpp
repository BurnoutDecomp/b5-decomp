// ===================================================================================
// BrnGui::CarSelectOnlinePlayerListItem  -- implementation
//   class:BrnGui::CarSelectOnlinePlayerListItem
//
// Show @ 0x8241A598
//   Marks the row visible and pushes the apt view-state: the "final" state once the
//   row's selection has been locked in, otherwise the plain "visible" state.
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Components/BrnCarSelectOnlinePlayerListItem.h"

namespace BrnGui
{
    // @ 0x8241A598
    void CarSelectOnlinePlayerListItem::Show()
    {
        mbShown = true;                                            // *(this+0x2E8) = 1

        // Always emit the base "visible" view-state.
        AddOutputAptViewState("apt_state", "visible", false);

        // Once the selection has been finalised, emit the "final" state instead.
        if (mbFinalSelection)
            AddOutputAptViewState("apt_state", "final", false);
        else
            AddOutputAptViewState("apt_state", "visible", false);
    }
}
