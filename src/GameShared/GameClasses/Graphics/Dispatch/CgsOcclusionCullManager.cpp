#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsXboxConditionalRenderShims.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector4_operation.h"            // Splat / lane-wise * and +

#include <cmath>                                     // std::fabs (the sign-mask clear)

struct IDirect3DDevice9;
extern IDirect3DDevice9* gpD3DDevice;

// The Xenon indexed draw (pc/gcm/renderengine/XenonD3D9Shims.cpp), the call the console makes.
extern "C" void D3DDevice_DrawIndexedVertices(IDirect3DDevice9* lpDevice,
                                               u32 lePrimitiveType,
                                               u32 luBaseVertexIndex,
                                               u32 luMinVertexIndex,
                                               u32 luNumVertices);

// CgsOcclusionCullManager.cpp - the hardware-occlusion-query manager's per-mesh
// conditional-render bracket and the trivial-accept path.
//
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
