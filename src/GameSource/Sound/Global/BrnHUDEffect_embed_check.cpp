#include "GameSource/Sound/Global/BrnHUDEffect.h"

// Exercise BrnSound::Logic::HUDEffect::GameModeData::GameModeData (Construct)
// @ 0x826AFE88 and ::Reset @ 0x826977D8.
namespace
{
void ExerciseGameModeData()
{
    // Construct: every touched field zeroed.
    BrnSound::Logic::HUDEffect::GameModeData lData;
    (void)lData.mEventScoreDelta.GetAverage();
    (void)lData.mStuntScore.GetCurrent();

    // Reset: combo multiplier defaults to 1, time-extended set true.
    lData.Reset();
    (void)lData.mStuntComboMultiplier.GetCurrent();
    (void)lData.mbTimeExtended;
}
}

// The block leads with the 10-sample Average (the X360 ctor zeroes maPoints[0..9]
// first); confirm the 10 f32 samples are the leading storage.
static_assert(sizeof(CgsSound::Utils::Average<10, f32>) >= 10 * sizeof(f32),
              "Average<10,f32> must hold 10 f32 sample points");
