#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"

// BrnDirector::Camera::Utils::CameraImpactEffect -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::Utils::CameraImpactEffect):
//   CameraImpactEffect::RegisterImpact @0x821F3648

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// @ 0x821F3648 -- BrnCameraShake.h:221 tripwire (non-gating), then keep the larger
// of the pending factor and the new magnitude (the asm's branchless
// `fsel(mfImpactFactor - lfImpulseMagnitude, mfImpactFactor, lfImpulseMagnitude)`).
void CameraImpactEffect::RegisterImpact(f32 lfImpulseMagnitude)
{
    CGS_ASSERT(lfImpulseMagnitude >= 0.0f, "lfImpulseMagnitude >= 0.0f");   // :221

    if (mfImpactFactor - lfImpulseMagnitude < 0.0f)
        mfImpactFactor = lfImpulseMagnitude;
}

}
}
}
