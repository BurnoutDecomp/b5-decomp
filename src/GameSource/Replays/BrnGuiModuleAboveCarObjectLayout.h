#ifndef BRN_GUI_MODULE_ABOVE_CAR_OBJECT_LAYOUT_H
#define BRN_GUI_MODULE_ABOVE_CAR_OBJECT_LAYOUT_H

#include "types.hpp"

namespace BrnReplays
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E1xxx (Clear).
class GuiModuleAboveCarObjectLayout
{
public:
    GuiModuleAboveCarObjectLayout* Clear();

private:
    u32 mVec[4];  // guest +0 (16-byte vector cleared via stvx128)
    u32 mUnk16;   // guest +16 (untouched by Clear)
    u32 mVal20;   // guest +20
    u32 mVal24;   // guest +24
};
}

#endif
