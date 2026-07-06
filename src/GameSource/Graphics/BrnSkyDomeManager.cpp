#include "GameSource/Graphics/BrnSkyDomeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSkyDomeManager::Prepare  @ 0x82408DF0
//
// CreateGeometry (@0x824076D8) and RaySphereDistance (@0x823FD468) are declared in the
// header but bodied by their own TUs; they are intentionally not defined here.
//   * CreateGeometry is verify-FAIL (fabricated VB/IB engine API surface in its reconstruction).
//   * RaySphereDistance is BLOCKED here: its body needs rw::math::fpu::{Sqrt, IsSimilar,
//     Magnitude}, none of which exist in the committed tree or the renderware vendor include
//     (only rw::math::vpu::Magnitude / EATech-vpu IsSimilar exist, in the wrong namespace over
//     the wrong vector type). Grounding those would require fabricating three fpu math helpers,
//     which is disallowed, so RaySphereDistance is left declared-but-undefined.

// BrnSkyDomeManager::Prepare  X360 0x82408DF0
// Allocate both sky-dome meshes: a high-density main dome (45 slices x 22 sectors) and a
// coarse cube-map dome (10 x 5). Always succeeds.
bool BrnSkyDomeManager::Prepare(
    BrnGraphics::Im3dSkyDome* lpRenderer,
    rw::IResourceAllocator* lpAlloc)
{
    CGS_ASSERT(lpAlloc != NULL, "lpAlloc != NULL");

    CreateGeometry(lpRenderer, lpAlloc,
                   &mpMainVertexBuffer, &mpMainIndexBuffer, &mMainDrawParameters,
                   45, 22);
    CreateGeometry(lpRenderer, lpAlloc,
                   &mpCubeVertexBuffer, &mpCubeIndexBuffer, &mCubeDrawParameters,
                   10, 5);
    return true;
}
