#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsXboxConditionalRenderShims.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsOcclusionCullManager.cpp - the hardware-occlusion-query manager's per-mesh
// conditional-render bracket and the trivial-accept path.
//
//   BeginMeshConditionalRender         @ 0x827E8F18
//   EndMeshConditionalRender           @ 0x827E9020
//   TrivialAcceptOccludeeBoundingBox   @ 0x827E9130
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
}
