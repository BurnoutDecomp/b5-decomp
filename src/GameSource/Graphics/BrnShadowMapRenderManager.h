// b5-decomp/src/GameSource/Graphics/BrnShadowMapRenderManager.h
#pragma once

#include "types.hpp"

// BrnGraphics::ShadowMapRenderManager -- drives the per-frame shadow-map render passes. It owns the
// double-buffered shadow-cache buffer indices (write / read) and a front-face-cull override flag, and
// wraps the renderer's shadow-map render targets (owned by BrnRendererMemory) with the Begin/End pass
// bracketing that binds each face's viewport + scissor and resolves the depth surface afterwards.
//
// Shape from the DecFIGS DWARF (GameSource/Graphics/BrnShadowMapRenderManager.h). Only the two
// X360-ARTIST-attested pass-bracket methods (BeginRenderShadowMap @0x823F7858 /
// EndRenderShadowMap @0x823FD708, both called by BrnRendererModule::Render) are given bodies in the
// .cpp; the remaining DWARF-listed methods are declared (ledger-gated) so the class shape matches, with
// bodies belonging to their own TUs. The class is embedded by value in BrnRendererModule.

struct BrnRendererMemory;   // BrnRendererMemory.h declares it `struct` (C4099 if mismatched)
class CgsRenderTarget;

namespace CgsGraphics
{
    class BufferedDispatchFrame;
    class DispatchPacketInterpreter;
    class Im2d;
}
class DispatchMeshContext;

namespace BrnGraphics
{
    // BrnShadowMapRenderManager.cpp:98 (DWARF) -- namespace-scope selector: when set, all shadow faces
    // share render-target slot 0 (packed / multi-section combined viewport); when clear, each face has
    // its own shadow render target. Read by Begin/EndRenderShadowMap (X360 byte_82F2423F).
    extern bool gbCombinedShadowMapViewport;

    struct ShadowMapRenderManager
    {
        // --- attested pass-bracket surface (bodies in BrnShadowMapRenderManager.cpp) -----------------

        // 0x823F7858 -- bind the shadow render target for face liIndex's draw pass (viewport + scissor),
        // install the shared shadow-pass depth/stencil state, and (when lbClear) clear the depth/stencil.
        void BeginRenderShadowMap(s32 liIndex, bool lbClear, BrnRendererMemory* lpAllocatedRenderTargets);

        // 0x823FD708 -- resolve face liIndex's shadow depth surface out to its sampleable texture.
        void EndRenderShadowMap(s32 liIndex, BrnRendererMemory* lpAllocatedRenderTargets);

        // --- remaining DWARF-listed surface (ledger-gated declarations; bodies in their own TUs) ------
        void Construct(u32 luBaseRenderTargetIndex);
        void Destruct();
        u32  GetWriteBufferIndex() const;
        u32  GetReadBufferIndex() const;
        void SwapCacheBuffers();
        void RenderAllShadowBuffers(CgsGraphics::BufferedDispatchFrame* lpDispatchFrame,
                                    CgsGraphics::DispatchPacketInterpreter* lpDispatchInterpreter,
                                    DispatchMeshContext* lpDispatchMeshContext,
                                    BrnRendererMemory* lpAllocatedRenderTargets,
                                    u32 lxRendererFlags,
                                    CgsGraphics::Im2d* lp2dRenderer);
        void BeginFrontFaceCullRender();
        void EndFrontFaceCullRender();
        void BeginBackFaceCullRender();
        void EndBackFaceCullRender();

    protected:
        // DWARF member set + order (BrnShadowMapRenderManager.h:112-114).
        u32  muWriteBufferIndex;    // +0x00
        u32  muReadBufferIndex;     // +0x04
        bool mbForceFrontFaceCull;  // +0x08
    };
}
