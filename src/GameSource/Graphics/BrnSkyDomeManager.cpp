#include "GameSource/Graphics/BrnSkyDomeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>   // sqrtf / fabsf

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSkyDomeManager::Prepare            @ 0x82408DF0
//   BrnSkyDomeManager::RaySphereDistance  @ 0x823FD468
//
// RaySphereDistance is fully inlined in the X360 image -- despite the assert string naming
// RwMathFPU::{Magnitude,IsSimilar}, the compiler emitted straight scalar float arithmetic
// (lfs/fmuls/fmadds/fsqrts/fabs), with no call to any rw::math::fpu helper. It is therefore
// reconstructed here store-for-store as plain scalar math; the two rodata constants are
// flt_82001C98 = 1.0f (target magnitude) and flt_82013F90 = 0.001f (similarity epsilon).
//
// CreateGeometry (@0x824076D8) is declared in the header but remains BLOCKED: its faithful
// body needs collaborators that are not homed in the committed tree in a callable/typed form
// -- rw::math::fpu::Matrix33FromYRotationAngle<float> (no home), the rw::IResourceAllocator
// virtual Allocate(ResourceDescriptor,Resource) surface, and a VertexBufferHeader*/
// IndexBufferHeader* return that conflicts with the manager's renderengine::VertexBuffer*/
// IndexBuffer* member types. Grounding those would require fabricating un-homed API, so
// CreateGeometry is left declared-but-undefined.

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

// BrnSkyDomeManager::RaySphereDistance  X360 0x823FD468
// Given a unit ray direction from the origin and a sphere (centre, radius), return the signed
// distance along the ray at which it meets the sphere. The X360 build inlines the whole thing
// as scalar float math; the three lanes of each Vector3Template<float> are read directly at
// byte offsets +0/+4/+8 (mX/mY/mZ), matching the lfs 0/4/8 loads in the asm.
float BrnSkyDomeManager::RaySphereDistance(
    const rw::math::fpu::Vector3Template<float>& lDirection,
    const rw::math::fpu::Vector3Template<float>& lSphereCenter,
    float lSphereRadius) const
{
    const float* lpDirection = reinterpret_cast<const float*>(&lDirection);
    const float* lpCenter = reinterpret_cast<const float*>(&lSphereCenter);

    // CGS_ASSERT( IsSimilar( Magnitude(lDirection), 1.0f, 0.001f ) )
    const float lrDirectionMagnitude = sqrtf(
        (lpDirection[2] * lpDirection[2])
        + ((lpDirection[0] * lpDirection[0]) + (lpDirection[1] * lpDirection[1])));
    CGS_ASSERT(fabsf(lrDirectionMagnitude - 1.0f) < 0.001f,
               "RwMathFPU::IsSimilar( RwMathFPU::Magnitude( lDirection ), 1.0f, 0.001f )");

    const float lCenterDotDirection =
        (lpCenter[2] * lpDirection[2])
        + ((lpDirection[0] * lpCenter[0]) + (lpCenter[1] * lpDirection[1]));
    const float lCenterDistanceSqr =
        (lpCenter[2] * lpCenter[2])
        + ((lpCenter[0] * lpCenter[0]) + (lpCenter[1] * lpCenter[1]));
    const float lRadiusSqr = lSphereRadius * lSphereRadius;
    const float lMSqr = lCenterDistanceSqr - (lCenterDotDirection * lCenterDotDirection);
    const float lQ = sqrtf(lRadiusSqr - lMSqr);

    if (lCenterDistanceSqr <= lRadiusSqr)
    {
        return lQ + lCenterDotDirection;
    }
    return lCenterDotDirection - lQ;
}
