#pragma once
#include "types.hpp"
namespace BrnGui {
// ARTIST0x823EDCE0..0x823EDD1C, raw payload consumed by GuiCache event238.
struct GuiEventRacePositionInfo
{
    s8 maiPositions[8];
    bool mabFinished[8];
    bool mabValid[8];
    s32 GetEventType() const { return 238; }
};
static_assert(sizeof(GuiEventRacePositionInfo) == 24, "race position event size");

}
