#pragma once

// =============================================================================
// ShadowPassPCLeaf.h  (pc/gcm/renderengine)
//
// The declared surface of the PC leaves the SHADOW-MAP pass needs. It exists for
// one structural reason: BrnShadowMapRenderManager.cpp used to declare
//
//     namespace { struct ClearDepthStencilParameters { ... }; }
//     void DeviceClearDepthStencil(const ClearDepthStencilParameters*);
//
// -- an external-linkage function whose parameter type has INTERNAL linkage. MSVC
// mangles an anonymous namespace into a per-TU `?A0x<hash>@` component, so that
// symbol's decorated name is unique to that one object file and no other TU can
// ever define it. The declaration was unsatisfiable by construction, not merely
// unmounted. Hoisting the parameter type into a real header at namespace scope is
// the fix; the leaf that defines the function includes the same header.
//
// Two of the four symbols live in ImmediateModePCLeaf.cpp (the immediate-mode
// state appliers' home) and two in XenonD3D9Shims.cpp (the D3D9 device leaf);
// each definition site is named below.
// =============================================================================

#include "types.hpp"

namespace renderengine
{
    class DepthStencilState;   // renderstates.h (the real 0x60-byte state object)

    // The clear descriptor the X360 renderengine device clear (sub_82B61D78) consumes.
    // BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap builds one on its stack as
    // { 0x30, 1.0f, 0 } and hands it over.
    //
    // mu32Flags carries the XENON D3DCLEAR_* mask, whose bit assignment differs from PC
    // Direct3D 9's because the console has four colour targets: TARGET0..3 = 0x01/0x02/0x04/
    // 0x08, ZBUFFER = 0x10, STENCIL = 0x20. So the shadow pass's 0x30 is Z | STENCIL, and the
    // PC leaf translates the mask rather than forwarding it (see DeviceClearDepthStencil).
    struct ClearDepthStencilParameters
    {
        u32 mu32Flags;     // +0x00  Xenon D3DCLEAR_* mask
        f32 mfDepth;       // +0x04  the Z value to clear to
        u32 mu32Stencil;   // +0x08  the stencil value to clear to
    };

    // FLAG PC-platform leaf: the console's scene render target is (re)bound by
    // BrnRendererModule::BeginRenderAntiAliased @ Render:725, which runs AFTER the shadow and
    // env-map passes. This PC build has no BeginRenderAntiAliased -- renderengine::Device::
    // FrameBegin has already bound the D3D9 implicit back buffer by the time Render starts --
    // so the shadow pass, which binds the shadow-map surfaces underneath it, must put the back
    // buffer back itself or every later pass would draw into the shadow map. These two are that
    // bracket: save the bound colour + depth surface, viewport and scissor, and restore them.
    // DELETE when BeginRenderAntiAliased is reconstructed.
    // Defined in XenonD3D9Shims.cpp.
    void PCSurfaceBracket_Save();
    void PCSurfaceBracket_Restore();
}

// X360 dword_8301090C == CgsDepthStencilStateFactory::saDepthStencilStates[0]: the shared
// shadow-pass depth/stencil state (Z test on, Z func LESSEQUAL, Z write on).
// Defined in ImmediateModePCLeaf.cpp.
extern renderengine::DepthStencilState* gpShadowDepthStencilState;

// X360 sub_82276AD0 == CgsGraphics::ImRendererBase::SetState(const DepthStencilState*):
// install a depth/stencil state object on the device.
// Defined in ImmediateModePCLeaf.cpp -- see the overload note in that file's banner.
void ImDeviceSetDepthStencilState(renderengine::DepthStencilState* lpState);

// X360 sub_82B61D78: clear the bound depth/stencil surface.
// Defined in ImmediateModePCLeaf.cpp.
void DeviceClearDepthStencil(const renderengine::ClearDepthStencilParameters* lpParameters);
