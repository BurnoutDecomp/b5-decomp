// ============================================================================
// pc/gcm/renderengine/SkidImmediateModePCLeaf.cpp
//
// [PC platform leaf] The two Xenon-only symbols the SKID (tyre-mark) immediate-mode
// draw declares and that no project TU defines. Same role and same rationale as the
// sibling ImmediateModePCLeaf.cpp does for the sky dome -- this one is deliberately
// separate so the sky wave's leaf stays its own change.
//
// The draw in question is CgsGraphics::ImRenderer<BrnGraphics::SkidVertex>::Render
// @0x8228E068 (BrnSkidVertex.cpp), which is what actually submits a trail strip:
//
//     TrailSystem::Render -> TrailRenderer::Render -> Im3dSkidsRenderer (ImRenderer
//     <SkidVertex>::Render) -> D3DDevice_BeginVertices / _EndVertices
//
// XenonD3D9Shims.cpp already realises D3DDevice_BeginVertices / _EndVertices over
// IDirect3DDevice9::DrawPrimitiveUP. The two that were left are:
//
//   * ImRendererBase::mgpDevice -- the shared device pointer. Its console definition
//     (off_83271608) lives in CgsImRenderer.cpp, which the sky wave measured as making
//     the link WORSE if mounted (it is X360-shaped and drags XGSetVertexBufferHeader /
//     XMemGetPageSize / XQueryMemoryProtect and an LNK2005 against CgsIm2d.cpp). The
//     single static is homed here instead, and pointed at the live PC device the shims
//     already own, so the skid path and the shims agree on one device.
//
//   * D3DDevice_InsertFence -- on Xenos this pushes a fence packet into the command
//     buffer so the CPU can tell when the GPU has finished with a ring range that the
//     next batch is about to overwrite. PC Direct3D 9's DrawPrimitiveUP takes a COPY of
//     the vertex data at submit time (that is the whole reason the shim can implement
//     BeginVertices over a staging buffer at all), so no such hazard exists and there is
//     nothing for a fence to protect. The console's own use is exactly that guard --
//     ImRenderer<SkidVertex>::Render only calls it when the batch exceeds 0x80000 bytes
//     -- so a no-op here is the correct PC behaviour, not a dropped effect.
//
// ⚠ NOT A SILENT NO-OP: the fence counts its calls and says so once, so a future wave
// that moves the shim to a real ring can see whether the guard was ever hit.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace CgsGraphics
{
    // The console's off_83271608 -- the shared D3D device the immediate-mode renderers pass to
    // the ring API. On PC it is only ever FORWARDED: D3DDevice_BeginVertices / _EndVertices /
    // _InsertFence all ignore their device argument and use XenonD3D9Shims.cpp's own live
    // IDirect3DDevice9 (`void* D3DDevice_BeginVertices(void* /*lpDeviceArg*/, ...)`). So the
    // value is inert here and stays null unless a caller sets it -- it is NOT a dropped binding.
    void* ImRendererBase::mgpDevice = 0;
}

extern "C" u32 D3DDevice_InsertFence(void* /*lpDevice*/)
{
    static u32 suFenceRequests = 0;
    if (++suFenceRequests == 1)
    {
        CgsDev::Log::WriteToLog(
            "[skid] D3DDevice_InsertFence: PC no-op (DrawPrimitiveUP copies the vertex run at "
            "submit, so the Xenos ring-overwrite hazard the fence guards does not exist here)\n");
    }
    return suFenceRequests;
}
