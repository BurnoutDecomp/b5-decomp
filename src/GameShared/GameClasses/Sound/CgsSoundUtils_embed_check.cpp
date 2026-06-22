#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// Exercise CgsSound::Utils::Slope::Slope(const SlopeParams&) @ 0x826A1F70.
namespace
{
void ExerciseSlopeFromParams()
{
    // Degenerate input span -> mfMaxInput must be nudged up by the epsilon.
    CgsSound::Utils::SlopeParams lDegenerate(2.0f, 2.0f, 0.0f, 1.0f);
    CgsSound::Utils::Slope lSlope(lDegenerate);
    (void)lSlope.GetParams();

    // Healthy span -> copied verbatim, no nudge.
    CgsSound::Utils::SlopeParams lHealthy(0.0f, 100.0f, -1.0f, 1.0f);
    CgsSound::Utils::Slope lSlope2(lHealthy);
    (void)lSlope2.GetParams();
}
}

// SlopeParams is exactly four f32 (the four range floats copied by the ctor).
static_assert(sizeof(CgsSound::Utils::SlopeParams) == 4 * sizeof(f32),
              "SlopeParams must be 4 x f32 (min/max input, min/max output)");
// Slope holds only its SlopeParams (DWARF CgsSoundUtils.h:346).
static_assert(sizeof(CgsSound::Utils::Slope) == sizeof(CgsSound::Utils::SlopeParams),
              "Slope must contain only its SlopeParams member");
