#include "GameSource/Graphics/BrnSkyDomeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include "BrnCommonTypes.h"                                          // Matrix44 / Vector3 / Vector3Plus / Vector4
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"             // the per-frame sky/cloud constants
#include "GameSource/Graphics/ImmediateMode/BrnIm3d.h"               // BrnGraphics::Im3dSkyDome
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h" // shadow::Device
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"  // CgsResource::ResourceDescriptor
#include "pc/gcm/renderengine/VertexDescriptor.h"                    // renderengine::VertexDescriptorData
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

// =============================================================================================
// The draw path (added 2026-07-29).
//
//   BrnSkyDomeManager::Construct              -- PS3 DecFIGS @0x350588 (inlined on X360)
//   BrnSkyDomeManager::Release                -- PS3 DecFIGS @0x350D6C (inlined on X360)
//   BrnSkyDomeManager::Render                 -- X360 @0x82400238
//   BrnSkyDomeManager::RenderToEnvironmentMap -- X360 @0x82400580
//
// Render and RenderToEnvironmentMap are the same routine over different buffers: Render draws
// the dense main dome (22x45) with the frame's view-projection into the back buffer;
// RenderToEnvironmentMap draws the coarse cube dome (5x10) with the env-map face's
// view-projection. Every constant they push comes out of BrnShaderConstantsFrame; the two cloud
// textures are the caller's layer-0 density + lighting maps, and BOTH must be non-null or the
// whole draw is skipped (the X360's two leading null checks).
//
// Neither Construct nor Release survives as a standalone X360 symbol (the compiler inlined both
// into the renderer module), so their bodies come from the DecFIGS Internal PS3 build, where the
// same source line numbers are attested (BrnSkyDomeManager.cpp:35 / :93). Both are trivial and
// member-for-member identical across the two platforms.
// =============================================================================================

// ---- the three shared immediate-mode render states the sky pass installs ----------------------
// X360 .data slots off dword_83010F20: the sky binds a blend state, a rasteriser state and a
// depth/stencil state through CgsGraphics::ImRendererBase::SetState's three overloads
// (sub_82276A68 blend / sub_82276B38 rasteriser / sub_82276AD0 depth-stencil, each a shadow-cached
// compare-then-apply). The MAIN pass and the ENV-MAP pass share the blend and rasteriser states
// and differ only in the depth/stencil state.
//
// These are engine-wide state objects built by the Cgs*StateFactory modules; no project TU homes
// them yet, so they are named externals exactly like gpBillboardDepthStencilState
// (CgsBillboardRenderer.cpp) and gpLionParticleSamplerState (ParticleRender.cpp).

// X360 dword_83010F38 -- the sky pass blend state (shared by both passes).
extern void* gpSkyDomeBlendState;
// X360 dword_83010F3C -- the cull-none rasteriser state (shared by both passes; the same object
// CgsGuiViewModule.cpp names as the shared cull-none rasteriser).
extern void* gpSkyDomeRasterizerState;
// X360 dword_83010F4C -- the MAIN sky pass depth/stencil state.
extern void* gpSkyDomeDepthStencilState;
// X360 dword_83010F50 -- the ENV-MAP sky pass depth/stencil state (a different object).
extern void* gpSkyDomeEnvMapDepthStencilState;

// CgsGraphics::ImRendererBase::SetState's three state overloads (X360 sub_82276A68 /
// sub_82276B38 / sub_82276AD0). Each compares against a module-static shadow and, on a change,
// pushes the object through the matching shadow::Device::Xbox2Set*LowLevelShadowed. Declared as
// the minimal external surface, mirroring BrnShadowMapRenderManager.cpp's
// ImDeviceSetDepthStencilState.
void ImDeviceSetBlendState(void* lpState);
void ImDeviceSetRasterizerState(void* lpState);
void ImDeviceSetDepthStencilState(void* lpState);

// The renderengine D3D device singleton the fast-path binders drive (X360 off_83271608), and the
// two Xenon D3DDevice_* thunks the sky draw binds through. Declared as the minimal extern surface,
// matching MeshHelper.cpp / ParticleRender.cpp / shadowingdevice.cpp.
struct IDirect3DDevice9;
extern IDirect3DDevice9* gpD3DDevice;

extern "C"
{
    void D3DDevice_SetIndices(IDirect3DDevice9* lpDevice, void* lpIndexData);
    void D3DDevice_SetStreamSource(IDirect3DDevice9* lpDevice, u32 luStreamNumber,
                                   const void* lpStreamData, u32 luOffsetInBytes,
                                   u32 luStride, u32 luFlags);
    void D3DDevice_DrawIndexedVertices(IDirect3DDevice9* lpDevice, u32 lePrimitiveType,
                                       u32 luBaseVertexIndex, u32 luStartIndex, u32 luIndexCount);
}

namespace
{
    // The X360 reads the vertex stride out of the bound vertex descriptor:
    //     lwz  r11, 0x10(r29)   ; renderer->mpVertexDescriptor
    //     lhz  r10, 8(r11)      ; muElementCount
    //     addi r10, r10, 1
    //     slwi r10, r10, 4      ; (count + 1) * 16
    //     lbzx r7,  r10, r11    ; the stride byte
    // i.e. the per-element stride table is packed immediately AFTER the used elements, at
    // 0x10 * (elementCount + 1) -- exactly where renderengine::VertexDescriptor::Initialize
    // writes it ("Per-stream stride table base = +0x10 * (count + 1) bytes into the object").
    //
    // ⚠ x64: the `0x10 * (count + 1)` cursor is a CONSOLE BYTE OFFSET and is wrong on this
    // target. renderengine::VertexDescriptorData leads with a POINTER (mpDeclaration), so on
    // LLP64 its element array starts at +0x14 rather than +0x10 and the packed stride bytes
    // land at +0x114, not at 0x10*(count+1) -- for the sky's two elements that expression
    // addresses 0x30, which is inside element 1's record. The table is therefore read BY NAME,
    // which is also how the only other consumer reads it (shadow::Device::
    // FlushVertexProgramState, `mauStreamStride[luElement]`), and how the PC leaf that builds
    // the descriptor writes it. Element 0 owns stream 0, which is the stream being bound.
    u32 GetVertexStreamStride(const renderengine::VertexDescriptorData* lpDescriptor)
    {
        if (lpDescriptor == nullptr || lpDescriptor->muElementCount == 0u)
        {
            return 0u;
        }
        return lpDescriptor->mauStreamStride[0];
    }

    // Build the sky shader's Vector4 "g_domeRanges" from the manager's two cached ray-sphere
    // distances and the frame's cloud-distance curve:
    //     x = mrZenithDistance
    //     y = 1 / (mrHorizonDistance - mrZenithDistance)
    //     z = the frame's cloud distance curve
    //     w = 0
    // (The X360 stores a constant into the y lane first -- flt_820473B4 == -13.0f in Render,
    // 0.0f in RenderToEnvironmentMap -- and then overwrites it with the reciprocal. That is a
    // dead store; only the final value is modelled.)
    Vector4 MakeCloudDomeRanges(f32 lrZenithDistance, f32 lrHorizonDistance, f32 lfCloudDistanceCurve)
    {
        Vector4 lRanges;
        lRanges.x = lrZenithDistance;
        lRanges.y = 1.0f / (lrHorizonDistance - lrZenithDistance);
        lRanges.z = lfCloudDistanceCurve;
        lRanges.w = 0.0f;
        return lRanges;
    }

    // Pack the view position and the sky scale into the shader's "ViewPositionAndSkyScale"
    // (X360: three vspltw lane broadcasts of the position + one of the scale, merged through two
    // vperm128 masks and a vsldoi128 -- the rw::math::vpu::Vector3Plus(x, y, z, w) constructor).
    // Both masks were read out of the ARTIST database with headless IDA 9.3, so the packing is
    // decoded rather than assumed:
    //   unk_82CDA3C0 = 00010203 00010203 00010203 14151617  -> vperm(splat(x), splat(y)) = (x,x,x,y)
    //   unk_82CDA400 = 08090A0B 1C1D1E1F 00010203 00010203  -> vperm(splat(z), splat(s)) = (z,s,z,z)
    // then vsldoi128(...,8) takes words {2,3} of the first and {0,1} of the second = (x, y, z, s).
    Vector3Plus MakeViewPositionAndSkyScale(const Vector3& lrViewPosition, f32 lfSkyScale)
    {
        Vector3Plus lPacked;
        lPacked.x = lrViewPosition.x;
        lPacked.y = lrViewPosition.y;
        lPacked.z = lrViewPosition.z;
        lPacked.w = lfSkyScale;
        return lPacked;
    }

    // rw::math::vpu::Matrix44::SetIdentity (the X360 loads the four identity rows out of
    // gIVector / unk_82181510 / unk_82181520 / unk_82181530).
    Matrix44 MakeIdentityMatrix44()
    {
        Matrix44 lM;
        lM.xAxis.x = 1.0f; lM.xAxis.y = 0.0f; lM.xAxis.z = 0.0f; lM.xAxis.w = 0.0f;
        lM.yAxis.x = 0.0f; lM.yAxis.y = 1.0f; lM.yAxis.z = 0.0f; lM.yAxis.w = 0.0f;
        lM.zAxis.x = 0.0f; lM.zAxis.y = 0.0f; lM.zAxis.z = 1.0f; lM.zAxis.w = 0.0f;
        lM.wAxis.x = 0.0f; lM.wAxis.y = 0.0f; lM.wAxis.z = 0.0f; lM.wAxis.w = 1.0f;
        return lM;
    }

    // Inlined rw::math::vpu::Mult(Vector4, Matrix44): one row of the combine (vspltw broadcast +
    // vmaddfp chain). Mirrors BrnShadowMap.cpp's committed MultRow.
    Vector4 MultRow(const Vector4& lrRow, const Matrix44& lrM)
    {
        const f32 lafLanes[4] = { lrRow.x, lrRow.y, lrRow.z, lrRow.w };
        const Vector4* const lapRows[4] = { &lrM.xAxis, &lrM.yAxis, &lrM.zAxis, &lrM.wAxis };

        Vector4 lvOut;
        f32* const lpfOut = &lvOut.x;
        lpfOut[0] = 0.0f; lpfOut[1] = 0.0f; lpfOut[2] = 0.0f; lpfOut[3] = 0.0f;
        for (int liRow = 0; liRow < 4; ++liRow)
        {
            const f32* const lpfRow = &lapRows[liRow]->x;
            for (int liLane = 0; liLane < 4; ++liLane)
            {
                lpfOut[liLane] += lafLanes[liRow] * lpfRow[liLane];
            }
        }
        return lvOut;
    }

    // Inlined rw::math::vpu::Mult(Matrix44, Matrix44) -- the X360's sub_823FF1D0 body, which
    // multiplies the world transform by the view-projection and hands the product to
    // Im3dSkyDome::SetTransform.
    Matrix44 MultMatrix(const Matrix44& lrA, const Matrix44& lrB)
    {
        Matrix44 lOut;
        lOut.xAxis = MultRow(lrA.xAxis, lrB);
        lOut.yAxis = MultRow(lrA.yAxis, lrB);
        lOut.zAxis = MultRow(lrA.zAxis, lrB);
        lOut.wAxis = MultRow(lrA.wAxis, lrB);
        return lOut;
    }

    // The sky dome's world-space radius (X360 flt_820473B0, loaded into the w lane of
    // ViewPositionAndSkyScale by both draw paths).
    const f32 KF_SKY_SCALE = 9500.0f;

    // The cloud dark/lite colours are pushed at DOUBLE their frame value
    // (vspltisw v0,2 / vcfsx v0,v0,0 -> 2.0f, then vmulfp128 on both).
    const f32 KF_CLOUD_COLOUR_SCALE = 2.0f;

    // vmulfp128 of a whole 4-lane vector by a broadcast scalar.
    Vector4 ScaleVector4(const Vector4& lrV, f32 lfScale)
    {
        Vector4 lOut;
        lOut.x = lrV.x * lfScale;
        lOut.y = lrV.y * lfScale;
        lOut.z = lrV.z * lfScale;
        lOut.w = lrV.w * lfScale;
        return lOut;
    }

    // The X360's stream/index binding, byte-for-byte:
    //     D3DDevice_SetStreamSource(dev, 0, vb, 0, 0,      1)   <- clears the previous binding
    //     D3DDevice_SetStreamSource(dev, 0, vb, 0, stride, 1)   <- the descriptor's real stride
    //     D3DDevice_SetIndices     (dev, ib)
    // The doubled SetStreamSource is deliberate (the same idiom the Lion particle path uses at
    // ParticleRender.cpp:90/117): the first call with a zero stride retires whatever format was
    // bound, the second installs the sky-dome format.
    void BindSkyDomeStreams(BrnGraphics::Im3dSkyDome* lpRenderer,
                            renderengine::VertexBuffer* lpVertexBuffer,
                            renderengine::IndexBuffer* lpIndexBuffer)
    {
        D3DDevice_SetStreamSource(gpD3DDevice, 0, lpVertexBuffer, 0, 0, 1);

        const u32 luStride = GetVertexStreamStride(
            reinterpret_cast<const renderengine::VertexDescriptorData*>(
                lpRenderer->GetVertexDescriptorData()));
        D3DDevice_SetStreamSource(gpD3DDevice, 0, lpVertexBuffer, 0, luStride, 1);

        D3DDevice_SetIndices(gpD3DDevice, lpIndexBuffer);
    }

    // The X360 draw: (PrimitiveType, BaseVertexIndex, StartIndex, IndexCount) read straight out of
    // the pass's DrawIndexedParameters block. Note the committed field NAMES read the third and
    // fourth words as muMinVertexIndex / muNumVertices; on this path they carry the start index
    // and the index count (which is exactly what CreateGeometry writes into them).
    void DrawSkyDome(const DrawIndexedParameters& lrParameters)
    {
        D3DDevice_DrawIndexedVertices(gpD3DDevice,
                                      lrParameters.mePrimitiveType,
                                      lrParameters.muBaseVertexIndex,
                                      lrParameters.muMinVertexIndex,
                                      lrParameters.muNumVertices);
    }
}

// BrnSkyDomeManager::Construct  (PS3 DecFIGS 0x350588; inlined on X360)
// Null the four buffer pointers and clear the two cached ray-sphere distances. The two
// DrawIndexedParameters blocks are deliberately left alone -- CreateGeometry fills them.
void BrnSkyDomeManager::Construct()
{
    mpMainVertexBuffer = nullptr;
    mpMainIndexBuffer  = nullptr;
    mpCubeVertexBuffer = nullptr;
    mpCubeIndexBuffer  = nullptr;
    mrHorizonDistance  = 0.0f;
    mrZenithDistance   = 0.0f;
}

// BrnSkyDomeManager::Release  (PS3 DecFIGS 0x350D6C; inlined on X360)
// Asserts the allocator and reports success. The dome's vertex/index buffers are NOT released
// here -- the whole allocator block goes away with the renderer's resource arena.
bool BrnSkyDomeManager::Release(rw::IResourceAllocator* lpAlloc)
{
    CGS_ASSERT(lpAlloc != NULL, "lpAlloc != NULL");
    (void)lpAlloc;
    return true;
}

// BrnSkyDomeManager::Render  X360 0x82400238
// Draw the main sky dome for one frame.
void BrnSkyDomeManager::Render(
    BrnGraphics::Im3dSkyDome* lpRenderer,
    const renderengine::Texture* lpLayer0Density,
    const renderengine::Texture* lpLayer0Lighting,
    const BrnShaderConstantsFrame* lpShaderConstants)
{
    // Both cloud textures must be resident; otherwise the whole pass is skipped.
    if (lpLayer0Density == nullptr || lpLayer0Lighting == nullptr)
    {
        return;
    }

    const Matrix44 lViewProjectionMatrix = lpShaderConstants->GetViewProjectionMatrix();
    const Vector3  lViewPosition         = lpShaderConstants->GetViewPosition();
    const Vector3  lKeyLightDirection    = lpShaderConstants->GetKeyLightDirection();

    const Vector4  lSky_TopColourDrk     = lpShaderConstants->GetTopColourDrk();
    const Vector4  lSky_HorColourPow     = lpShaderConstants->GetHorColourPow();
    const Vector4  lSky_SunColourPow     = lpShaderConstants->GetSunColourPow();
    const Vector3  lSky_HorBleedSclPow   = lpShaderConstants->GetHorBleedSclPow();

    const Vector4  lCloudDarkColour0     = lpShaderConstants->GetCloudDarkColour0();
    const Vector4  lCloudLiteColour0     = lpShaderConstants->GetCloudLiteColour0();
    const Vector4  lCloudTextureScaleAndOffsets0 =
        lpShaderConstants->GetCloudTextureScaleAndOffsets0();

    const Vector4  lCloudLayerDensity    = lpShaderConstants->GetCloudLayerDensity();
    const Vector4  lCloudLayerInvFeather = lpShaderConstants->GetCloudLayerInvFeather();
    const Vector4  lCloudLayerOpacity    = lpShaderConstants->GetCloudLayerOpacity();

    const Vector4  lFogScattering        = lpShaderConstants->GetFogScattering();
    const f32      lfCloudDistanceCurve  = lpShaderConstants->GetCloudDistanceCurve();

    const Vector4 lCloudDomeRanges =
        MakeCloudDomeRanges(mrZenithDistance, mrHorizonDistance, lfCloudDistanceCurve);
    const Matrix44    lWorldTransform            = MakeIdentityMatrix44();
    const Vector3Plus lViewPositionAndSkyScale   = MakeViewPositionAndSkyScale(lViewPosition,
                                                                              KF_SKY_SCALE);

    lpRenderer->BeginRendering();

    // Bind the dome's stream twice, exactly as the X360 does: once with a zero stride to drop the
    // previous binding, then with the descriptor's real stride.
    BindSkyDomeStreams(lpRenderer, mpMainVertexBuffer, mpMainIndexBuffer);

    ImDeviceSetBlendState(gpSkyDomeBlendState);
    ImDeviceSetRasterizerState(gpSkyDomeRasterizerState);
    ImDeviceSetDepthStencilState(gpSkyDomeDepthStencilState);

    const Matrix44 lWorldViewProjection = MultMatrix(lWorldTransform, lViewProjectionMatrix);
    lpRenderer->SetTransform(&lWorldViewProjection);

    lpRenderer->SetConstants(
        lViewPositionAndSkyScale,
        lKeyLightDirection,
        lSky_TopColourDrk,
        lSky_HorColourPow,
        lSky_SunColourPow,
        lSky_HorBleedSclPow,
        lpLayer0Density,
        lpLayer0Lighting,
        lFogScattering,
        ScaleVector4(lCloudDarkColour0, KF_CLOUD_COLOUR_SCALE),
        ScaleVector4(lCloudLiteColour0, KF_CLOUD_COLOUR_SCALE),
        lCloudDomeRanges,
        lCloudTextureScaleAndOffsets0,
        lCloudLayerDensity,
        lCloudLayerInvFeather,
        lCloudLayerOpacity);

    shadow::Device::FlushVertexProgramState();
    DrawSkyDome(mMainDrawParameters);

    lpRenderer->EndRendering();
    shadow::Device::ResetShadowing();
}

// BrnSkyDomeManager::RenderToEnvironmentMap  X360 0x82400580
// Draw the coarse sky dome into one face of the environment map. Identical to Render except for
// the face's view-projection / view position, the CUBE buffers + draw parameters, and the env-map
// depth/stencil state.
void BrnSkyDomeManager::RenderToEnvironmentMap(
    BrnGraphics::EEnvironmentMapFace leFace,
    BrnGraphics::Im3dSkyDome* lpRenderer,
    const renderengine::Texture* lpLayer0Density,
    const renderengine::Texture* lpLayer0Lighting,
    const BrnShaderConstantsFrame* lpShaderConstants)
{
    if (lpLayer0Density == nullptr || lpLayer0Lighting == nullptr)
    {
        return;
    }

    const Matrix44 lViewProjectionMatrix = lpShaderConstants->GetEnvMapViewProjectionMatrix(leFace);
    const Vector3  lViewPosition         = lpShaderConstants->GetEnvMapViewPosition();
    const Vector3  lKeyLightDirection    = lpShaderConstants->GetKeyLightDirection();

    const Vector4  lSky_TopColourDrk     = lpShaderConstants->GetTopColourDrk();
    const Vector4  lSky_HorColourPow     = lpShaderConstants->GetHorColourPow();
    const Vector4  lSky_SunColourPow     = lpShaderConstants->GetSunColourPow();
    const Vector3  lSky_HorBleedSclPow   = lpShaderConstants->GetHorBleedSclPow();

    const Vector4  lCloudDarkColour0     = lpShaderConstants->GetCloudDarkColour0();
    const Vector4  lCloudLiteColour0     = lpShaderConstants->GetCloudLiteColour0();
    const Vector4  lCloudTextureScaleAndOffsets0 =
        lpShaderConstants->GetCloudTextureScaleAndOffsets0();

    const Vector4  lCloudLayerDensity    = lpShaderConstants->GetCloudLayerDensity();
    const Vector4  lCloudLayerInvFeather = lpShaderConstants->GetCloudLayerInvFeather();
    const Vector4  lCloudLayerOpacity    = lpShaderConstants->GetCloudLayerOpacity();

    const Vector4  lFogScattering        = lpShaderConstants->GetFogScattering();
    const f32      lfCloudDistanceCurve  = lpShaderConstants->GetCloudDistanceCurve();

    const Vector4 lCloudDomeRanges =
        MakeCloudDomeRanges(mrZenithDistance, mrHorizonDistance, lfCloudDistanceCurve);
    const Matrix44    lWorldTransform          = MakeIdentityMatrix44();
    const Vector3Plus lViewPositionAndSkyScale = MakeViewPositionAndSkyScale(lViewPosition,
                                                                            KF_SKY_SCALE);

    lpRenderer->BeginRendering();

    BindSkyDomeStreams(lpRenderer, mpCubeVertexBuffer, mpCubeIndexBuffer);

    ImDeviceSetBlendState(gpSkyDomeBlendState);
    ImDeviceSetRasterizerState(gpSkyDomeRasterizerState);
    ImDeviceSetDepthStencilState(gpSkyDomeEnvMapDepthStencilState);

    const Matrix44 lWorldViewProjection = MultMatrix(lWorldTransform, lViewProjectionMatrix);
    lpRenderer->SetTransform(&lWorldViewProjection);

    lpRenderer->SetConstants(
        lViewPositionAndSkyScale,
        lKeyLightDirection,
        lSky_TopColourDrk,
        lSky_HorColourPow,
        lSky_SunColourPow,
        lSky_HorBleedSclPow,
        lpLayer0Density,
        lpLayer0Lighting,
        lFogScattering,
        ScaleVector4(lCloudDarkColour0, KF_CLOUD_COLOUR_SCALE),
        ScaleVector4(lCloudLiteColour0, KF_CLOUD_COLOUR_SCALE),
        lCloudDomeRanges,
        lCloudTextureScaleAndOffsets0,
        lCloudLayerDensity,
        lCloudLayerInvFeather,
        lCloudLayerOpacity);

    shadow::Device::FlushVertexProgramState();
    DrawSkyDome(mCubeDrawParameters);

    lpRenderer->EndRendering();
    shadow::Device::ResetShadowing();
}
