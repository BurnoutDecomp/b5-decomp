#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"  // rw::Resource, rw::IResourceAllocator
#include "pc/gcm/renderengine/DeviceParameters.h"  // renderengine::DeviceParameters (by-value Construct param)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnRendererMemory::Construct       @ 0x823FCA38  (EXECUTED in the boot trace)
//   BrnRendererMemory::CreateBackBuffer@ 0x823F6F78  (EXECUTED in the boot trace)
//
// BrnRendererMemory owns the renderer's pool of off-screen render targets (anti-alias, downsample,
// back buffer, shadow maps, env map, bloom, depth-of-field, work, particle, sun-corona, snapshot)
// plus the two blit shader-program pairs (depth blit, composite blit) and the blit texture state.
// Construct builds every render target (delegating to the Create* helpers) then compiles the four
// blit shader programs and the blit texture state through the renderengine resource pipeline;
// CreateBackBuffer builds the back-buffer render target and patches it to share the down-sample
// buffer's depth surface. It is embedded by value inside BrnRendererModule (mAllocatedRenderTargets).
//
// LAYOUT: the member set + order is the DecFIGS DWARF (GameSource/Graphics/BrnRendererMemory.h),
// reconciled against the X360 ARTIST asm store offsets (offset authority):
//   - mapRenderTarget is an 11-entry pointer array (the Construct zero-loop runs 11 times and the
//     blit program pointers begin immediately after at the next dword; the DWARF's 12th entry is a
//     PS3 merge-window delta the X360 build does not carry).
//   - the four blit ProgramBuffer pointers + the two TempRegisterCount byte-pairs + the blit
//     TextureState pointer + the blit-state Resource land exactly where the asm stores them
//     (0x2C/0x30/0x38/0x3C, the byte pairs, 0x44, 0x48).
//   - mu32ScreenWidth / mu32ScreenHeight take the front-buffer resolution from the device parameters.
// The x64 compile widens the pointers; members are reached BY NAME, never by raw offset.
//
// X360-ATTESTATION GATE: only the methods the X360 ARTIST ledger attests for this class are declared
// (Construct + the Create* render-target helpers it calls + CreateBackBuffer). The many PS3-only
// getters/Blit paths the DWARF lists are left out.

namespace renderengine
{
    class ProgramBuffer;   // the four blit shader programs are held by pointer
    class TextureState;    // the blit texture state is held by pointer
}

class CgsRenderTarget;     // the render-target pool entries are held by pointer

struct BrnRendererMemory
{
    // Render-target pool slot indices (the index each Create* helper stores its target into, read off
    // the X360 asm: AntiAlias->0, ShadowMap0->1, ShadowMap1->2, EnvMap->3, DownSample->4, BackBuffer->5,
    // Bloom->6, DepthOfField->7, Work->8, Particle->9, SunCorona->10).
    enum ERenderTargetSlot
    {
        E_RENDER_TARGET_ANTI_ALIAS     = 0,
        E_RENDER_TARGET_SHADOW_MAP_0   = 1,
        E_RENDER_TARGET_SHADOW_MAP_1   = 2,
        E_RENDER_TARGET_ENV_MAP        = 3,
        E_RENDER_TARGET_DOWN_SAMPLE    = 4,
        E_RENDER_TARGET_BACK_BUFFER    = 5,
        E_RENDER_TARGET_BLOOM          = 6,
        E_RENDER_TARGET_DEPTH_OF_FIELD = 7,
        E_RENDER_TARGET_WORK           = 8,
        E_RENDER_TARGET_PARTICLE       = 9,
        E_RENDER_TARGET_SUN_CORONA     = 10,
        E_RENDER_TARGET_COUNT          = 11
    };

    // 0x823FCA38 -- build every render target then compile the blit shader programs + blit texture
    // state. lParameters supplies the front-buffer resolution; lpAllocator is the rw resource
    // allocator the module owns; lbEnableMSAA enables anti-aliasing on the back/anti-alias buffers.
    void Construct(renderengine::DeviceParameters lParameters,
                   rw::IResourceAllocator* lpAllocator,
                   bool lbEnableMSAA);

    // 0x823F6F78 -- build the back-buffer render target (a 1-section colour+depth target at the
    // front-buffer resolution), realise it through the allocator, then patch its post-fx render
    // target to share the down-sample buffer's depth surface and use the default render-target state.
    void CreateBackBuffer(rw::IResourceAllocator* lpAllocator, u32 luWidth, u32 luHeight);

    // FLAG PC bring-up scope (2026-08-12): NOT an X360 function. Construct() above is the console's
    // only entry point into the render-target pool, and it is UNLINKABLE on PC today - eight of the
    // nine Create*Buffer helpers have no body in the tree, BrnResource::Allocators::
    // GetGlobalGraphicsAllocator() is declaration-only, and the four gacIm2d*BlitProgram microcode
    // blobs are external generated data with no definition. Rather than fabricate those nine bodies
    // to satisfy a linker, this entry point does the slice the shadow wave actually needs: clear the
    // pool and build ONLY the shadow-map target, where Construct's third call would. Delete it (and
    // call Construct) the moment the rest of the pool exists.
    //
    // ONE store differs from CreateShadowmapBuffer's, and is flagged at the definition: the target is
    // described as ONE section at the COMBINED 1280x1920 rather than three sections of 1280x640,
    // because PC D3D9 has no EDRAM-surface + resolve pair -- the depth TEXTURE is the render surface,
    // so the surface is the whole atlas and the per-cascade band is a viewport. Everything else is
    // CreateShadowmapBuffer's, value for value.
    void PCBringUpCreateShadowMapBufferOnly(rw::IResourceAllocator* lpAllocator);

    // 0x823F4910 -- return one of the shadow-map render targets by index. The X360 asserts the index
    // is in [0,4) and returns mapRenderTarget[liIndex + 1] (slot 0 is the anti-alias buffer; the
    // shadow-map slots begin at slot 1). Called by ShadowMapRenderManager::Begin/EndRenderShadowMap.
    CgsRenderTarget* GetShadowMapBuffer(s32 liIndex);

private:
    // The Create* render-target helpers Construct delegates to. Bodies live in their own (already
    // attested) TUs; declared here so Construct can call them by name.
    void CreateAntiAliasBuffer(rw::IResourceAllocator* lpAllocator, bool lbEnableMSAA);
    void CreateDownSampleBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateShadowmapBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateEnvmapBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateBloomBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateDepthOfFieldBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateWorkBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateParticleBuffer(rw::IResourceAllocator* lpAllocator);
    void CreateSunCoronaBuffer(rw::IResourceAllocator* lpAllocator);

    // --- data layout (DWARF order; offsets verified against the X360 asm) --------------------------

    // The render-target pool (index by E_RENDER_TARGET_*; the back buffer is slot 5, the down-sample
    // buffer slot 4 -- CreateBackBuffer reads mapRenderTarget[4] and writes mapRenderTarget[5]).
    CgsRenderTarget* mapRenderTarget[E_RENDER_TARGET_COUNT];    // +0x00 (11 dwords zeroed in Construct)

    // Depth-blit shader program pair + its working/original temp-register counts.
    renderengine::ProgramBuffer* mpDepthBlitVertexProgramBuffer;      // +0x2C
    renderengine::ProgramBuffer* mpDepthBlitPixelProgramBuffer;       // +0x30
    u8                           mu8DepthBlitTempRegisterCountCurrent;  // +0x34
    u8                           mu8DepthBlitTempRegisterCountOriginal; // +0x35

    // Composite-blit shader program pair + its working/original temp-register counts.
    renderengine::ProgramBuffer* mpCompositeBlitVertexProgramBuffer;   // +0x38
    renderengine::ProgramBuffer* mpCompositeBlitPixelProgramBuffer;    // +0x3C
    u8                           mu8CompositeBlitTempRegisterCountCurrent;  // +0x40
    u8                           mu8CompositeBlitTempRegisterCountOriginal; // +0x41

    // The blit texture state object + its backing resource block.
    renderengine::TextureState* mpBlitTextureState;            // +0x44
    rw::Resource                mBlitTextureStateResource;     // +0x48

    // Front-buffer (display) resolution the renderer memory was sized for.
    u32 mu32ScreenWidth;                                       // +0x5C
    u32 mu32ScreenHeight;                                      // +0x60

    bool mbIsSD;                                               // +0x64
    u8   mu8TileIndex;                                         // +0x65
    u8   mu8ZCullIndex;                                        // +0x66
    u32  muZCullAddress;                                       // +0x68
    u32  muTileCompressionAddress;                             // +0x6C

    void* mpSnapshotBufferPixels;                             // +0x70 (DWARF)
};
