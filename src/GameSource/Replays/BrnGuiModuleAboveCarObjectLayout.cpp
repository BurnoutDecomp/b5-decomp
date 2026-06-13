#include "BrnGuiModuleAboveCarObjectLayout.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnReplays::GuiModuleAboveCarObjectLayout::Clear
//
// Clears the 16-byte layout vector and two trailing words.

namespace BrnReplays
{
GuiModuleAboveCarObjectLayout* GuiModuleAboveCarObjectLayout::Clear()
{
    mVal24 = 0;
    mVal20 = 0;

    mVec[0] = 0;
    mVec[1] = 0;
    mVec[2] = 0;
    mVec[3] = 0;

    return this;
}
}
