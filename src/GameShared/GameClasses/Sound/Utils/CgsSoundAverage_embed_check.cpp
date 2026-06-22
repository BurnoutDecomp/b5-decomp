// Embed-check for CgsSoundAverage.h:
//   CgsSound::Utils::Average<N,float>::Record  for N in {3,4,5,9,10,25}
//     @0x826A8138 / 0x826A8580 / 0x826A80A8 / 0x826A8348 / 0x826A8218 / 0x826A83D8
//
// Exercises the bodied Record() on each emitted instantiation and verifies the
// circular-write cursor, the full-window resum, and the (1/N) mean.

#include "GameShared/GameClasses/Sound/Utils/CgsSoundAverage.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

using namespace CgsSound::Utils;

// Layout invariant the asm depends on: N sample floats, then a byte cursor, then
// the cached mean float aligned to 4. ABI-stable (all floats + 1 byte), so an
// absolute-size check is legal here.
static_assert(sizeof(Average<3u, f32>)  == (3u  * sizeof(f32)) + 4u + sizeof(f32), "<3> layout");
static_assert(sizeof(Average<25u, f32>) == (25u * sizeof(f32)) + 4u + sizeof(f32), "<25> layout");

static bool fnearly(f32 a, f32 b)
{
    f32 d = a - b;
    if (d < 0.0f) { d = -d; }
    return d <= 1.0e-4f;
}

template <u32 N>
static void exercise_window()
{
    Average<N, f32> lAvg; // Reset() zeroes the buffer + cursor + mean.

    // One sample of 1.0 into a zeroed window: sum == 1.0, mean == 1/N.
    lAvg.Record(1.0f);
    CGS_ASSERT(fnearly(lAvg.GetAverage(), 1.0f / static_cast<f32>(N)), "single-sample mean == 1/N");

    // Fill the whole window with 2.0: every slot is 2.0, mean == 2.0.
    for (u32 lu = 0; lu < N; ++lu)
    {
        lAvg.Record(2.0f);
    }
    CGS_ASSERT(fnearly(lAvg.GetAverage(), 2.0f), "full window of 2.0 -> mean 2.0");

    // One more wraps the cursor and overwrites the oldest slot with 4.0.
    // Window is then (N-1) twos + one four; mean == (2*(N-1)+4)/N.
    lAvg.Record(4.0f);
    const f32 lExpected = (2.0f * static_cast<f32>(N - 1) + 4.0f) / static_cast<f32>(N);
    CGS_ASSERT(fnearly(lAvg.GetAverage(), lExpected), "wrapped window mean");
}

int main()
{
    exercise_window<3u>();
    exercise_window<4u>();
    exercise_window<5u>();
    exercise_window<9u>();
    exercise_window<10u>();
    exercise_window<25u>();
    return 0;
}
