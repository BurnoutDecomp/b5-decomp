#include "GameSource/Gui/View/BrnDistrictMarkerComponent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824733B8 (no prior source, no
// DecFIGS DWARF for this TU). Store-for-store with the X360 asm:
//   if (hide==1 && state != HIDDEN) { apt->SetViewState("transout"); state = HIDDEN; }
//   else if (hide==0 && state == HIDDEN) { apt->SetViewState("transin"); state = VISIBLE; }
// The apt target is reached through vtable slot 0xC (index 3) with a single
// string argument (call site @0x824733F4 / @0x8247342C).

namespace BrnGui
{
    // @ 0x824733B8
    void DistrictMarkerComponent::SetHideCountyIcon(bool lbHide)
    {
        // Embedded subobject call: &mAptTarget == this+0xC, matching the X360 vcall
        // `(*(*(this+0xC)+12))(this+0xC, ...)` (this-register = the subobject address).
        if (lbHide && miIconState != E_ICON_STATE_HIDDEN)
        {
            mAptTarget.SetViewState("transout");
            miIconState = E_ICON_STATE_HIDDEN;
            return;
        }
        if (!lbHide && miIconState == E_ICON_STATE_HIDDEN)
        {
            mAptTarget.SetViewState("transin");
            miIconState = E_ICON_STATE_VISIBLE;
        }
    }
}
