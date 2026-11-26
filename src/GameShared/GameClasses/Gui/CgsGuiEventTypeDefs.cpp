#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include <cfloat>   // FLT_MAX

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8240E328.

namespace CgsGui
{
    // @ 0x8240E328 - assert mfTimeNow (+0x04) is not the -FLT_MAX "unset" marker, then
    // return it. The X360 loads the f32 and returns it in a double register, so the
    // f32 is widened to f64 here. Path/line from the baked assert string
    // (GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h:250).
    f64 GuiEventTimeInfo::GetTime() const
    {
        CGS_ASSERT_AT(mfTimeNow != -FLT_MAX,
            "mfTimeNow!=-FLT_MAX",
                "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsGuiEventTypeDefs.h", 250 /* ARTIST 0x8240E358 */);

        return mfTimeNow;
    }
}
