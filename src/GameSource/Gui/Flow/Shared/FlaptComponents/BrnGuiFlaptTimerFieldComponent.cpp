// ===================================================================================
// BrnGui::FlaptTimerFieldComponent  -- timer colour-band query accessors
//   class:BrnGui::FlaptTimerFieldComponent
//
//   IsTimeSafe      @ 0x824100A8
//   IsTimeDangerous @ 0x82410280
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Both are non-static, non-virtual
// member functions (asm: r3 = this; result returned via clrlwi r3,r11,24 -> bool,
// matching the DWARF 'bool IsTimeSafe()' / 'bool IsTimeDangerous()').
//
// Each branches on meCountingMode (+0x70). In an active counting mode it first
// fires a dev-assert checking that the danger boundary is correctly ordered
// relative to the safe boundary, then returns a single float comparison of
// mfCurrentTime (+0x6C) against the relevant boundary. The X360 emitted that
// dev-assert as a Begin/StrStream/Fire/End sequence that streams the two boundary
// floats into the assert buffer; per this project's house style (see
// BrnAIAggressiveness.cpp and the BrnFlaptFileRef exemplar) that sequence is
// folded into CGS_ASSERT(cond,"msg"), dropping the streamed values and the baked
// d:\p4 file/line in favour of __FILE__/__LINE__.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptTimerFieldComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @ 0x824100A8 -- true while the current time sits in the safe colour band.
    bool FlaptTimerFieldComponent::IsTimeSafe()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            // Counting down: safe boundary is reached before the danger boundary,
            // so the danger boundary must be the lower value.
            CGS_ASSERT(mfDangerColourBoundary < mfSafeColourBoundary,
                       "Danger colour boundry isn't less than the safe colour boundary\n");
            return mfCurrentTime > mfSafeColourBoundary;
        }

        if (meCountingMode != E_TIMER_MODE_COUNTING_UP)
        {
            // Not actively counting -> always considered safe.
            return true;
        }

        // Counting up: the safe boundary is the lower value (danger above it).
        CGS_ASSERT(mfDangerColourBoundary > mfSafeColourBoundary,
                   "Danger colour boundry isn't greater than the safe colour boundary\n");
        return mfCurrentTime < mfSafeColourBoundary;
    }

    // @ 0x82410280 -- true while the current time sits in the danger colour band.
    bool FlaptTimerFieldComponent::IsTimeDangerous()
    {
        if (meCountingMode == E_TIMER_MODE_COUNTING_DOWN)
        {
            CGS_ASSERT(mfDangerColourBoundary < mfSafeColourBoundary,
                       "Danger colour boundry isn't less than the safe colour boundary\n");
            return mfCurrentTime < mfDangerColourBoundary;
        }

        if (meCountingMode != E_TIMER_MODE_COUNTING_UP)
        {
            // Not actively counting -> never considered dangerous.
            return false;
        }

        CGS_ASSERT(mfDangerColourBoundary > mfSafeColourBoundary,
                   "Danger colour boundry isn't greater than the safe colour boundary\n");
        return mfCurrentTime > mfDangerColourBoundary;
    }
}
