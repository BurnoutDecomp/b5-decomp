#pragma once

#include "types.hpp"

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
// X360-verified offsets used by this TU (only the touched members are named;
// the rest of the object is opaque and modelled as explicit padding):
//   0x280  miOccludeeIndex   s32     current occludee slot (result[160])
//   0x290  meOcclusionMode   s32     OCCLUSIONMODE_RENDER == 2 when drawing
//   0x2B0  maQueryIds[64]    u16     per-occludee GPU query id (0xFFFF == none),
//                                    indexed `this + 2*(miOccludeeIndex+344)`
//   0x330  mbRenderingMesh   u8      set across a Begin/End mesh pair
//   0x331  mbOcclusionEnabled u8     occlusion feature enabled
// =============================================================================

namespace CgsGraphics
{
    // meOcclusionMode states. The asm only proves OCCLUSIONMODE_RENDER == 2 (the
    // value Begin/End/Trivial assert against); the lower values are the standard
    // off/build phases of an occlusion pass.
    enum E_OcclusionMode
    {
        OCCLUSIONMODE_OFF    = 0,
        OCCLUSIONMODE_BUILD  = 1,
        OCCLUSIONMODE_RENDER = 2
    };

    // KI_MAX_NUM_QUERIES: maQueryIds capacity (the asm bounds-checks miOccludeeIndex
    // against 64 before every query-table access).
    const s32 KI_MAX_NUM_QUERIES = 64;

    struct OcclusionCullManager
    {
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

        u8  mPad0[0x280];                       // +0x000 opaque prefix
        s32 miOccludeeIndex;                    // +0x280
        u8  mPad1[0x290 - 0x284];               // +0x284 opaque
        s32 meOcclusionMode;                    // +0x290
        u8  mPad2[0x2B0 - 0x294];               // +0x294 opaque
        u16 maQueryIds[KI_MAX_NUM_QUERIES];     // +0x2B0 (128 bytes -> ends 0x330)
        u8  mbRenderingMesh;                    // +0x330
        u8  mbOcclusionEnabled;                 // +0x331
    };
}
