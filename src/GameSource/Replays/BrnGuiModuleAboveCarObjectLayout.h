#ifndef BRN_GUI_MODULE_ABOVE_CAR_OBJECT_LAYOUT_H
#define BRN_GUI_MODULE_ABOVE_CAR_OBJECT_LAYOUT_H

#include "types.hpp"
#include "BrnCommonTypes.h"

namespace BrnReplays
{
// ARTIST Clear8264CDD8 and AboveCarRenderer Render/Replay82462A38/8245B3A0.
// Field names describe those readers; the record has no pointers and remains32 bytes.
class GuiModuleAboveCarObjectLayout
{
public:
    GuiModuleAboveCarObjectLayout* Clear();

    Vector3 mWorldPosition;
    u32 mColour;
    u32 muRacePosition;
    bool mbVisible;
};
static_assert(sizeof(GuiModuleAboveCarObjectLayout) == 32, "ARTIST replay car stride");
}

#endif
