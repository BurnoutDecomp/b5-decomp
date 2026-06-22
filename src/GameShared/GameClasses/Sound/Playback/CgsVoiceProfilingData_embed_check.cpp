// Embed-check for CgsVoiceProfilingData.h:
//   CgsSound::Playback::VoiceProfilingData<128>::SubmitNewTime @ 0x8268EDB0
//
// Exercises the bodied function on the <128> instantiation and verifies the min/max/
// most-recent tracking, the history shift, and the window-mean (1/128) computation.

#include "GameShared/GameClasses/Sound/Playback/CgsVoiceProfilingData.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

using namespace CgsSound::Playback;

// Layout invariant the asm depends on: stats follow the N-float history contiguously
// (history at object start, then max/min/avg/most-recent). ABI-stable -- all floats,
// no pointer members -- so an absolute-offset check is legal here.
static_assert(sizeof(VoiceProfilingData<128u>) == (128u + 4u) * sizeof(f32),
              "VoiceProfilingData<128>: 128 history floats + 4 stat floats, no padding");

static bool fnearly(f32 a, f32 b)
{
    f32 d = a - b;
    if (d < 0.0f) { d = -d; }
    return d <= 1.0e-4f;
}

static void exercise()
{
    VoiceProfilingData<128u> lData; // Reset() zeroes the window + stats.

    // First sample: start*scale == 2.0f*1.0f == 2.0f. min is the 0.0 sentinel, so it
    // seeds to 2.0; max raises to 2.0; most-recent == 2.0; mean == 2.0/128.
    lData.SubmitNewTime(2.0f, 1.0f);
    CGS_ASSERT(fnearly(lData.GetMostRecentTime(), 2.0f), "most-recent == time");
    CGS_ASSERT(fnearly(lData.GetMaximumTime(), 2.0f), "max seeded to first time");
    CGS_ASSERT(fnearly(lData.GetMinimumTime(), 2.0f), "min seeded off 0.0 sentinel");
    CGS_ASSERT(fnearly(lData.GetTimeByIndex(0), 2.0f), "slot 0 holds newest time");
    CGS_ASSERT(fnearly(lData.GetAverageTime(), 2.0f / 128.0f), "mean == sum/128");

    // Second sample: 1.0f*1.0f == 1.0f. It is < current min(2.0) so min lowers to 1.0;
    // it is < max(2.0) so max stays 2.0; most-recent == 1.0. Window now holds {1,2}
    // (slot0=1 newest, slot1=2 shifted), sum == 3.0, mean == 3.0/128.
    lData.SubmitNewTime(1.0f, 1.0f);
    CGS_ASSERT(fnearly(lData.GetMostRecentTime(), 1.0f), "most-recent updated");
    CGS_ASSERT(fnearly(lData.GetMaximumTime(), 2.0f), "max retained");
    CGS_ASSERT(fnearly(lData.GetMinimumTime(), 1.0f), "min lowered");
    CGS_ASSERT(fnearly(lData.GetTimeByIndex(0), 1.0f), "newest in slot 0");
    CGS_ASSERT(fnearly(lData.GetTimeByIndex(1), 2.0f), "prior shifted to slot 1");
    CGS_ASSERT(fnearly(lData.GetAverageTime(), 3.0f / 128.0f), "mean over window");

    // Third sample larger than max: 5.0f. max raises to 5.0; min stays 1.0.
    lData.SubmitNewTime(5.0f, 1.0f);
    CGS_ASSERT(fnearly(lData.GetMaximumTime(), 5.0f), "max raised");
    CGS_ASSERT(fnearly(lData.GetMinimumTime(), 1.0f), "min retained");
    CGS_ASSERT(fnearly(lData.GetAverageTime(), 8.0f / 128.0f), "mean over {5,1,2}");
}

int main()
{
    exercise();
    return 0;
}
