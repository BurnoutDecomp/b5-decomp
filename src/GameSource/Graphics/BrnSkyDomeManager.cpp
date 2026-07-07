#include "GameSource/Graphics/BrnSkyDomeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor
#include "pc/gcm/renderengine/Xbox2VertexBufferShims.h"              // D3DVertexBuffer_Lock/Unlock, Xbox2UnSet
#include "rw/rwcore_structs.h"                                       // rw::IResourceAllocator, rw::Resource
#include "rw/math/fpu/matrix33_operation.h"                          // Matrix33FromYRotationAngle, Mult

#include <cmath>   // sqrtf / fabsf / sinf / cosf

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSkyDomeManager::Prepare            @ 0x82408DF0
//   BrnSkyDomeManager::RaySphereDistance  @ 0x823FD468
//   BrnSkyDomeManager::CreateGeometry     @ 0x824076D8
//
// RaySphereDistance is fully inlined in the X360 image -- despite the assert string naming
// RwMathFPU::{Magnitude,IsSimilar}, the compiler emitted straight scalar float arithmetic
// (lfs/fmuls/fmadds/fsqrts/fabs), with no call to any rw::math::fpu helper. It is therefore
// reconstructed here store-for-store as plain scalar math; the two rodata constants are
// flt_82001C98 = 1.0f (target magnitude) and flt_82013F90 = 0.001f (similarity epsilon).
//
// CreateGeometry allocates one sky-dome mesh (slices x sectors + apex) through the resource
// allocator, exactly like EffectsVertexBufferManager::Construct: build a Parameters block,
// GetResourceDescriptor, allocator->DoAllocate, Initialize, then lock/write/unlock. The
// per-vertex normal is the current Y-rotation matrix applied to (cos(elev), sin(elev), 0),
// with the elevation ramped as t^2 * (pi/2) across the slices; each vertex stores
// {normal.xyz, RaySphereDistance(normal), sqrt(x^2 + z^2)}. The index buffer is a stitched
// triangle strip (primitive type 6) with degenerate restarts between rings and a fan cap to
// the apex. The two un-homed rwmath helpers (Matrix33FromYRotationAngle / Mult) are
// reconstructed in rw/math/fpu/matrix33_operation.h.

// BrnSkyDomeManager::Prepare  X360 0x82408DF0
// Allocate both sky-dome meshes: a high-density main dome (45 slices x 22 sectors) and a
// coarse cube-map dome (10 x 5). Always succeeds.
bool BrnSkyDomeManager::Prepare(
    BrnGraphics::Im3dSkyDome* lpRenderer,
    rw::IResourceAllocator* lpAlloc)
{
    CGS_ASSERT(lpAlloc != NULL, "lpAlloc != NULL");

    // X360 Prepare @0x82408E30/E54: li r9(liSliceCount)=22/5, r10(liSectorCount)=45/10 -- the
    // committed literals were slice<->sector swapped (yielded indexCount 2158 vs 2066 + wrong
    // sectorAngle 2pi/22 vs 2pi/45). Corrected to (sliceCount, sectorCount) = (22,45) and (5,10).
    CreateGeometry(lpRenderer, lpAlloc,
                   &mpMainVertexBuffer, &mpMainIndexBuffer, &mMainDrawParameters,
                   22, 45);
    CreateGeometry(lpRenderer, lpAlloc,
                   &mpCubeVertexBuffer, &mpCubeIndexBuffer, &mCubeDrawParameters,
                   5, 10);
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

// BrnSkyDomeManager::CreateGeometry  X360 0x824076D8
// Allocate and fill one sky-dome mesh: a ring of liSectorCount vertices per liSliceCount
// elevation slices plus a single apex vertex, tessellated as a stitched triangle strip.
// Vertex stride is 20 bytes (5 floats): {normal.x, normal.y, normal.z, ray-sphere distance,
// sqrt(normal.x^2 + normal.z^2)}. Reconstructed store-for-store against the inlined asm.
void BrnSkyDomeManager::CreateGeometry(
    BrnGraphics::Im3dSkyDome* lpRenderer,
    rw::IResourceAllocator* lpAlloc,
    renderengine::VertexBuffer** lppVertexBuffer,
    renderengine::IndexBuffer** lppIndexBuffer,
    DrawIndexedParameters* lpDrawIndexedParams,
    int32_t liSliceCount,
    int32_t liSectorCount)
{
    (void)lpRenderer;   // unused on X360 (the renderer arg is never read by CreateGeometry)

    // The sphere the rays are cast against, and the two per-sector Y-rotation matrices.
    float laSkyCenter[3] = { 0.0f, -99000.0f, 0.0f };   // flt_82047BB4 = -99000.0f
    const float lfSphereRadius = 100000.0f;             // flt_820080E8

    const float lfSectorAngle = 6.2831855f / static_cast<float>(liSectorCount);   // 2*pi / sectors
    float laRotSector[9];
    rw::math::fpu::Matrix33FromYRotationAngle<float>(laRotSector, lfSectorAngle);
    float laRotHalfSector[9];
    rw::math::fpu::Matrix33FromYRotationAngle<float>(laRotHalfSector, lfSectorAngle * 0.5f);

    CGS_ASSERT(lpAlloc, "lpAlloc");
    CGS_ASSERT(lppVertexBuffer, "lppVertexBuffer");
    CGS_ASSERT(lppIndexBuffer, "lppIndexBuffer");
    CGS_ASSERT(lpDrawIndexedParams, "lpDrawIndexedParams");

    const int32_t liVertexCount = liSliceCount * liSectorCount + 1;
    const int32_t liSliceCountMinusOne = liSliceCount - 1;
    const int32_t liIndexCount =
        (2 * liSectorCount + 3) * (liSliceCount - 1) + 2 * liSectorCount + liSliceCount + 1;

    // ---- Vertex buffer -------------------------------------------------------------------
    renderengine::VertexBuffer::Parameters lVbParams;
    lVbParams.muFormat = 0u;
    lVbParams.muLength = static_cast<u32>(20 * liVertexCount);

    CgsResource::ResourceDescriptor lVbResDesc;
    renderengine::VertexBuffer::GetResourceDescriptor(
        reinterpret_cast<u64*>(&lVbResDesc),
        static_cast<int>(reinterpret_cast<usize>(&lVbParams)));

    rw::Resource lVbResource = lpAlloc->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>(lVbResDesc), 0);
    CGS_ASSERT(lVbResource.m_baseResources[0], "vbResource.GetMemoryResource()");

    renderengine::VertexBufferHeader* lpVbHeader = renderengine::VertexBuffer::Initialize(
        reinterpret_cast<renderengine::VertexBuffer::Wrapper*>(&lVbResource), &lVbParams, 0, 0);
    *lppVertexBuffer = reinterpret_cast<renderengine::VertexBuffer*>(lpVbHeader);
    CGS_ASSERT(*lppVertexBuffer, "NULL != (*lppVertexBuffer)");

    lpDrawIndexedParams->muBaseVertexIndex = 0u;

    renderengine::VertexBuffer_Xbox2UnSet(lpVbHeader);
    u8* const lpVertexBase = reinterpret_cast<u8*>(static_cast<usize>(static_cast<u32>(
        renderengine::D3DVertexBuffer_Lock(lpVbHeader, 0, 0, 0))));

    float laCurrent[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };   // identity
    int32_t liVertex = 0;
    const float lfElevationStep = 1.0f / static_cast<float>(liSliceCount);
    float lfElevationAccum = 0.0f;
    float lfElevationParam = 0.0f;
    for (int32_t liSlice = 0; liSlice < liSliceCount; ++liSlice)
    {
        const float lfElevation = (lfElevationParam * lfElevationParam) * 1.5707964f;   // t^2 * pi/2
        const float lfSin = static_cast<float>(std::sin(lfElevation));
        const float lfCos = static_cast<float>(std::cos(lfElevation));

        float* lpWrite = reinterpret_cast<float*>(lpVertexBase + 20 * liVertex);
        liVertex += liSectorCount;
        for (int32_t liSector = 0; liSector < liSectorCount; ++liSector)
        {
            // normal = current * (cos, sin, 0)  (column-major; third column * 0)
            const float lfNormalX = laCurrent[0] * lfCos + laCurrent[3] * lfSin + laCurrent[6] * 0.0f;
            const float lfNormalY = laCurrent[1] * lfCos + laCurrent[4] * lfSin + laCurrent[7] * 0.0f;
            const float lfNormalZ = laCurrent[2] * lfCos + laCurrent[5] * lfSin + laCurrent[8] * 0.0f;

            float laDirection[3] = { lfNormalX, lfNormalY, lfNormalZ };
            const float lfDistance = RaySphereDistance(
                reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laDirection[0]),
                reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laSkyCenter[0]),
                lfSphereRadius);

            lpWrite[0] = lfNormalX;
            lpWrite[1] = lfNormalY;
            lpWrite[2] = lfNormalZ;
            lpWrite[3] = lfDistance;
            lpWrite[4] = static_cast<float>(std::sqrt(lfNormalX * lfNormalX + lfNormalZ * lfNormalZ));
            lpWrite += 5;

            float laNext[9];
            rw::math::fpu::Mult<float>(laNext, laRotSector, laCurrent);
            for (int32_t liElement = 0; liElement < 9; ++liElement)
            {
                laCurrent[liElement] = laNext[liElement];
            }
        }

        lfElevationAccum += lfElevationStep;
        if (liSlice % 2)
        {
            // Odd slice: reset the rotation to identity for the next ring.
            laCurrent[0] = 1.0f; laCurrent[1] = 0.0f; laCurrent[2] = 0.0f;
            laCurrent[3] = 0.0f; laCurrent[4] = 1.0f; laCurrent[5] = 0.0f;
            laCurrent[6] = 0.0f; laCurrent[7] = 0.0f; laCurrent[8] = 1.0f;
        }
        else
        {
            // Even slice: offset the next ring by half a sector (staggered strip).
            for (int32_t liElement = 0; liElement < 9; ++liElement)
            {
                laCurrent[liElement] = laRotHalfSector[liElement];
            }
        }
        lfElevationParam = lfElevationAccum;
    }

    // Apex vertex (straight up).
    {
        float laDirection[3] = { 0.0f, 1.0f, 0.0f };
        const float lfDistance = RaySphereDistance(
            reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laDirection[0]),
            reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laSkyCenter[0]),
            lfSphereRadius);
        float* lpApex = reinterpret_cast<float*>(lpVertexBase + 20 * liVertex);
        lpApex[0] = 0.0f;
        lpApex[1] = 1.0f;
        lpApex[2] = 0.0f;
        lpApex[3] = lfDistance;
        lpApex[4] = static_cast<float>(std::sqrt(0.0f));
    }
    CGS_ASSERT(liVertex + 1 == liVertexCount, "liVertex == liVertexCount");

    renderengine::D3DVertexBuffer_Unlock(lpVbHeader);

    // Cache the horizon (+X) and zenith (+Y) ray-sphere distances on the manager.
    {
        float laDirection[3] = { 1.0f, 0.0f, 0.0f };
        mrHorizonDistance = RaySphereDistance(
            reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laDirection[0]),
            reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laSkyCenter[0]),
            lfSphereRadius);
    }
    {
        float laDirection[3] = { 0.0f, 1.0f, 0.0f };
        mrZenithDistance = RaySphereDistance(
            reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laDirection[0]),
            reinterpret_cast<const rw::math::fpu::Vector3Template<float>&>(laSkyCenter[0]),
            lfSphereRadius);
    }

    // ---- Index buffer --------------------------------------------------------------------
    renderengine::IndexBufferParameters lIbParams;
    lIbParams.muField00 = 0u;
    lIbParams.muFormat = 16u;                            // 16-bit indices
    lIbParams.muCount = static_cast<u32>(liIndexCount);

    // Inlined IndexBuffer::GetResourceDescriptor: five {size, alignment} entries, identity by
    // default, with slot 0 = the 0x24-byte header and slot 2 = the rounded index bytes.
    CgsResource::ResourceDescriptor lIbResDesc;
    for (int32_t liEntry = 0; liEntry < 5; ++liEntry)
    {
        lIbResDesc.m_baseResourceDescriptors[liEntry].m_size = 0u;
        lIbResDesc.m_baseResourceDescriptors[liEntry].m_alignment = 1u;
    }
    lIbResDesc.m_baseResourceDescriptors[0].m_size = 0x24u;
    lIbResDesc.m_baseResourceDescriptors[0].m_alignment = 4u;
    lIbResDesc.m_baseResourceDescriptors[2].m_size =
        static_cast<u32>((2 * liIndexCount + 15) & ~15);
    lIbResDesc.m_baseResourceDescriptors[2].m_alignment = 4u;

    rw::Resource lIbResource = lpAlloc->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>(lIbResDesc), 0);
    CGS_ASSERT(lIbResource.m_baseResources[0], "ibResource.GetMemoryResource()");

    renderengine::IndexBufferHeader* lpIbHeader = renderengine::IndexBuffer::Initialize(
        reinterpret_cast<renderengine::IndexBufferWrapper*>(&lIbResource), &lIbParams);
    *lppIndexBuffer = reinterpret_cast<renderengine::IndexBuffer*>(lpIbHeader);
    CGS_ASSERT(*lppIndexBuffer, "(*lppIndexBuffer)");

    lpDrawIndexedParams->muMinVertexIndex = 0u;
    lpDrawIndexedParams->muNumVertices = static_cast<u32>(liIndexCount);
    lpDrawIndexedParams->mePrimitiveType = 6u;

    renderengine::IndexBufferLockInfo lLockInfo;
    const int liLocked = renderengine::IndexBuffer::Lock(
        reinterpret_cast<renderengine::IndexBufferHeader*>(*lppIndexBuffer), 2, &lLockInfo);
    CGS_ASSERT(liLocked,
               "(*lppIndexBuffer)->Lock(renderengine::IndexBuffer::LOCK_WRITE, lLockedBuffer)");

    u16* const lpIndices = reinterpret_cast<u16*>(lLockInfo.mpBits);
    int32_t liInd = 0;
    int32_t liSlice = 0;
    if (liSliceCountMinusOne > 0)
    {
        int32_t liRowBase = 0;
        do
        {
            const int32_t liNextRowBase = liRowBase + liSectorCount;
            int32_t liLower = liRowBase;
            int32_t liUpper = liRowBase + liSectorCount;
            if (liSlice)
            {
                lpIndices[liInd++] = static_cast<u16>((liSlice % 2) ? liNextRowBase : liRowBase);
            }
            if (liSectorCount > 0)
            {
                int32_t liRemaining = liSectorCount;
                do
                {
                    if (liSlice % 2)
                    {
                        lpIndices[liInd] = static_cast<u16>(liUpper);
                        lpIndices[liInd + 1] = static_cast<u16>(liLower);
                    }
                    else
                    {
                        lpIndices[liInd] = static_cast<u16>(liLower);
                        lpIndices[liInd + 1] = static_cast<u16>(liUpper);
                    }
                    liInd += 2;
                    --liRemaining;
                    ++liLower;
                    ++liUpper;
                }
                while (liRemaining);
            }
            int32_t liDegenerate;
            if (liSlice % 2)
            {
                liDegenerate = liRowBase;
                lpIndices[liInd] = static_cast<u16>(liNextRowBase);
            }
            else
            {
                liDegenerate = liRowBase + liSectorCount;
                lpIndices[liInd] = static_cast<u16>(liRowBase);
            }
            ++liSlice;
            liRowBase += liSectorCount;
            lpIndices[liInd + 1] = static_cast<u16>(liDegenerate);
            lpIndices[liInd + 2] = static_cast<u16>(liDegenerate);
            liInd += 3;
        }
        while (liSlice < liSliceCountMinusOne);
    }

    const int32_t liLastRowBase = liSlice * liSectorCount;
    CGS_ASSERT(liSlice != 0, "liSlice != 0");

    int32_t liCap = liInd + 2;
    lpIndices[liInd] = static_cast<u16>(liLastRowBase);
    lpIndices[liInd + 1] = static_cast<u16>(liLastRowBase);
    int32_t liRunning = liLastRowBase;
    if (liSectorCount > 1)
    {
        int32_t liRemaining = liSectorCount - 1;
        do
        {
            lpIndices[liCap] = static_cast<u16>(liVertex);   // apex vertex index
            ++liRunning;
            lpIndices[liCap + 1] = static_cast<u16>(liRunning);
            liCap += 2;
            --liRemaining;
        }
        while (liRemaining);
    }
    lpIndices[liCap] = static_cast<u16>(liVertex);           // apex vertex index
    lpIndices[liCap + 1] = static_cast<u16>(liLastRowBase);
    lpIndices[liCap + 2] = static_cast<u16>(liLastRowBase);
    CGS_ASSERT(liCap + 3 == liIndexCount, "liInd == liIndexCount");

    renderengine::IndexBuffer::Unlock(
        reinterpret_cast<renderengine::IndexBufferHeader*>(*lppIndexBuffer), &lLockInfo);
}
