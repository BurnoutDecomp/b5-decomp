#include "GameSource/Director/Camera/Utils/CameraUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnDirector::Camera::Utils::TransitionSmoother -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::Utils::TransitionSmoother):
//   TransitionSmoother::Set @0x821F22A0

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// @ 0x821F22A0 -- CameraUtils.h:157/:160 range tripwires (both non-gating; the
// stores land in the asm order: data/target first, then the ideal amount and the
// zeroed live amount between the two guards).
void TransitionSmoother::Set(f32 lfValue, f32 lfLerpAmount0to1,
                             f32 lfLerpAmountLerpAmount0to1,
                             f32 lfSimilarityToleranceScale)
{
    mfData   = lfValue;
    mfTarget = lfValue;

    CGS_ASSERT(lfLerpAmount0to1 >= 0.0f && lfLerpAmount0to1 <= 1.0f,
               "lfLerpAmount0to1 >= 0.0f && lfLerpAmount0to1 <= 1.0f");   // :157

    mfIdealLerpAmount = lfLerpAmount0to1;
    mfLerpAmount      = 0.0f;

    CGS_ASSERT(lfLerpAmountLerpAmount0to1 >= 0.0f && lfLerpAmountLerpAmount0to1 <= 1.0f,
               "lfLerpAmountLerpAmount0to1 >= 0.0f && lfLerpAmountLerpAmount0to1 <= 1.0f");   // :160

    mfLerpAmountLerpAmount     = lfLerpAmountLerpAmount0to1;
    mfSimilarityToleranceScale = lfSimilarityToleranceScale;
}

}
}
}
