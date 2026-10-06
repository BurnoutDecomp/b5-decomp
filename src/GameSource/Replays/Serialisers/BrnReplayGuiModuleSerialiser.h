#pragma once

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"

namespace BrnReplays
{
// ARTIST8264CFA8 derives from the complete BaseSerialiser and appends one flag.
class GuiModuleSerialiser : public BaseSerialiser
{
public:
    s32 Construct();
    GuiModuleStaticLayout* GetStaticLayout();
    s32 Read();
    s32 Write();
private:
    bool mbStaticLayoutReset;
};
}
