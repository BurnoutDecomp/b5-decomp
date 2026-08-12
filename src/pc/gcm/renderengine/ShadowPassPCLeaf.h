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
    class RenderTargetState;   // PostFxRenderTargetPCLeaf.cpp (the bound-surface descriptor)

    // =========================================================================
    // X360 dword_83010A30 -- THE "last render-target state installed on the device" shadow.
    //
    // The console has exactly ONE of these, and both readers (CgsRenderTarget::
    // SetRenderTargetState* @0x827E7588/0x827E7668 and rw::graphics::postfx::RenderTarget::
    // Begin @0x823F9250) skip a redundant Device::SetState against it. This build had grown
    // TWO private copies -- one file-local in each of those TUs -- which is not just
    // duplication, it is a LIVE BUG on PC:
    //
    //   The shadow pass is bracketed by PCSurfaceBracket_Save/Restore, a PC-only stand-in for
    //   the console's BeginRenderAntiAliased. The console's rebind goes THROUGH Device::SetState
    //   and therefore keeps this shadow in step; the PC bracket restores the saved surfaces with
    //   raw IDirect3DDevice9::SetRenderTarget / SetDepthStencilSurface calls, BEHIND the shadow's
    //   back. So after the first shadow frame the shadow still reads "the shadow-map state is
    //   installed" while the device actually holds the back buffer -- and every LATER frame's
    //   SetRenderTargetState skips its bind and renders the cascades into the BACK BUFFER's
    //   colour and depth instead of into the shadow map.
    //
    // One definition (PostFxRenderTargetPCLeaf.cpp, beside the only Device::SetState that
    // writes it) and one invalidation point (PCSurfaceBracket_Restore) close that.
    // =========================================================================
    extern const RenderTargetState* gpLastRenderTargetState;

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

    // =========================================================================
    // THE SHADOW-MAP SAMPLE SEMANTICS SEAM (PC bring-up, 2026-08-12).
    //
    // The Xenos samples the shadow map through a hardware DEPTH-COMPARISON fetch: the
    // shipped pixel shaders do `texldp rN, rN, s15` and consume ONLY `.x`, as a 0..1
    // "lit" factor (`mul r0.w, r0.w, rN.x`), with rN.z/rN.w as the comparison
    // reference. All 92 s15 shaders in build/game/SHADERS.BNDL are written that way
    // and NONE of them does a manual compare, so the comparison MUST come from the
    // sampler, exactly as it does on the console.
    //
    // Direct3D 9 has no generic comparison-sampler state. What it has is two
    // vendor conventions, and they are NOT interchangeable:
    //   * a real depth-stencil format (D24X8 / D16) created as a TEXTURE -- the
    //     NVIDIA/Intel "hardware shadow map": the fetch compares and (with a LINEAR
    //     filter) 2x2-PCFs, returning 0..1. THIS is the Xenos semantic.
    //   * INTZ / DF24 / DF16 -- readable depth: the fetch returns the RAW stored
    //     depth. `lit *= rawDepth` is not a shadow test; on a cleared (1.0) map it is
    //     identically "fully lit", which is exactly the no-shadows symptom.
    // So the depth-target format is not a free choice: it decides whether the shipped
    // shaders mean anything. PostFxRenderTargetPCLeaf.cpp picks it and reports which
    // semantic it got through these two accessors; the sampler-state applier below
    // configures unit 15 to match.
    //
    // ShadowDepthFormat()                  the D3DFORMAT / FOURCC the shadow depth
    //                                      texture was created with (0 = none yet).
    // ShadowDepthFormatIsHardwareCompare() true when that format's fetch COMPARES.
    // Both defined in PostFxRenderTargetPCLeaf.cpp.
    u32  ShadowDepthFormat();
    bool ShadowDepthFormatIsHardwareCompare();

    // Install the sampler state unit luUnit needs for the format above. The console
    // does this through a renderengine::TextureState built in
    // BrnRendererModule::Construct @0x8240A778; this build has no TextureState objects
    // (they need the render-target pool), so shadow::Device::SetResource binds the
    // TEXTURE only and the unit keeps whatever the last user left -- for unit 15 that
    // is the D3D9 defaults (POINT/WRAP), which defeats the PCF the hardware compare
    // exists to give. DELETE when Construct's TextureState pair lands.
    // Defined in XenonD3D9Shims.cpp.
    void ShadowSampler_ApplyState(u32 luUnit);

    // The number of world DrawIndexedPrimitiveUP submissions issued so far. Read by the
    // shadow pass's [shadow-fetch] probe to separate "no draws reached the shadow target"
    // from "draws reached it and rasterised nothing".
    // Defined in XenonD3D9Shims.cpp.
    u64 WorldDrawCallCount();

    // [FLAG PC bring-up probe] the shadow-pass occlusion probe -- the ONLY way to establish
    // whether the shadow map is being written on this backend (a D3DPOOL_DEFAULT depth
    // texture cannot be locked or copied out, so the depth bytes are unreadable from the
    // CPU; the fragment count that survived the depth test is readable and answers the same
    // question). Bracket a cascade's draws with Begin/End; LastPixels returns the PREVIOUS
    // frame's count without stalling. ShadowProbe_TextureBound asks the runtime -- not the
    // engine's own shadow cache -- whether a texture is really bound at a sampler unit.
    // All four defined in XenonD3D9Shims.cpp. DELETE with the shadow bring-up.
    void ShadowProbe_Begin(u32 luCascade);
    void ShadowProbe_End(u32 luCascade);
    bool ShadowProbe_LastPixels(u32 luCascade, u32* lpuPixels);
    bool ShadowProbe_TextureBound(u32 luUnit);

    // [FLAG PC bring-up probe] the CLIP-SPACE TALLY, the follow-up to the occlusion probe.
    // The occlusion counts said "thousands of draws, no fragments"; this says WHERE the
    // geometry went. Every caster draw's first three referenced vertices are pushed through
    // the record's own baked WVP and bucketed into five mutually exclusive causes, and the
    // device state in force at the cascade's FIRST draw is captured alongside -- necessary
    // because the material walk rebinds viewport-affecting, cull and depth state per
    // technique, so what BeginRenderShadowMap set is not necessarily what the draws ran
    // under. Slot 3 is the WORLD-OPAQUE CONTROL: a probe that reports plausible numbers
    // there is a probe that works. DELETE with the shadow bring-up.
    struct ShadowClipReport
    {
        u32 muSampled, muInside, muOutXY, muOutZNear, muOutZFar, muBehindW, muNoWvp;
        f32 mafFirstObject[3];   // the first sampled vertex, object space -- the decode check
        f32 mafFirstClip[4];     // ...and what the record's WVP made of it
        u32 muVpX, muVpY, muVpW, muVpH;
        f32 mfVpMinZ, mfVpMaxZ;
        s32 miScissorL, miScissorT, miScissorR, miScissorB;
        u32 muScissorEnable, muZEnable, muZWrite, muZFunc, muCull, muColourWrite;
        u32 muZFuncEffective, muCullEffective;   // re-read after any env override, at the draw
        f32 mfHalfWidthMetres, mfHalfHeightMetres, mfDepthSpanMetres;  // the FITTED extent
        f32 mafClipMin[3], mafClipMax[3];        // the caster set's clip-space AABB
        u32 muTrisSampled, muTrisSubPixel;       // the sub-pixel census
        f32 mfMaxTriPixelArea;
    };
    bool ShadowProbe_ClipTally(u32 luSlot, ShadowClipReport* lpReport);

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
