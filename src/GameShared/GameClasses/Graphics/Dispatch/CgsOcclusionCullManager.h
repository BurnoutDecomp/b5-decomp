#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                    // Matrix44 / Vector4
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"            // ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h" // Im3dUntexVertex
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsPositionOnlyVertex.h" // PositionOnlyVertex
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"                // DrawIndexedParameters

namespace rw { struct IResourceAllocator; }
namespace renderengine { class IndexBuffer; class VertexBuffer; }

// =============================================================================
// CgsOcclusionCullManager.h  (GameShared/GameClasses/Graphics/Dispatch)
//
// Hardware-occlusion-query manager used by the render dispatch path. For each
// drawn mesh the dispatcher issues a GPU occlusion query (an occludee), records
// the query id in a fixed-capacity table, and later replays the query as a
// predicated/conditional draw so the GPU can skip fully-occluded geometry.
// On the X360 the predication is driven by the Xenon D3D extensions
// D3DDevice_Begin/EndConditionalRendering keyed off the recorded query id.
//
// Layout is reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly
// (BeginMeshConditionalRender @ 0x827E8F18, EndMeshConditionalRender
// @ 0x827E9020, TrivialAcceptOccludeeBoundingBox @ 0x827E9130; authoritative
// for the member offsets) and the assert strings baked into those bodies
// (CgsOcclusionCullManager.h: meOcclusionMode / mbOcclusionEnabled /
// mbRenderingMesh / miOccludeeIndex / KI_MAX_NUM_QUERIES).
//
// Console offsets from the DecFIGS member names and ARTIST stores:
//   0x000  mViewProjectionMatrix  the camera's view-projection (InterpretOcclusionQuery)
//   0x0C0  mNearClipOffset   Vector4 subtracted from the clip-space box centre
//   0x0D0  maCubeVertices[8]  Im3dUntexVertex, 0x20-byte stride
//   0x1D0  maCubeVerticesZOnly[8] PositionOnlyVertex, 0x10-byte stride
//   0x250  mau16CubeIndices[24]
//   0x280  miOccludeeIndex   s32     current occludee slot (result[160])
//   0x284  miNextQueryIndex  s32     next GPU survey id
//   0x28C  mpOccludeeIm3dZOnly       the position-only renderer the box is drawn with
//   0x290  meOcclusionMode   s32     OCCLUSIONMODE_QUERY / OCCLUSIONMODE_RENDER
//   0x2A0  mDrawParameters           the box's indexed draw
//   0x2B0  maQueryIds[64]    u16     per-occludee GPU query id (0xFFFF == none),
//                                    indexed `this + 2*(miOccludeeIndex+344)`
//   0x330  mbRenderingMesh   u8      set across a Begin/End mesh pair
//   0x331  mbOcclusionEnabled u8     occlusion feature enabled
// =============================================================================

namespace CgsGraphics
{
    struct Im3dUntex;
    struct Im3dZOnly;

    struct OcclusionQueryList
    {
        Matrix44* mpBoxToClipMatrices;
        u32* mpuTrivialAcceptBits;
        u32 muSize;
        u32 muNumQueries;
    };

    // KI_MAX_NUM_QUERIES: maQueryIds capacity (the asm bounds-checks miOccludeeIndex
    // against 64 before every query-table access).
    const s32 KI_MAX_NUM_QUERIES = 64;

    struct OcclusionCullManager
    {
        // meOcclusionMode states. QUERY (1) is what RenderOccludeeBoundingBox asserts and
        // RENDER (2) what the mesh conditional-render bracket asserts.
        enum OcclusionMode
        {
            OCCLUSIONMODE_NONE              = 0,
            OCCLUSIONMODE_QUERY             = 1,
            OCCLUSIONMODE_RENDER            = 2,
            OCCLUSIONMODE_GENERATEQUERYLIST = 3
        };

        // The GPU survey-id window RenderOccludeeBoundingBox issues queries from (its assert
        // bounds miNextQueryIndex to [0, 63] on this build).
        static const s32 KI_BASE_QUERY_INDEX = 0;
        static const s32 KI_MAX_QUERY_INDEX  = 63;

        // X360 0x827F1E00. Store the passed renderers and allocator-backed cube/query resources;
        // the query-list capacity is caller-provided and controls both optional array sizes.
        void Construct(Im3dUntex* lpOccludeeIm3d,
                       Im3dZOnly* lpOccludeeIm3dZOnly,
                       rw::IResourceAllocator* lpAllocator,
                       u32 luQueryListSize);
        void BeginQueries();
        void EndQueries();

        // X360 0x827E8F18: assert RENDER mode + enabled + not already rendering a
        // mesh, mark rendering, then begin a predicated draw on the current
        // occludee's query id (skip if the id is the 0xFFFF "trivially accepted"
        // sentinel).
        void BeginMeshConditionalRender();

        // X360 0x827E9020: assert RENDER mode + enabled + rendering, clear the
        // rendering flag, end the predicated draw for the current occludee, then
        // advance to the next occludee slot.
        void EndMeshConditionalRender();

        // X360 0x827E9130: mark the current occludee as trivially accepted
        // (query id = 0xFFFF, i.e. always-draw) and advance to the next slot.
        void TrivialAcceptOccludeeBoundingBox();

        // X360 0x827E91A8 / 0x827E9250: select the conditional-render mode and
        // reset the shadow cache when leaving it.
        void BeginConditionalRender();
        void EndConditionalRender();

        // Transform the occludee's box into clip space; unless it reaches the near-clip offset,
        // issue a GPU survey that draws the box with the position-only renderer and record
        // the survey id for the occludee. A box crossing the near plane is accepted as visible.
        void RenderOccludeeBoundingBox(const rw::math::vpu::Matrix44* lpWorldViewProjection,
                                       Matrix44 lOccludeeBoxMatrix);

        // Console offsets in the comments; members after the first pointer move on x64.
        Matrix44              mViewProjectionMatrix;     // +0x000
        Matrix44              mAlwaysFailMatrix;         // +0x040
        Matrix44              mAlwaysPassMatrix;         // +0x080
        Vector4               mNearClipOffset;           // +0x0C0
        Im3dUntexVertex       maCubeVertices[8];          // +0x0D0, 0x20-byte stride
        PositionOnlyVertex    maCubeVerticesZOnly[8];     // +0x1D0, 0x10-byte stride
        u16                   mau16CubeIndices[24];       // +0x250
        s32                   miOccludeeIndex;           // +0x280
        s32                   miNextQueryIndex;          // +0x284
        Im3dUntex*            mpOccludeeIm3d;             // +0x288
        // Im3dZOnly's proven Im3dBase<PositionOnlyVertex> base supplies the query draw's SetTransform.
        ImRenderer<PositionOnlyVertex>* mpOccludeeIm3dZOnly; // +0x28C
        s32                   meOcclusionMode;           // +0x290
        renderengine::VertexBuffer* mpVertexBuffer;       // +0x294
        renderengine::VertexDescriptor* mpVertexDescriptor; // +0x298
        renderengine::IndexBuffer* mpIndexBuffer;         // +0x29C
        DrawIndexedParameters mDrawParameters;           // +0x2A0
        u16 maQueryIds[KI_MAX_NUM_QUERIES];     // +0x2B0 (128 bytes -> ends 0x330)
        // ARTIST Construct initializes these state bytes before creating the vertex/index buffers
        // and the caller-sized query list.
        bool mbRenderingMesh = false;            // +0x330 (Construct @0x827F1E00)
        bool mbOcclusionEnabled = false;         // +0x331
        bool mbShowOccludeeBBs = false;           // +0x332
        bool mbOccludeesDoubleSided = false;      // +0x333
        bool mbOcclusionCheapShader = true;       // +0x334
        bool mbDisplayStats = false;              // +0x335
        bool mbDetectNearClipIntersections = true;// +0x336
        bool mbImmediateMode = false;             // +0x337
        bool mbConditionalRendering = true;       // +0x338
        OcclusionQueryList* mpQueryList;           // +0x33C (Construct @0x827F1E00)
    };

    static_assert(sizeof(Im3dUntexVertex) == 0x20,
                  "ARTIST Construct advances Im3dUntexVertex by 0x20 bytes");
    static_assert(sizeof(PositionOnlyVertex) == 0x10,
                  "ARTIST Construct advances PositionOnlyVertex by 0x10 bytes");
}
