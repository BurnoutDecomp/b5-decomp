#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsXboxConditionalRenderShims.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"            // Splat / lane-wise * and +
#include "pc/gcm/renderengine/IndexBuffer.h"
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/Xbox2VertexBufferShims.h"
#include "rw/rwcore_structs.h"                        // IResourceAllocator / Resource

#include <cmath>                                     // std::fabs (the sign-mask clear)
#include <cstring>                                   // std::memcpy
#include <cstdlib>
#include <cstdio>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

struct IDirect3DDevice9;
extern IDirect3DDevice9* gpD3DDevice;

// The Xenon indexed draw (pc/gcm/renderengine/XenonD3D9Shims.cpp), the call the console makes.
extern "C" void D3DDevice_DrawIndexedVertices(IDirect3DDevice9* lpDevice,
                                               u32 lePrimitiveType,
                                               u32 luBaseVertexIndex,
                                               u32 luMinVertexIndex,
                                               u32 luNumVertices);

extern "C"
{
    void D3DDevice_SetStreamSource(IDirect3DDevice9* lpDevice,
                                   u32 luStreamNumber,
                                   const void* lpStreamData,
                                   u32 luOffsetInBytes,
                                   u32 luStride,
                                   u32 luFlags);
    void D3DDevice_SetIndices(IDirect3DDevice9* lpDevice, void* lpIndexData);
}

// CgsOcclusionCullManager.cpp - the hardware-occlusion-query manager's query
// bracket, conditional-render path, and trivial-accept handling.
//
//   BeginQueries                      @ 0x827F25F0
//   EndQueries                        @ 0x827EDF50
//   BeginMeshConditionalRender         @ 0x827E8F18
//   EndMeshConditionalRender           @ 0x827E9020
//   TrivialAcceptOccludeeBoundingBox   @ 0x827E9130
//   RenderOccludeeBoundingBox
//
// Reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly (authoritative for the
// member offsets / store widths). The OcclusionCullManager layout lives in the
// owning header CgsOcclusionCullManager.h. The predicated-draw device calls are the
// Xenon D3D extensions declared in CgsXboxConditionalRenderShims.h (platform
// externals; not reconstructed here).
//
// The query id is read as a u16 from maQueryIds (the asm uses lhzx). Begin compares
// the SIGN-EXTENDED value against -1 (extsh + cmpwi -1); End compares the raw u16
// against 0xFFFF (cmplwi 0xFFFF). Both pick out the same 0xFFFF "trivially
// accepted / no query" sentinel; the widths are reproduced exactly.

namespace CgsGraphics
{
    static_assert(sizeof(Matrix44) == 0x40,
                  "ARTIST Construct allocates one 64-byte Matrix44 per query slot");

    namespace
    {
        // rw::IResourceAllocator::AllocateMemoryResource is inlined in ARTIST. Its descriptor is
        // the five-lane identity record with only lane 0 carrying {size, alignment}; DoAllocate
        // returns the same passed allocator's memory resource.
        void* AllocateMemoryResource(rw::IResourceAllocator* lpAllocator,
                                     u32 luSize,
                                     u32 luAlignment)
        {
            rw::ResourceDescriptor lDescriptor;
            for (u32 luLane = 0u; luLane < 5u; ++luLane)
            {
                lDescriptor.m_baseResourceDescriptors[luLane].m_size = 0u;
                lDescriptor.m_baseResourceDescriptors[luLane].m_alignment = 1u;
            }
            lDescriptor.m_baseResourceDescriptors[0].m_size = luSize;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;

            const rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
            return lResource.m_baseResources[0];
        }
    }

    // CgsGraphics::OcclusionCullManager::Construct @ 0x827F1E00.
    // The vertex/index resources and query-list allocations all use lpAllocator. ARTIST's
    // 0x2000-entry caller requests 0x80000 bytes (512 KiB) for the matrix array; the PC renderer's
    // deferred caller reserves that exact table in sWorldDispatchAllocator before Construct.
    void OcclusionCullManager::Construct(Im3dUntex* lpOccludeeIm3d,
                                         Im3dZOnly* lpOccludeeIm3dZOnly,
                                         rw::IResourceAllocator* lpAllocator,
                                         u32 luQueryListSize)
    {
        mpOccludeeIm3d = lpOccludeeIm3d;
        ImRenderer<PositionOnlyVertex>* const lpPositionOnlyRenderer =
            static_cast<ImRenderer<PositionOnlyVertex>*>(lpOccludeeIm3dZOnly);
        mpOccludeeIm3dZOnly = lpPositionOnlyRenderer;
        mpVertexDescriptor = lpPositionOnlyRenderer->mpVertexDescriptor;

        miOccludeeIndex = 0;
        meOcclusionMode = OCCLUSIONMODE_NONE;
        mNearClipOffset.SetZero();

        mbRenderingMesh = false;
        mbOcclusionEnabled = false;
        mbShowOccludeeBBs = false;
        mbDisplayStats = false;
        mbOcclusionCheapShader = true;
        mbDetectNearClipIntersections = true;
        mbOccludeesDoubleSided = false;
        mbImmediateMode = false;
        mbConditionalRendering = true;

        const f32 lfNegativeExtent = -1.001f;
        const f32 lfPositiveExtent = 1.001f;
        const Vector3 laCubeCorners[8] =
        {
            { lfNegativeExtent, lfPositiveExtent, lfNegativeExtent, 0.0f },
            { lfPositiveExtent, lfPositiveExtent, lfNegativeExtent, 0.0f },
            { lfPositiveExtent, lfNegativeExtent, lfNegativeExtent, 0.0f },
            { lfNegativeExtent, lfNegativeExtent, lfNegativeExtent, 0.0f },
            { lfNegativeExtent, lfNegativeExtent, lfPositiveExtent, 0.0f },
            { lfPositiveExtent, lfNegativeExtent, lfPositiveExtent, 0.0f },
            { lfPositiveExtent, lfPositiveExtent, lfPositiveExtent, 0.0f },
            { lfNegativeExtent, lfPositiveExtent, lfPositiveExtent, 0.0f }
        };
        for (u32 luVertex = 0u; luVertex < 8u; ++luVertex)
        {
            maCubeVertices[luVertex].mv3Pos = laCubeCorners[luVertex];
            maCubeVertices[luVertex].mv4Colour =
                RGBA8{ 0xFFu, static_cast<u8>(luVertex * 8u), 0u, 0xFFu };
            maCubeVerticesZOnly[luVertex].mv3Pos = maCubeVertices[luVertex].mv3Pos;
        }

        mAlwaysFailMatrix.xAxis = { -10.0f, 0.0f, 0.0f, 0.0f };
        mAlwaysFailMatrix.yAxis = { 0.0f, 0.05f, 0.0f, 0.0f };
        mAlwaysFailMatrix.zAxis = { 0.0f, 0.0f, 0.05f, 0.0f };
        mAlwaysFailMatrix.wAxis = { 0.0f, 0.0f, 0.0f, 1.0f };
        mAlwaysPassMatrix.xAxis = { 0.02f, 0.0f, 0.0f, 0.0f };
        mAlwaysPassMatrix.yAxis = { 0.0f, 0.02f, 0.0f, 0.0f };
        mAlwaysPassMatrix.zAxis = { 0.0f, 0.0f, 0.02f, 0.0f };
        mAlwaysPassMatrix.wAxis = { 0.0f, 0.0f, 0.0f, 1.0f };

        const u16 lauCubeIndices[24] =
        {
            0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u,
            3u, 4u, 7u, 0u, 1u, 6u, 5u, 2u,
            2u, 5u, 4u, 3u, 0u, 7u, 6u, 1u
        };
        std::memcpy(mau16CubeIndices, lauCubeIndices, sizeof(mau16CubeIndices));

        // The original position-only descriptor has one vertex element. Its byte offset and
        // per-element stride come from the same typed descriptor data consumed by the PC
        // producer and shadow::Device; do not substitute a guessed vertex stride.
        const renderengine::VertexDescriptorData* const lpDescriptorData =
            reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor);
        const u16 luPositionOffset = lpDescriptorData->maElements[0].muOffset;
        const u8 luPositionStride = lpDescriptorData->mauStreamStride[0];

        renderengine::VertexBuffer::Parameters lVertexBufferParameters;
        lVertexBufferParameters.muFormat = 0u;
        lVertexBufferParameters.muLength = 8u * static_cast<u32>(sizeof(PositionOnlyVertex));
        rw::ResourceDescriptor lVertexBufferDescriptor;
        renderengine::VertexBuffer::GetResourceDescriptor(&lVertexBufferDescriptor,
                                                          &lVertexBufferParameters);
#if !defined(D_PLATFORM_X360)
        // FLAG PC-platform leaf: the selected native Initialize allocates the
        // actual payload in its existing low-4GB arena and consumes only the
        // caller-owned lane0 header. Do not request an unused lane2 block from
        // the real main-only WorldDispatch owner or change its capacities.
        lVertexBufferDescriptor.m_baseResourceDescriptors[2].m_size = 0u;
        lVertexBufferDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
#endif
        rw::Resource lVertexBufferResource = lpAllocator->DoAllocate(lVertexBufferDescriptor, 0);
        CGS_ASSERT(lVertexBufferResource.m_baseResources[0],
                   "couldn't allocate memory for vertex descriptor");

#if defined(D_PLATFORM_X360)
        renderengine::VertexBufferHeader* const lpVertexBufferHeader =
            renderengine::VertexBuffer::Initialize(
                reinterpret_cast<renderengine::VertexBuffer::Wrapper*>(&lVertexBufferResource),
                &lVertexBufferParameters, 0, 0);
#else
        // The native initializer reads this typed header field only; its
        // payload is the real ArenaAlloc result, not a Resource pointer lane.
        renderengine::VertexBuffer::Wrapper lVertexWrapper{};
        lVertexWrapper.mpHeader = static_cast<renderengine::VertexBufferHeader*>(
            lVertexBufferResource.m_baseResources[0]);
        renderengine::VertexBufferHeader* const lpVertexBufferHeader =
            renderengine::VertexBuffer::Initialize(&lVertexWrapper, &lVertexBufferParameters, 0, 0);
#endif
        mpVertexBuffer = reinterpret_cast<renderengine::VertexBuffer*>(lpVertexBufferHeader);
        CGS_ASSERT(lpVertexBufferHeader, "couldn't create a mpVertexBuffer");

        renderengine::VertexBuffer_Xbox2UnSet(lpVertexBufferHeader);
#if defined(D_PLATFORM_X360)
        u8* const lpVertexBufferBytes = reinterpret_cast<u8*>(static_cast<usize>(
            static_cast<u32>(renderengine::D3DVertexBuffer_Lock(lpVertexBufferHeader, 0, 0, 0))));
#else
        renderengine::VertexBuffer::LockInfo lVertexLockInfo{};
        const int liVertexLocked = renderengine::VertexBuffer::Lock(
            lpVertexBufferHeader, 0, &lVertexLockInfo, 0, 0);
        CGS_ASSERT(liVertexLocked, "couldn't lock mpVertexBuffer");
        u8* const lpVertexBufferBytes = static_cast<u8*>(lVertexLockInfo.mpBits);
#endif
        for (u32 luVertex = 0u; luVertex < 8u; ++luVertex)
        {
            std::memcpy(lpVertexBufferBytes + luPositionOffset + luVertex * luPositionStride,
                        &maCubeVerticesZOnly[luVertex], sizeof(PositionOnlyVertex));
        }
        renderengine::D3DVertexBuffer_Unlock(lpVertexBufferHeader);

        renderengine::IndexBufferParameters lIndexBufferParameters;
        lIndexBufferParameters.muField00 = 0u;
        lIndexBufferParameters.muFormat = 16u;
#if defined(D_PLATFORM_X360)
        lIndexBufferParameters.muCount = 24u;
#else
        // FLAG PC-platform leaf: the indexed native consumer accepts triangle
        // lists, not Xenos QUADLIST13. Preserve the six actual box faces as
        // twelve triangles, using the existing non-indexed quad backend's split.
        lIndexBufferParameters.muCount = 36u;
#endif
        rw::ResourceDescriptor lIndexBufferDescriptor;
        for (u32 luLane = 0u; luLane < 5u; ++luLane)
        {
            lIndexBufferDescriptor.m_baseResourceDescriptors[luLane].m_size = 0u;
            lIndexBufferDescriptor.m_baseResourceDescriptors[luLane].m_alignment = 1u;
        }
        lIndexBufferDescriptor.m_baseResourceDescriptors[0].m_size = 0x24u;
        lIndexBufferDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;
        lIndexBufferDescriptor.m_baseResourceDescriptors[2].m_size = 0x30u;
        lIndexBufferDescriptor.m_baseResourceDescriptors[2].m_alignment = 4u;
#if !defined(D_PLATFORM_X360)
        // FLAG PC-platform leaf: IndexBuffer::Initialize likewise owns its
        // actual native triangle payload in the existing mapped arena; this allocator
        // supplies the actual36-byte CPU header, as in the native sky owner.
        lIndexBufferDescriptor.m_baseResourceDescriptors[2].m_size = 0u;
        lIndexBufferDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
#endif
        rw::Resource lIndexBufferResource = lpAllocator->DoAllocate(lIndexBufferDescriptor, 0);
        CGS_ASSERT(lIndexBufferResource.m_baseResources[0],
                   "couldn't allocate memory for index buffer");

#if defined(D_PLATFORM_X360)
        renderengine::IndexBufferHeader* const lpIndexBufferHeader =
            renderengine::IndexBuffer::Initialize(
                reinterpret_cast<renderengine::IndexBufferWrapper*>(&lIndexBufferResource),
                &lIndexBufferParameters);
#else
        renderengine::IndexBufferWrapper lIndexWrapper{};
        lIndexWrapper.mpHeader = static_cast<renderengine::IndexBufferHeader*>(
            lIndexBufferResource.m_baseResources[0]);
        renderengine::IndexBufferHeader* const lpIndexBufferHeader =
            renderengine::IndexBuffer::Initialize(&lIndexWrapper, &lIndexBufferParameters);
#endif
        mpIndexBuffer = reinterpret_cast<renderengine::IndexBuffer*>(lpIndexBufferHeader);
        CGS_ASSERT(lpIndexBufferHeader, "couldn't create mpIndexBuffer");

        renderengine::IndexBufferLockInfo lIndexBufferLockInfo;
        const int liIndexBufferLocked = renderengine::IndexBuffer::Lock(
            lpIndexBufferHeader, 2, &lIndexBufferLockInfo);
        CGS_ASSERT(liIndexBufferLocked, "couldn't lock mpIndexBuffer");
#if defined(D_PLATFORM_X360)
        std::memcpy(lIndexBufferLockInfo.mpBits, mau16CubeIndices, sizeof(mau16CubeIndices));
#else
        u16* const lpuNativeIndices = static_cast<u16*>(lIndexBufferLockInfo.mpBits);
        for (u32 luFace = 0; luFace < 6u; ++luFace)
        {
            const u16* const lpQuad = &mau16CubeIndices[luFace * 4u];
            u16* const lpTriangles = &lpuNativeIndices[luFace * 6u];
            lpTriangles[0] = lpQuad[0]; lpTriangles[1] = lpQuad[1]; lpTriangles[2] = lpQuad[2];
            lpTriangles[3] = lpQuad[0]; lpTriangles[4] = lpQuad[2]; lpTriangles[5] = lpQuad[3];
        }
#endif
        renderengine::IndexBuffer::Unlock(lpIndexBufferHeader, &lIndexBufferLockInfo);

#if defined(D_PLATFORM_X360)
        mDrawParameters.mePrimitiveType = 13u;
#else
        mDrawParameters.mePrimitiveType = 4u; // native indexed triangle list
#endif
        mDrawParameters.muBaseVertexIndex = 0u;
        mDrawParameters.muMinVertexIndex = 0u;
#if defined(D_PLATFORM_X360)
        mDrawParameters.muNumVertices = 24u;
#else
        mDrawParameters.muNumVertices = lIndexBufferParameters.muCount;
#endif

        // ARTIST requests a 0x10-byte console record; the two pointer fields are 64-bit in this
        // PC record, so size this passed-allocator request from the actual host type.
        mpQueryList = static_cast<OcclusionQueryList*>(
            AllocateMemoryResource(lpAllocator, static_cast<u32>(sizeof(OcclusionQueryList)), 0x80u));
        mpQueryList->mpBoxToClipMatrices = nullptr;
        mpQueryList->mpuTrivialAcceptBits = nullptr;
        mpQueryList->muSize = luQueryListSize;
        mpQueryList->muNumQueries = 0u;

        if (luQueryListSize != 0u)
        {
            const u32 luMatrixBytes = luQueryListSize << 6u;
            mpQueryList->mpBoxToClipMatrices = static_cast<Matrix44*>(
                AllocateMemoryResource(lpAllocator, luMatrixBytes, 0x80u));
            const u32 luTrivialBitBytes = 4u * ((luQueryListSize >> 5u) + 1u);
            mpQueryList->mpuTrivialAcceptBits = static_cast<u32*>(
                AllocateMemoryResource(lpAllocator, luTrivialBitBytes, 0x80u));
        }

#if !defined(D_PLATFORM_X360)
        if (std::getenv("BRN_RENDER_PREPARE_DIAG"))
        {
            char lacLine[768];
            std::snprintf(lacLine, sizeof(lacLine),
                "[occlusion-resource] manager=%p allocator=%p vertexHeader=%p vertexBits=%p vertexBytes=%u positionOffset=%u positionStride=%u indexHeader=%p indexBits=%p indexBytes=%u indexStride=%u queryList=%p matrices=%p acceptBits=%p queryCapacity=%u drawIndices=%u\n",
                static_cast<const void*>(this), static_cast<const void*>(lpAllocator),
                static_cast<const void*>(lpVertexBufferHeader), lVertexLockInfo.mpBits,
                lVertexLockInfo.muSize, static_cast<u32>(luPositionOffset), static_cast<u32>(luPositionStride),
                static_cast<const void*>(lpIndexBufferHeader), lIndexBufferLockInfo.mpBits,
                lpIndexBufferHeader->muSizeRounded, lIndexBufferLockInfo.muStride,
                static_cast<const void*>(mpQueryList),
                static_cast<const void*>(mpQueryList->mpBoxToClipMatrices),
                static_cast<const void*>(mpQueryList->mpuTrivialAcceptBits),
                mpQueryList->muSize, mDrawParameters.muNumVertices);
            CgsDev::Log::WriteToLog(lacLine);
        }
#endif

        CGS_ASSERT((reinterpret_cast<usize>(mpQueryList) & 0x7Fu) == 0u,
                   "((uint32_t)mpQueryList) % 128 == 0");
        CGS_ASSERT((reinterpret_cast<usize>(mpQueryList->mpBoxToClipMatrices) & 0x7Fu) == 0u,
                   "((uint32_t)mpQueryList->mpBoxToClipMatrices) % 128 == 0");
        CGS_ASSERT((reinterpret_cast<usize>(mpQueryList->mpuTrivialAcceptBits) & 0x7Fu) == 0u,
                   "((uint32_t)mpQueryList->mpuTrivialAcceptBits ) % 128 == 0");
    }

    // CgsGraphics::OcclusionCullManager::BeginQueries @ 0x827F25F0.
    void OcclusionCullManager::BeginQueries()
    {
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_NONE,
                   "meOcclusionMode == OCCLUSIONMODE_NONE");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");

        miNextQueryIndex = 0;
        miOccludeeIndex = 0;
        meOcclusionMode = OCCLUSIONMODE_QUERY;

        if (mbImmediateMode)
        {
            if (mbOcclusionCheapShader)
                mpOccludeeIm3dZOnly->BeginRendering();
            else
                mpOccludeeIm3d->BeginRendering();
            return;
        }

        mpOccludeeIm3dZOnly->BeginRendering();
        shadow::Device::SetVertexDescriptor(
            reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor));
        shadow::Device::FlushVertexProgramState();
        D3DDevice_SetStreamSource(gpD3DDevice, 0u, mpVertexBuffer, 0u, 0u, 1u);
        D3DDevice_SetIndices(gpD3DDevice, mpIndexBuffer);
    }

    // CgsGraphics::OcclusionCullManager::EndQueries @ 0x827EDF50.
    void OcclusionCullManager::EndQueries()
    {
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_QUERY,
                   "meOcclusionMode == OCCLUSIONMODE_QUERY");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");

        meOcclusionMode = OCCLUSIONMODE_NONE;
        if (mbImmediateMode && !mbOcclusionCheapShader)
            mpOccludeeIm3d->EndRendering();
        else
            mpOccludeeIm3dZOnly->EndRendering();
        shadow::Device::ResetShadowing();
    }

    // X360 0x827E91A8. When conditional rendering is enabled, the render mode
    // brackets the subsequent mesh walk; both indices restart for each bracket.
    void OcclusionCullManager::BeginConditionalRender()
    {
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_NONE,
                   "meOcclusionMode == OCCLUSIONMODE_NONE");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");

        if (mbConditionalRendering)
            meOcclusionMode = OCCLUSIONMODE_RENDER;
        miNextQueryIndex = 0;
        miOccludeeIndex = 0;
    }

    // X360 0x827E9250. The console always resets the shadow cache after ending
    // the bracket, including when conditional rendering was disabled.
    void OcclusionCullManager::EndConditionalRender()
    {
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_RENDER || !mbConditionalRendering,
                   "meOcclusionMode == OCCLUSIONMODE_RENDER || !mbConditionalRendering");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");

        meOcclusionMode = OCCLUSIONMODE_NONE;
        shadow::Device::ResetShadowing();
    }

    void OcclusionCullManager::BeginMeshConditionalRender()
    {
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_RENDER, "meOcclusionMode == OCCLUSIONMODE_RENDER");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");
        CGS_ASSERT(!mbRenderingMesh, "!mbRenderingMesh");

        mbRenderingMesh = 1;
        CGS_ASSERT(miOccludeeIndex < KI_MAX_NUM_QUERIES, "miOccludeeIndex < KI_MAX_NUM_QUERIES");

        const s16 li16QueryId = static_cast<s16>(maQueryIds[miOccludeeIndex]);
        if (li16QueryId != -1)
        {
            D3DDevice_BeginConditionalRendering(gpXboxD3DDevice, static_cast<u32>(li16QueryId));
        }
    }

    void OcclusionCullManager::EndMeshConditionalRender()
    {
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_RENDER, "meOcclusionMode == OCCLUSIONMODE_RENDER");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");
        CGS_ASSERT(mbRenderingMesh, "mbRenderingMesh");

        mbRenderingMesh = 0;
        CGS_ASSERT(miOccludeeIndex < KI_MAX_NUM_QUERIES, "miOccludeeIndex < KI_MAX_NUM_QUERIES");

        if (maQueryIds[miOccludeeIndex] != 0xFFFF)
        {
            D3DDevice_EndConditionalRendering(gpXboxD3DDevice);
        }
        ++miOccludeeIndex;
    }

    void OcclusionCullManager::TrivialAcceptOccludeeBoundingBox()
    {
        CGS_ASSERT(miOccludeeIndex < KI_MAX_NUM_QUERIES, "miOccludeeIndex < KI_MAX_NUM_QUERIES");

        maQueryIds[miOccludeeIndex] = static_cast<u16>(0xFFFF);
        ++miOccludeeIndex;
    }

    namespace
    {
        // One output row of the row-vector product lrRow * lrMatrix, accumulated the way the
        // console's vmulfp / vmaddfp chain does: x term first, then y, z and w added on.
        rw::math::vpu::Vector4 TransformRow(const rw::math::vpu::Vector4& lrRow,
                                            const rw::math::vpu::Matrix44& lrMatrix)
        {
            rw::math::vpu::Vector4 lvResult = rw::math::vpu::Splat(lrRow.x) * lrMatrix.xAxis;
            lvResult = rw::math::vpu::Splat(lrRow.y) * lrMatrix.yAxis + lvResult;
            lvResult = rw::math::vpu::Splat(lrRow.z) * lrMatrix.zAxis + lvResult;
            lvResult = rw::math::vpu::Splat(lrRow.w) * lrMatrix.wAxis + lvResult;
            return lvResult;
        }
    }

    // Called by DrawRenderableMesh::InterpretOcclusionQuery with the mesh's world-view-
    // projection and its decoded box matrix (rows: the three scaled half-axes, then the
    // centre). The box goes to clip space as lOccludeeBoxMatrix * WVP. If the clip-space
    // centre, pulled back by mNearClipOffset, still lies in front of the near plane by more
    // than the box's projected half-extent (z + w measured per row), the box is drawn
    // inside a GPU survey and the survey id is recorded for the occludee; otherwise the box
    // crosses the near plane and the occludee is recorded as always-visible (0xFFFF).
    void OcclusionCullManager::RenderOccludeeBoundingBox(const rw::math::vpu::Matrix44* lpWorldViewProjection,
                                                         Matrix44 lOccludeeBoxMatrix)
    {
        CGS_ASSERT(lpWorldViewProjection, "lpWorldViewProjection");
        CGS_ASSERT(meOcclusionMode == OCCLUSIONMODE_QUERY, "meOcclusionMode == OCCLUSIONMODE_QUERY");
        CGS_ASSERT(mbOcclusionEnabled, "mbOcclusionEnabled");
        CGS_ASSERT(miOccludeeIndex < KI_MAX_NUM_QUERIES, "miOccludeeIndex < KI_MAX_NUM_QUERIES");
        CGS_ASSERT(miNextQueryIndex <= KI_MAX_QUERY_INDEX && miNextQueryIndex >= KI_BASE_QUERY_INDEX,
                   "miNextQueryIndex <= KI_MAX_QUERY_INDEX && miNextQueryIndex >= KI_BASE_QUERY_INDEX");

        rw::math::vpu::Matrix44 lBoxToClip;
        lBoxToClip.xAxis = TransformRow(lOccludeeBoxMatrix.xAxis, *lpWorldViewProjection);
        lBoxToClip.yAxis = TransformRow(lOccludeeBoxMatrix.yAxis, *lpWorldViewProjection);
        lBoxToClip.zAxis = TransformRow(lOccludeeBoxMatrix.zAxis, *lpWorldViewProjection);
        lBoxToClip.wAxis = TransformRow(lOccludeeBoxMatrix.wAxis, *lpWorldViewProjection);

        const rw::math::vpu::Vector4 lvCentre = lBoxToClip.wAxis - mNearClipOffset;
        const f32 lfCentreDepth = lvCentre.z + lvCentre.w;
        const f32 lfHalfExtent  = std::fabs(lBoxToClip.xAxis.z + lBoxToClip.xAxis.w)
                                + std::fabs(lBoxToClip.yAxis.z + lBoxToClip.yAxis.w)
                                + std::fabs(lBoxToClip.zAxis.z + lBoxToClip.zAxis.w);

        if (lfCentreDepth >= lfHalfExtent)
        {
            mpOccludeeIm3dZOnly->SetTransform(&lBoxToClip);

            maQueryIds[miOccludeeIndex] = static_cast<u16>(miNextQueryIndex);
            D3DDevice_BeginConditionalSurvey(gpXboxD3DDevice, static_cast<u32>(miNextQueryIndex), 1u);
            D3DDevice_DrawIndexedVertices(gpD3DDevice,
                                          mDrawParameters.mePrimitiveType,
                                          mDrawParameters.muBaseVertexIndex,
                                          mDrawParameters.muMinVertexIndex,
                                          mDrawParameters.muNumVertices);
            D3DDevice_EndConditionalSurvey(gpXboxD3DDevice, 0u);

            ++miNextQueryIndex;
            ++miOccludeeIndex;
        }
        else
        {
            maQueryIds[miOccludeeIndex] = static_cast<u16>(0xFFFF);
            ++miOccludeeIndex;
        }
    }
}
