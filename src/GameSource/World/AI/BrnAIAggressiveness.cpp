#include "GameSource/World/AI/BrnAIAggressiveness.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnAI::Aggressiveness -- the three speed-match setters. Each one bounds-checks its
// argument to [0,1] and stores it into the matching f32 member. The X360 build fires a
// dev-assert on an out-of-range value: it streams a descriptive message ("Bad
// percentage is " / "Bad Proximity is " / "Bad speed match time is " followed by the
// value) into the assert buffer and calls FireAssert with the baked
// World/AI/BrnAIAggressiveness.h file/line. CGS_ASSERT folds that Begin/Fire/End
// sequence and forwards the message directly (dropping the streamed value and the baked
// d:\p4 file/line in favour of __FILE__/__LINE__). All three are called from
// AICar::SetRoadRageMadness, AICar::OnModeStart and AICar::Reset.

namespace BrnAI
{
    // @0x827645E0 -- store mfProximitySpeedMatch (this+0x8).
    void Aggressiveness::SetProximityToSpeedMatch(f32 lfValue)
    {
        CGS_ASSERT(lfValue >= 0.0f && lfValue <= 1.0f, "Bad Proximity is ");
        mfProximitySpeedMatch = lfValue;
    }

    // @0x827646C8 -- store mfAcclerationRateForSpeedMatch (this+0x14).
    void Aggressiveness::SetAcclerationRateForSpeedMatch(f32 lfValue)
    {
        CGS_ASSERT(lfValue >= 0.0f && lfValue <= 1.0f, "Bad percentage is ");
        mfAcclerationRateForSpeedMatch = lfValue;
    }

    // @0x827647B0 -- store mfTimeForSpeedMatch (this+0xC).
    void Aggressiveness::SetTimeForSpeedMatch(f32 lfValue)
    {
        CGS_ASSERT(lfValue >= 0.0f && lfValue <= 1.0f, "Bad speed match time is ");
        mfTimeForSpeedMatch = lfValue;
    }
}
