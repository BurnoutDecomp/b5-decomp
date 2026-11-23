#pragma once
#include "types.hpp"

namespace BrnGui {
// ARTIST AddGuiEvent specializations copy one byte for these payload-less signals.
class GuiEventPlayerWrecked { public: u8 maData[1]; s32 GetEventType() const { return 548; } };
class GuiPlayerDrivableFromCrash { public: u8 maData[1]; s32 GetEventType() const { return 378; } };
}
