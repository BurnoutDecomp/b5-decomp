#include "GameSource/Sound/Collision/BrnCollisionFrameInformation.h"

// Exercise BrnSound::Logic::FrameInformation::IsPaused @ 0x826958E8 and
// UpdateFatalityFlag @ 0x826ADD18.
namespace
{
void ExerciseFrameInformation()
{
    BrnSound::Logic::FrameInformation lFrame;

    // IsPaused: clear -> not paused; set a pause bit -> paused.
    bool lbPaused = lFrame.IsPaused();
    (void)lbPaused;
    lFrame.maPaused.SetBit(0);
    lbPaused = lFrame.IsPaused();
    (void)lbPaused;

    // Fatality state machine walk-through:
    //   OFF + crash -> START (arms the 5-frame window)
    lFrame.maPaused.UnSetBit(0);
    lFrame.meFatality.Flush(BrnSound::Logic::E_FATAL_OFF);
    lFrame.UpdateFatalityFlag(true);   // -> E_FATAL_START, count = 5

    // count down through the start window -> commit to ON on the 5th frame.
    for (int li = 0; li < (int)BrnSound::Logic::KU32_FATAL_START_FRAME_COUNT; ++li)
    {
        lFrame.UpdateFatalityFlag(true);
    }

    // crash input drops -> back to OFF.
    lFrame.UpdateFatalityFlag(false);
    (void)lFrame.meFatality.GetCurrent();
}
}

// FrameInformation begins with the 64-byte player transform (the X360 places
// meFatality at +64 == result[16]); confirm the transform is the first member and
// is 64 bytes wide (4x4 f32 affine).
static_assert(sizeof(Matrix44Affine) == 16 * sizeof(f32),
              "Matrix44Affine must be a 4x4 f32 matrix (64 bytes)");
