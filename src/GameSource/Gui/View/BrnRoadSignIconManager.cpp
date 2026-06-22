#include "GameSource/Gui/View/BrnRoadSignIconManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x824F5778 (no prior source, no
// DecFIGS DWARF for this TU). Store-for-store with the X360 asm: the broadcast is
// skipped entirely when the requested value already matches the master flag; the
// per-icon apt "_visible" view-state is only driven for icons whose current flag
// differs AND that have an apt view bound, but the per-icon flag is always stored
// when it differs. The master flag is written once after the walk.

namespace BrnGui
{
    // @ 0x824F5778
    void RoadSignIconManager::SetSignsVisible(u8 lbVisible)
    {
        if (lbVisible == mbSignsVisible)
            return;

        for (u32 i = 0; i < KU_NUM_SIGN_ICONS; ++i)
        {
            RoadSignIcon& lIcon = maIcons[i];
            if (lbVisible != lIcon.mbVisible)
            {
                if (lIcon.mbHasAptView == 1)
                {
                    const char* lpacState = (lbVisible == 1) ? "true" : "false";
                    lIcon.AddOutputAptViewState("_visible", lpacState, true);
                }
                lIcon.mbVisible = lbVisible;
            }
        }

        mbSignsVisible = lbVisible;
    }
}
