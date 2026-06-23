#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8240E388.

namespace CgsGui
{
    // @ 0x8240E388 - assert the gui-cache pointer has been wired up (read at +0x10)
    // and return it. Path/line from the baked assert string
    // (GameShared/GameClasses/Gui/CgsGuiShared.h:201).
    BrnGui::GuiCache* GuiAccessPointers::GetGuiCache()
    {
        CGS_ASSERT(mpGuiCache != nullptr, "mpGuiCache");
        return mpGuiCache;
    }
}
