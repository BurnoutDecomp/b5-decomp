#include "BrnGuiModuleAboveCarObjectLayout.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnReplays::GuiModuleAboveCarObjectLayout::Clear
//
// Clear position, race position, and the single visibility byte. Preserve colour.

namespace BrnReplays
{
GuiModuleAboveCarObjectLayout* GuiModuleAboveCarObjectLayout::Clear()
{
    mbVisible = false;
    muRacePosition = 0;
    mWorldPosition = {0.0f, 0.0f, 0.0f, 0.0f};

    return this;
}
}
