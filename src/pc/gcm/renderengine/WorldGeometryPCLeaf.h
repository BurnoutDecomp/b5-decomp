#pragma once

#include "types.hpp"

#include <cstddef>   // size_t (the eviction hook's byte count)

// =============================================================================
// WorldGeometryPCLeaf.h -- the RETAINED world-geometry surface (PC leaf).
//
// FLAG PC-platform leaf: THE CONSOLE HAS NO EQUIVALENT OF THIS FILE.
// On the Xbox 360 the serialised IndexBuffer/VertexBuffer headers a bundle carries
// already ARE GPU-visible memory: D3DDevice_SetIndices / _SetStreamSource hand the
// Xenos the bundle's own base address and the draw fetches straight out of it. PC
// Direct3D 9 cannot bind host memory -- geometry has to live in a device object --
// so the world's static bundle buffers are mirrored ONCE into D3D9 buffers here and
// drawn with SetStreamSource/SetIndices/DrawIndexedPrimitive.
//
// Two console-only vertex-fetch features are BAKED into those mirrors at creation,
// because D3D9 has no runtime equivalent for either:
//   * D3DDECLTYPE_DEC3N (packed 10:10:10 normals) -- most current NVIDIA drivers do
//     not expose D3DDTCAPS_DEC3N, so the packed lanes are expanded to FLOAT3;
//   * the Xenos PRIMITIVE RESET (PA_SU_SC_MODE_CNTL MULTI_PRIM_IB_ENA) -- D3D9 has
//     no primitive restart, so strip runs are re-cut into an equivalent triangle list.
// Both were previously redone PER DRAW CALL inside WorldDraw_IndexedUP against a
// DrawIndexedPrimitiveUP submit; measured at ~36% of the main thread's self time on
// 2026-08-15 (~3,800 draws a frame, each re-expanding a whole static vertex buffer).
// The transforms are unchanged -- the same bytes, the same triangles, the same
// winding -- they simply happen once per buffer instead of once per draw.
//
// Contents are keyed on the bundle buffer's header pointer PLUS the plan applied to
// it (a vertex buffer can be bound under two techniques whose descriptors differ,
// e.g. the z-only pass, and their expanded strides differ), so a mirror is never
// reused under the wrong plan.
//
// LIFETIME: the mirrors alias bundle memory that the streamer recycles when a track
// unit is swapped, so CgsResource::Pool::FreeMemoryForResource notifies this leaf
// (WorldGeometry_OnResourceMemoryFreed) before each heap block goes back, and every
// mirror whose header or source bytes lie in that block is released.
// =============================================================================

namespace renderengine
{
    // One bundle vertex buffer plus the expansion plan the bound declaration implies.
    struct WorldGeometryVertexPlan
    {
        const void* mpHeader;              // the serialised VertexBufferHeader32
        const void* mpData;                // its resolved host bytes
        u32         muNumVertices;         // muSize / muSourceStride
        u32         muSourceStride;        // the bundle's own vertex stride
        u32         muExpandedStride;      // source + 8 per DEC3N element
        u32         muDec3nCount;
        u16         mau16Dec3nOffsets[16]; // ascending source byte offsets
    };

    // One index RUN out of a bundle index buffer, plus the primitive state it draws under.
    struct WorldGeometryIndexPlan
    {
        const void* mpHeader;               // the serialised IndexBufferHeader32
        const void* mpRun;                  // its resolved host bytes + muStartIndex indices
        u32         muStartIndex;
        u32         muIndexCount;
        u32         muResetIndex;
        s32         miMappedPrimitiveType;  // D3DPRIMITIVETYPE, as MapPrimitive returned it
        u32         muMappedPrimitiveCount; // ...and its primitive count
        bool        mb32Bit;                // index width, from the header Common tag
        bool        mbResetEnabled;         // the technique's rasteriser-state reset flag
    };

    // What a prepared draw needs at submit time (and what the caller's probes read).
    struct WorldGeometryDraw
    {
        void* mpVertexBuffer;          // IDirect3DVertexBuffer9*
        void* mpIndexBuffer;           // IDirect3DIndexBuffer9*
        u32   muExpandedStride;
        u32   muNumVertices;
        s32   miPrimitiveType;         // D3DPRIMITIVETYPE, after any strip re-cut
        u32   muPrimitiveCount;
        u32   mau32FirstIndices[3];    // the first three indices of the SUBMITTED run
        u32   muFirstIndexCount;
    };

    enum EWorldGeometryPrepare
    {
        E_WORLDGEOMETRY_READY       = 0,  // mirrors are live, submit the draw
        E_WORLDGEOMETRY_SKIP        = 1,  // the run carries nothing drawable; draw nothing
        E_WORLDGEOMETRY_UNAVAILABLE = 2,  // no mirror could be made; caller keeps its own path
    };

    // Fetch (creating on first use) the mirrors for one draw.
    //
    // Called ~3,800 times a frame, so a repeat draw of an already-mirrored mesh answers out
    // of a direct-mapped front cache and never touches the retained maps at all; the plans
    // are still validated in full, so the fast path can only return the same mirrors the
    // lookup would have. See the front-cache section in WorldGeometryPCLeaf.cpp.
    EWorldGeometryPrepare WorldGeometry_Prepare(const WorldGeometryVertexPlan& lrVertexPlan,
                                                const WorldGeometryIndexPlan& lrIndexPlan,
                                                WorldGeometryDraw* lpOutDraw);

    // Bind the mirrors and submit. Returns the D3D9 HRESULT of the draw.
    s32 WorldGeometry_Submit(const WorldGeometryDraw& lrDraw, u32 luBaseVertexIndex);

    // Release every mirror whose header pointer or source bytes lie inside the block
    // [lpBase, lpBase + luSize). Called from the resource pool's free path.
    //
    // ⚠ INVARIANTS the two callers rely on, stated so the next streaming wave sees them:
    //  * SINGLE-THREADED. This mutates the same maps WorldGeometry_Prepare inserts into,
    //    with no lock. Today the only free path (BundleLoader::UnloadBundle ->
    //    Pool::RemoveReference -> Pool::FreeMemoryForResource) runs synchronously on the
    //    render thread. If bundle unloading ever moves to the streamer's own thread, this
    //    call must be deferred to the render thread (a queue drained before the frame's
    //    first dispatch) -- NOT locked, since Prepare is on the draw path.
    //  * NO RELOCATION WITHOUT A FREE. The pool's defragmenter (Pool::meDefragStage, still
    //    deferred on PC) would move a live resource WITHOUT passing through
    //    FreeMemoryForResource; if it is ever bodied it must notify this leaf for the old
    //    range (or the mirrors keyed on the old header pointer go stale).
    void WorldGeometry_OnResourceMemoryFreed(const void* lpBase, size_t luSize);

    // Release everything. NO CALLER TODAY: this build never tears the D3D device down
    // (there is no gDevice->Release() anywhere -- process exit reclaims it), so nothing
    // needs to release the mirrors first. Kept as the one correct place to do so the day a
    // device teardown / reset path exists; wire it there, next to the other renderengine
    // shutdown leaves.
    void WorldGeometry_ReleaseAll();
}
