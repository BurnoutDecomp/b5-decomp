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

    // FLAG PC-platform leaf: the POST-FX RAW-DEPTH sampler seam -- the sibling of
    // ShadowSampler_ApplyState above, for the other depth semantic.
    //
    // A post-fx target's depth is created in a RAW-VALUE format (INTZ / DF24 / DF16) so the
    // composite's DoF / motion-blur permutations and rwgpfxdof can read depth VALUES; those
    // fetches must be POINT (filtering depths averages two surfaces into one that exists at
    // neither) and CLAMP. The console says the same in RenderTarget::CreateStates @0x82403A18,
    // which builds that target's depth TextureState with mag = min = 0; this build cannot let
    // a TextureState speak, because SetSamplerStateLowLevel is a no-op, so the words are applied
    // at BIND time instead -- keyed on the bound resource's format, never on a unit number.
    //
    // ApplyRawDepthSamplerState is applied automatically by D3DDevice_SetTexture; this is the
    // named entry for a caller that binds a depth texture some other way.
    void PostFxDepthSampler_ApplyState(u32 luUnit);

    // The units currently holding a raw-depth texture, one bit per sampler. Device::SetState
    // unbinds them before it binds that same texture's surface as the depth-stencil: D3D9 (unlike
    // D3D10+) does NOT auto-unbind, and a texture that is simultaneously a sampler source and the
    // depth target has undefined sample results -- with no error and no log line.
    // Defined in XenonD3D9Shims.cpp.
    u32 PostFxDepthSampler_BoundUnitMask();

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
    //
    // ⚠ DO NOT DELETE ON "BeginRenderAntiAliased EXISTS". It exists as of 2026-08-13
    // (BrnRendererModule.cpp @0x823FFA18) and that was always the wrong condition. FOUR things must
    // hold first, and one part of this bracket must SURVIVE regardless:
    //   (1) THE DEFINITIONS MUST EXIST. That body is compiled out behind
    //       BRN_ANTIALIAS_BRACKET_AVAILABLE because six symbols it calls have no definition in the
    //       linked object set (the four Xenos D3DDevice_* tiling/resolve entry points and
    //       CgsDev::PerfMonGpu::Start/StopMonitor). The full list and what each owes is in that
    //       banner in BrnRendererModule.cpp.
    //   (2) THE POOL MUST ACTUALLY HOLD THE TARGETS. BeginRenderAntiAliased binds
    //       mapRenderTarget[0] (GetAntiAliasBuffer) and ResolveMSAA reads mapRenderTarget[4]
    //       (GetDownSampleBuffer), and NEITHER null-tests -- faithfully, the X360 asm
    //       0x823FFB08-0x823FFB34 has no null test either. Those slots are filled LAZILY on PC by
    //       BrnRendererMemory::PCBringUpCreatePostFxSceneTargets, reached through
    //       EnsurePostFxSceneTargets (BrnRendererModule.cpp:265-282), which returns false until
    //       renderengine::gDevice exists, until the shadow slot-nulling pass has run, and until
    //       gDisplayWidth/Height are non-zero. Until Render gates the bracket call on that
    //       returning true, wiring it up null-derefs on the loading screen.
    //   (3) RENDER MUST CALL IT ON EVERY FRAME THAT RUNS THIS PASS. The console's rebind is
    //       unconditional at Render:725; a PC call that is skipped on any frame leaves the shadow
    //       surfaces bound for the rest of it.
    //   (4) NOTHING BETWEEN THIS PASS AND THAT CALL MAY DRAW WITHOUT BINDING ITS OWN TARGET. On the
    //       console the env-map pass sits in that gap and binds its own; on PC, verify it.
    // WHY THE INVALIDATION IN PCSurfaceBracket_Restore BECOMES UNNECESSARY, once (1)-(4) hold:
    // BeginRenderAntiAliased rebinds THROUGH renderengine::Device::SetState and writes
    // gpLastRenderTargetState in the same breath (X360 0x823FFB2C-0x823FFB34), so the shadow stays
    // in step -- which is exactly what this bracket's raw D3D9 restore cannot do, and is the whole
    // reason the invalidation exists.
    // ⚠ WHAT MUST SURVIVE THE DELETION: the SAMPLER-15 PARK in PCSurfaceBracket_Save is NOT a
    // stand-in for BeginRenderAntiAliased and has no console counterpart at all -- the console can
    // leave the shadow map bound on unit 15 while writing it, because it samples the RESOLVED copy
    // and writes tiled EDRAM, whereas here the texture IS the surface and D3D9 leaves that
    // undefined. Relocate it into the shadow manager's own bracket; deleting it with the surface
    // save/restore would introduce a fresh undefined-behaviour bug that BeginRenderAntiAliased does
    // nothing to prevent.
    // Defined in XenonD3D9Shims.cpp.
    void PCSurfaceBracket_Save();
    void PCSurfaceBracket_Restore();

    // Alpha-to-coverage (rung 9): re-derive the vendor A2C state from its inputs. Called by
    // renderengine::Device::SetState after every render-target bind because the bound colour
    // target's sample count is one of those inputs (XenonD3D9Shims.cpp, the derivation banner
    // above AlphaCoverage_Reconcile). Cheap and idempotent. Defined in XenonD3D9Shims.cpp.
    void PCAlphaCoverage_Reconcile();

    // =========================================================================
    // THE MOTION-BLUR MASK'S PC CARRIER (step 11).
    //
    // The console's per-pixel motion-blur mask is the STENCIL BUFFER: the frame's stencil
    // CLEAR is (u8)(mfWorldBlurAmount*255) and the two car mesh-list windows REPLACE it with
    // (u8)(mfCarsBlurAmount*255) (BrnRendererModule::Render @0x8240C2C0 / @0x8240CEC4 /
    // @0x8240D338; both halves are already live on PC). Its consumer, the composite, samples
    // the depth-stencil as A8R8G8B8 and multiplies the screen velocity by the stencil lane --
    // and THAT is the step Direct3D 9 cannot express, because a D3D9 depth texture has no
    // sampleable stencil lane at all.
    //
    // This copies the console's own stencil content into the SCENE TARGET'S ALPHA LANE, which
    // D3D9 can sample, using the one stencil operation D3D9 does have: the TEST. Two
    // full-screen alpha-only quads (EQUAL carsByte, then NOTEQUAL) write the two bytes as
    // vertex diffuse alpha, so the value reaches `tex2D(SamplerSource, uv).a` in the eight
    // blur permutations bit for bit.
    //
    // CALL IT after every world pass and BEFORE ResolveMSAA, on frames whose layer-0 effects
    // frame has mMotionBlurData.mbIsActive -- the same gate the console's own force windows
    // use. It refuses (and says so once) if the swap chain is bound instead of the scene
    // target. The full derivation, the arithmetic reason the obvious per-draw blend scheme is
    // WRONG, and the two honest limits are on the definition in XenonD3D9Shims.cpp.
    //
    // NOT a bring-up stand-in: it stands in for no console function. What retires it is a
    // backend with a stencil SRV (D3D10+), not more reconstruction.
    // =========================================================================
    void PCStampMotionBlurMask(u32 luCarsBlurStencil, u32 luWorldBlurStencil);

    // =========================================================================
    // FLAG PC bring-up: THE SCENE-TARGET PRESENT BLIT's device state.
    //
    // ⚠ THIS IS NOT THE POST-FX COMPOSITE. The console's BrnPostFx::Render @0x8240A468 is what
    // consumes the resolved scene -- tone map, bloom, depth of field, motion blur, the colour
    // grade -- and it is a SEPARATE, LATER wave. These two exist for one reason: the moment
    // BrnRendererModule.cpp's BRN_ANTIALIAS_BRACKET_AVAILABLE goes to 1, BeginRenderAntiAliased
    // binds the anti-alias buffer's surfaces and the world passes stop drawing into the swap
    // chain. Nothing then puts the result back, so without a blit the screen is the black that
    // renderengine::Device::FrameBegin cleared the back buffer to -- and the wave would read as a
    // regression rather than as the step it is.
    //
    // WHAT RETIRES THEM: BrnPostFx::Render drawing the composite itself. Delete these two, their
    // definitions in XenonD3D9Shims.cpp, PCBringUpBlitSceneTargetToBackBuffer in
    // BrnRendererModule.cpp and its call in Render, all together, on that day. They are a
    // bring-up stand-in for a real console function, exactly like PCSurfaceBracket_* above --
    // and, like it, the thing that retires them is a RECONSTRUCTION, not merely a decision.
    //
    // WHY THE Im2d PATH CANNOT DO THIS ALONE (each item is a live defeat, not a tidy-up):
    //   * The BACK BUFFER IS NOT BOUND at that point in the frame -- the anti-alias buffer is.
    //     Rebinding it is EndRenderAntiAliased's job (@0x82408B00, not yet reconstructed), so
    //     Begin does it: GetBackBuffer(0) -> SetRenderTarget(0), and it nulls
    //     gpLastRenderTargetState for the same reason PCSurfaceBracket_Restore does -- a raw
    //     D3D9 rebind is invisible to the engine's "last state installed" cache, and a cache
    //     that lies causes a SKIPPED bind, which is what put the shadow cascades in the back
    //     buffer.
    //   * ImRenderer<V>::BeginRendering enables ALPHA BLENDING (SRCALPHA/INVSRCALPHA) and
    //     ImRendererBase::SetTexture sets D3DTSS_ALPHAOP to MODULATE. The scene target's alpha is
    //     whatever the world left, and BeginRenderAntiAliased clears it to 0.0f -- so the blit
    //     would be multiplied by zero alpha and draw NOTHING. Begin forces opaque,
    //     texture-only (SELECTARG1) stage ops; End puts the 2D tail's blended MODULATE back.
    //   * D3D9's default address mode is WRAP, and the half-texel-corrected UVs run to
    //     1.0 + 0.5/width. Begin sets CLAMP.
    // =========================================================================
    void PCSceneBlit_Begin();
    void PCSceneBlit_End();

    // =========================================================================
    // FLAG PC bring-up INITIALISATION: clear a freshly created off-screen target ONCE, at
    // creation, so that the first frame which renders into it does not render into undefined
    // memory.
    //
    // THE CONSOLE DOES NOT NEED THIS, and the asymmetry is the whole justification. On the Xenos
    // the scene target is EDRAM and the TILING PASS clears colour, depth and stencil at the TOP
    // of every frame, the first one included -- that is what D3DDevice_BeginTiling's pClearColor
    // / ClearZ / stencil arguments are. The PC bracket's only clear is at the BOTTOM of the
    // frame: the 0x300 D3DDevice_Resolve clears the scene target behind its copy, which is what
    // leaves it clean for the NEXT frame's world pass. That is faithful -- and it leaves the
    // FIRST frame cleared by nothing at all, rendering the world into a D3DPOOL_DEFAULT texture
    // whose contents D3D9 leaves UNDEFINED, over undefined depth, and that frame is copied and
    // PRESENTED.
    //
    // Called ONCE per target from BrnRendererMemory::PCBringUpCreatePostFxSceneTargets, with the
    // section-0 state of each of the two targets it creates. Defined in XenonD3D9Shims.cpp,
    // beside the resolve whose clear it borrows (TilingClearBoundSurfaces), so the viewport /
    // scissor bracket and the D3DCLEAR_STENCIL all-or-nothing retry exist exactly once. The
    // values it writes, and the attestation for each of them, are on that definition.
    //
    // DELETE IT when a frame-TOP clear exists again -- the multisampled path's
    // D3DDevice_BeginTiling clear, or BrnPostFx::Render @0x8240A468's own composite -- NOT
    // merely when the bracket lands.
    // =========================================================================
    void PCBringUpClearRenderTargetState(const RenderTargetState* lpState);

    // =========================================================================
    // [PC-platform leaf] PCInstallDefaultRenderTargetState -- give the engine its "device's own
    // surface" state.
    //
    // The console installs rw::graphics::postfx::gpDefaultRenderTargetState (X360 dword_83271614)
    // from renderengine::Device::Start: it is the front-buffer surface descriptor, and it is what
    // every USE_DEVICE_FOR_WRITE render target binds through -- rw::graphics::postfx::RenderTarget::
    // Begin @0x823F9250 picks it for that colour mode, and CgsRenderTarget::SetRenderTargetState
    // @0x827E7588 falls back to it for a target with no section-0 state. The pool's BACK_BUFFER
    // target (BrnRendererMemory::CreateBackBuffer @0x823F6F78) is USE_DEVICE_FOR_WRITE, so the
    // post-fx composite (BrnPostFx::Render @0x8240A468: `lpDestinationRenderTarget->Begin(0)`)
    // lands on the device back buffer ONLY if this state exists. Until now this build's default
    // was null and Device::SetState(null) is a no-op -- which meant Begin(0) on the back buffer
    // would leave the SCENE target bound and the composite would draw the scene onto itself.
    //
    // On PC the device's own surfaces are the swap chain's back buffer (colour) and the
    // D3DPRESENT_PARAMETERS auto depth-stencil (D24S8, swap-chain sized -- device.cpp), which is
    // what this captures, once, right after CreateDevice, at the console's own placement in
    // Device::Start. Both surfaces are AddRef'd for the device's lifetime (no Reset path exists on
    // this build). Defined in PostFxRenderTargetPCLeaf.cpp beside Device::SetState, the consumer.
    // =========================================================================
    void PCInstallDefaultRenderTargetState(u32 luWidth, u32 luHeight);
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
