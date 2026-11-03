#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/indexbuffer.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"            // Splat / lane-wise * and +
#include "rw/rwcore_structs.h"                        // IResourceAllocator / Resource

#include <cmath>                                     // std::fabs (the sign-mask clear)
#include <cstring>                                   // std::memcpy
#include <cstdlib>
#include <cstdio>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

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
    CGS_STATIC_ASSERT((sizeof(Matrix44) == 0x40),
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
            Vector3(lfNegativeExtent, lfPositiveExtent, lfNegativeExtent),
            Vector3(lfPositiveExtent, lfPositiveExtent, lfNegativeExtent),
            Vector3(lfPositiveExtent, lfNegativeExtent, lfNegativeExtent),
            Vector3(lfNegativeExtent, lfNegativeExtent, lfNegativeExtent),
            Vector3(lfNegativeExtent, lfNegativeExtent, lfPositiveExtent),
            Vector3(lfPositiveExtent, lfNegativeExtent, lfPositiveExtent),
            Vector3(lfPositiveExtent, lfPositiveExtent, lfPositiveExtent),
            Vector3(lfNegativeExtent, lfPositiveExtent, lfPositiveExtent)
        };
        for (u32 luVertex = 0u; luVertex < 8u; ++luVertex)
        {
            maCubeVertices[luVertex].mv3Pos = laCubeCorners[luVertex];
            RGBA8& lrColour = maCubeVertices[luVertex].mv4Colour;
            lrColour.r = 0xFFu; lrColour.g = static_cast<u8>(luVertex * 8u);
            lrColour.b = 0u; lrColour.a = 0xFFu;
            maCubeVerticesZOnly[luVertex].mv3Pos = maCubeVertices[luVertex].mv3Pos;
        }

        mAlwaysFailMatrix.xAxis.Set(-10.0f, 0.0f, 0.0f, 0.0f);
        mAlwaysFailMatrix.yAxis.Set(0.0f, 0.05f, 0.0f, 0.0f);
        mAlwaysFailMatrix.zAxis.Set(0.0f, 0.0f, 0.05f, 0.0f);
        mAlwaysFailMatrix.wAxis.Set(0.0f, 0.0f, 0.0f, 1.0f);
        mAlwaysPassMatrix.xAxis.Set(0.02f, 0.0f, 0.0f, 0.0f);
        mAlwaysPassMatrix.yAxis.Set(0.0f, 0.02f, 0.0f, 0.0f);
        mAlwaysPassMatrix.zAxis.Set(0.0f, 0.0f, 0.02f, 0.0f);
        mAlwaysPassMatrix.wAxis.Set(0.0f, 0.0f, 0.0f, 1.0f);

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
        const u8 luPositionStride = lpDescriptorData->GetElementStrides()[0];

        renderengine::VertexBuffer::Parameters lVertexBufferParameters;
        lVertexBufferParameters.type = 0u;
        lVertexBufferParameters.bufferSize = 8u * static_cast<u32>(sizeof(PositionOnlyVertex));
        rw::ResourceDescriptor lVertexBufferDescriptor;
        lVertexBufferDescriptor = renderengine::VertexBuffer::GetResourceDescriptor(lVertexBufferParameters);
        rw::Resource lVertexBufferResource = lpAllocator->DoAllocate(lVertexBufferDescriptor, 0);
        CGS_ASSERT(lVertexBufferResource.m_baseResources[0],
                   "couldn't allocate memory for vertex descriptor");

        mpVertexBuffer = renderengine::VertexBuffer::Initialize(lVertexBufferResource, lVertexBufferParameters);
        CGS_ASSERT(mpVertexBuffer, "couldn't create a mpVertexBuffer");
        mpVertexBuffer->Xbox2UnSet();
        u8* const lpVertexBufferBytes = static_cast<u8*>(D3DVertexBuffer_Lock(mpVertexBuffer, 0, 0, 0));
        for (u32 luVertex = 0u; luVertex < 8u; ++luVertex)
        {
            std::memcpy(lpVertexBufferBytes + luPositionOffset + luVertex * luPositionStride,
                        &maCubeVerticesZOnly[luVertex], sizeof(PositionOnlyVertex));
        }
        D3DVertexBuffer_Unlock(mpVertexBuffer);

        renderengine::IndexBuffer::Parameters lIndexBufferParameters;
        lIndexBufferParameters.type = 0u;
        lIndexBufferParameters.depth = 16u;
        lIndexBufferParameters.numIndices = 24u;
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
        rw::Resource lIndexBufferResource = lpAllocator->DoAllocate(lIndexBufferDescriptor, 0);
        CGS_ASSERT(lIndexBufferResource.m_baseResources[0],
                   "couldn't allocate memory for index buffer");

        mpIndexBuffer = renderengine::IndexBuffer::Initialize(lIndexBufferResource, lIndexBufferParameters);
        CGS_ASSERT(mpIndexBuffer, "couldn't create mpIndexBuffer");
        renderengine::IndexBuffer::Locked lIndexBufferLockInfo;
        const bool lbIndexBufferLocked = mpIndexBuffer->Lock(2, lIndexBufferLockInfo);
        CGS_ASSERT(lbIndexBufferLocked, "couldn't lock mpIndexBuffer");
        std::memcpy(lIndexBufferLockInfo.indices, mau16CubeIndices, sizeof(mau16CubeIndices));
        mpIndexBuffer->Unlock(lIndexBufferLockInfo);

        mDrawParameters.SetPrimitiveType(static_cast<renderengine::PrimitiveType>(13));
        mDrawParameters.SetBaseVertexIndex(0u);
        mDrawParameters.SetStartIndex(0u);
        mDrawParameters.SetIndexCount(24u);

        // ARTIST requests a 0x10-byte console record; the two pointer fields are 64-bit in this
        // PC record, so size this passed-allocator request from the actual host type.
        mpQueryList = static_cast<OcclusionQueryList*>(
            AllocateMemoryResource(lpAllocator, static_cast<u32>(sizeof(OcclusionQueryList)), 0x80u));
        mpQueryList->mpBoxToClipMatrices = 0;
        mpQueryList->mpuTrivialAcceptBits = 0;
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
            ::D3DDevice_BeginConditionalRendering(gpD3DDevice, static_cast<u32>(li16QueryId));
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
            ::D3DDevice_EndConditionalRendering(gpD3DDevice);
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
            __vector4 lvResult = __vmulfp(__vspltw(lrRow.mV, 0), lrMatrix.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lrRow.mV, 1), lrMatrix.yAxis.mV, lvResult);
            lvResult = __vmaddfp(__vspltw(lrRow.mV, 2), lrMatrix.zAxis.mV, lvResult);
            lvResult = __vmaddfp(__vspltw(lrRow.mV, 3), lrMatrix.wAxis.mV, lvResult);
            return rw::math::vpu::Vector4(lvResult);
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
        const f32 lfCentreDepth = static_cast<f32>(lvCentre.GetZ()) + static_cast<f32>(lvCentre.GetW());
        const f32 lfHalfExtent  = std::fabs(static_cast<f32>(lBoxToClip.xAxis.GetZ()) + static_cast<f32>(lBoxToClip.xAxis.GetW()))
                                + std::fabs(static_cast<f32>(lBoxToClip.yAxis.GetZ()) + static_cast<f32>(lBoxToClip.yAxis.GetW()))
                                + std::fabs(static_cast<f32>(lBoxToClip.zAxis.GetZ()) + static_cast<f32>(lBoxToClip.zAxis.GetW()));

        if (lfCentreDepth >= lfHalfExtent)
        {
            static_cast<Im3dZOnly*>(mpOccludeeIm3dZOnly)->SetTransform(lBoxToClip);

            maQueryIds[miOccludeeIndex] = static_cast<u16>(miNextQueryIndex);
            ::D3DDevice_BeginConditionalSurvey(gpD3DDevice, static_cast<u32>(miNextQueryIndex), 1u);
            D3DDevice_DrawIndexedVertices(gpD3DDevice,
                                          static_cast<D3DPRIMITIVETYPE>(mDrawParameters.GetPrimitiveType()),
                                          mDrawParameters.GetBaseVertexIndex(),
                                          mDrawParameters.GetStartIndex(),
                                          mDrawParameters.GetIndexCount());
            ::D3DDevice_EndConditionalSurvey(gpD3DDevice, 0u);

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
