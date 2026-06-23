#pragma once

#include "types.hpp"

// CgsGui::GuiEventTimeInfo - the per-frame time payload carried on GUI update events.
// GetTime() returns the "now" stamp (a 32-bit float widened to double) after asserting
// it has been initialised (i.e. is not the -FLT_MAX "unset" marker).
//
// Layout from BURNOUT_X360_ARTIST.XEX @ 0x8240E328 (GetTime): mfTimeNow is read with
// `lfs ...,4(this)` so it sits at +0x04, and is compared against -FLT_MAX before being
// returned. Path/line from the baked assert string
// (GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h:250).
namespace CgsGui
{
    class GuiEventTimeInfo
    {
    public:
        // @ 0x8240E328 - assert mfTimeNow has been set (!= -FLT_MAX) and return it
        // widened to double, matching the X360's lfs->return of the f32.
        f64 GetTime() const;

    private:
        f32 mfTimeDelta;    // +0x00 - leading frame-delta companion to mfTimeNow
        f32 mfTimeNow;      // +0x04 - current time stamp (-FLT_MAX while unset)
    };

    static_assert(sizeof(GuiEventTimeInfo) == 8, "GuiEventTimeInfo: mfTimeNow at +0x04");
}
