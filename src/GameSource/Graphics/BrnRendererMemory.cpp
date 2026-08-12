#include "GameSource/Graphics/BrnRendererMemory.h"

#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"  // CgsRenderTarget (+ serialise-side setters)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // postfx::RenderTarget + gpDefaultRenderTargetState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBuffer
#include "GameSource/Resource/BrnResourceAllocator.h"          // BrnResource::Allocators::GetGlobalGraphicsAllocator

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnRendererMemory::Construct             @ 0x823FCA38  (EXECUTED in the boot trace)
//   BrnRendererMemory::CreateShadowmapBuffer @ 0x823F6D98
//   BrnRendererMemory::CreateBackBuffer      @ 0x823F6F78  (EXECUTED in the boot trace)
//   BrnRendererMemory::GetShadowMapBuffer    @ 0x823F4910
//
// BrnRendererMemory owns the renderer's render-target pool and the blit shader programs. Construct
// builds every render target through the Create* helpers, then compiles the four blit shader programs
// (depth blit vertex/pixel, composite blit vertex/pixel) and the blit texture state through the
// renderengine resource pipeline (GetResourceDescriptor -> allocator DoAllocate -> Initialize), the
// same idiom the post-fx effects use. CreateBackBuffer builds the back-buffer render target and patches
// its post-fx render target to share the down-sample buffer's depth-stencil section.

// ---------------------------------------------------------------------------------------------
// BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE -- the FULL render-target pool gate (PC bring-up,
// 2026-08-12).
//
// BrnRendererMemory::Construct @0x823FCA38 is reconstructed and correct, and it is the console's
// ONLY entry point into the pool. It is also UNLINKABLE on this build, and will stay so until a
// lot more of the renderer exists. Its unresolved set, measured with dumpbin, is fourteen
// symbols, none of which has a body anywhere in the tree:
//   * the eight sibling pool helpers -- CreateAntiAliasBuffer, CreateDownSampleBuffer,
//     CreateEnvmapBuffer, CreateBloomBuffer, CreateDepthOfFieldBuffer, CreateWorkBuffer,
//     CreateParticleBuffer, CreateSunCoronaBuffer (CreateShadowmapBuffer and CreateBackBuffer,
//     the two that ARE bodied, live in this file),
//   * BrnResource::Allocators::GetGlobalGraphicsAllocator() -- declaration-only, defined nowhere,
//   * the four gacIm2d*BlitProgram shader-microcode blobs -- external generated data,
//   * renderengine::TextureState::GetResourceDescriptor.
// Writing any of those to satisfy a linker would be fabrication, so Construct is COMPILED OUT and
// the shadow wave's slice goes through PCBringUpCreateShadowMapBufferOnly below. The body stays
// here, unchanged and reviewable, for the wave that lands the rest of the pool: flip this to 1
// then, delete the bring-up entry point, and call Construct.
//
// Everything else in this TU -- CreateShadowmapBuffer, CreateBackBuffer, GetShadowMapBuffer -- is
// LIVE and in the link.
#define BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE 0
// ---------------------------------------------------------------------------------------------

#if BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE
// --- renderengine::TextureState (canonical home: states/texturestate.cpp) -------------------------
// Re-declared here as the minimal call surface Construct uses (its GetResourceDescriptor static + the
// sampler param block). The canonical class lives in its own committed TU with no shared header; the
// param block mirrors its layout (SamplerStateParameters 0x48 + trailing raster) so the shapes agree
// if a shared header is ever reconstructed. The X360 Construct builds the param block but its
// GetResourceDescriptor (fixed-size texture-state descriptor) ignores it -- the block is retained as
// the original source's seeded sampler config (clamp LOD floats, filter-mode dwords, address/filter
// bytes), carried until the texture state is Initialize'd lazily on first use.
namespace renderengine
{
    struct SamplerStateParameters
    {
        u32 mu32Reserved00;   // +0x00 (=0)
        u32 mu32Reserved04;   // +0x04 (=0)
        u32 mu32Reserved08;   // +0x08 (=0)
        u32 mu32Reserved0C;   // +0x0C (=0)
        u32 mu32Reserved10;   // +0x10 (=0)
        u32 mu32FilterMode;   // +0x14 (=2)
        u32 mu32Reserved18;   // +0x18 (=0)
        u32 mu32Reserved1C;   // +0x1C (=0)
        u32 mu32MaxAnisotropy;// +0x20 (=0xD)
        u32 mu32Reserved24;   // +0x24 (=0)
        u32 mu32MipFilter;    // +0x28 (=1)
        f32 mfMinLod;         // +0x2C (clamp LOD, flt_82001CC0 = 0.0)
        f32 mfMaxLod;         // +0x30 (clamp LOD, flt_82001CC0 = 0.0)
        u32 mu32Reserved34;   // +0x34 (=0)
        u32 mu32Reserved38;   // +0x38 (=0)
        u32 mu32Reserved3C;   // +0x3C (=0)
        u8  mu8AddressU;      // +0x40 (=0)
        u8  mu8AddressV;      // +0x41 (=0)
        u8  mu8AddressW;      // +0x42 (=0)
        u8  mu8MinFilter;     // +0x43 (=1)
        u8  mu8MagFilter;     // +0x44 (=1)
        u8  mau8Pad45[3];     // +0x45 (alignment to +0x48)
    };

    struct TextureStateParameters
    {
        SamplerStateParameters mSampler;  // +0x00
        u32                    muRaster;  // +0x48 bound-raster pointer (=0)
    };

    class TextureState
    {
    public:
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor);
    };
}
#endif  // BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE

namespace
{
    // The packed Xenos surface formats CreateBackBuffer seeds the colour section with (X360 immediates).
    const u32 KU_BACK_BUFFER_BUFFER_FORMAT  = 0x18280186u;
    const u32 KU_BACK_BUFFER_TEXTURE_FORMAT = 0x18280106u;

    // --- shadow-map render-target sizing -------------------------------------------------------------
    // The combined shadow-map buffer's dimensions, read straight out of the image's data segment by
    // CreateShadowmapBuffer @0x823F6D98 (lwz from dword_82F24240 / dword_82F24244). Re-dumped from
    // BURNOUT_X360_ARTIST.XEX.i64 for this reconstruction: 0x82F24240 = 0x00000500 = 1280 and
    // 0x82F24244 = 0x00000780 = 1920. NO function in the image writes either dword - the only xrefs
    // are the two reads in that one helper - so they are initialised data, not runtime configuration.
    //
    // The height dword is the COMBINED height of all three cascades; the helper divides it by three to
    // get the per-section height (1920 / 3 = 640). That is the same 3 the section count is set to, and
    // it matches gbCombinedShadowMapViewport (byte_82F2423F, which dumps as 0x01) - one 1280x1920
    // surface carrying three stacked 1280x640 cascade viewports.
    const u32 KU_SHADOW_MAP_WIDTH            = 1280u;
    const u32 KU_SHADOW_MAP_COMBINED_HEIGHT  = 1920u;
    const u32 KU_SHADOW_MAP_NUM_CASCADES     = 3u;

    // The packed Xenos depth-surface format the shadow map is built with; the X360 loads the same
    // immediate (lis 0x2D20 / ori 0x196) into BOTH the depth buffer format and the depth texture
    // format, which is what makes the target sampleable as a shadow map.
    const u32 KU_SHADOW_MAP_DEPTH_FORMAT = 0x2D200196u;

#if BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE
    // The clamp LOD-bias the blit sampler is seeded with (X360 flt_82001CC0).
    const f32 KF_BLIT_SAMPLER_LOD_BIAS = 0.0f;

    // The four blit shader microcode blobs the X360 image embeds (Im2dDepthBlit / Im2dCompositeBlit
    // vertex+pixel). The compiled bytes are external generated shader data resolved at link; here we
    // name the blob + its byte size (the sizes are the X360 ProgramBufferParameters size words).
    extern "C" const u8 gacIm2dDepthBlitVertexProgram[];      // 340 bytes (X360 unk_8203DAA8)
    extern "C" const u8 gacIm2dDepthBlitPixelProgram[];       // 236 bytes (X360 unk_8203DC00)
    extern "C" const u8 gacIm2dCompositeBlitVertexProgram[];  // 400 bytes (X360 unk_8203DCF0)
    extern "C" const u8 gacIm2dCompositeBlitPixelProgram[];   // 508 bytes (X360 unk_8203DE80)

    const u32 KU_DEPTH_BLIT_VS_SIZE     = 340;
    const u32 KU_DEPTH_BLIT_PS_SIZE     = 236;
    const u32 KU_COMPOSITE_BLIT_VS_SIZE = 400;
    const u32 KU_COMPOSITE_BLIT_PS_SIZE = 508;

    // Compile one blit shader program: build its resource descriptor, carve the resource from the
    // allocator, then initialise the program over it. Mirrors BrnPostFx*::CreateProgram (the X360
    // inlines this four times in Construct).
    renderengine::ProgramBufferData* CreateBlitProgram(rw::IResourceAllocator* lpAllocator,
                                                       const u8* lpMicrocode, u32 luSize, bool lbPixelProgram)
    {
        renderengine::ProgramBufferParameters lParameters = {};
        lParameters.muFunction   = static_cast<u32>(reinterpret_cast<usize>(lpMicrocode));
        lParameters.muShaderType = lbPixelProgram ? 1u : 0u;
        lParameters.muReserved8  = luSize;

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);
        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
        return renderengine::ProgramBuffer::Initialize(
            reinterpret_cast<renderengine::ProgramResourceLayout*>(&lResource), &lParameters);
    }
#endif  // BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE
}

#if BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE
// 0x823FCA38 -- build every render target, then compile the blit shader programs + blit texture state.
void BrnRendererMemory::Construct(renderengine::DeviceParameters lParameters,
                                  rw::IResourceAllocator* lpAllocator,
                                  bool lbEnableMSAA)
{
    // Reset the tile / z-cull reservation cursors and the snapshot pixels pointer (the X360 zeroes the
    // tile/zcull index bytes and the two reservation-address dwords up front).
    mu8TileIndex             = 0;
    muTileCompressionAddress = 0;
    mu8ZCullIndex            = 0;
    muZCullAddress           = 0;

    // The renderer memory is sized for the front-buffer (display) resolution.
    mu32ScreenWidth  = lParameters.muFrontBufferWidth;
    mu32ScreenHeight = lParameters.muFrontBufferHeight;

    // Clear the render-target pool.
    for (u32 luSlot = 0; luSlot < E_RENDER_TARGET_COUNT; ++luSlot)
    {
        mapRenderTarget[luSlot] = nullptr;
    }

    // Build every render target (each helper allocates its CgsRenderTarget into its pool slot).
    CreateAntiAliasBuffer(lpAllocator, lbEnableMSAA);
    CreateDownSampleBuffer(lpAllocator);
    CreateShadowmapBuffer(lpAllocator);
    CreateBloomBuffer(lpAllocator);
    CreateDepthOfFieldBuffer(lpAllocator);
    CreateWorkBuffer(lpAllocator);
    CreateEnvmapBuffer(lpAllocator);
    CreateParticleBuffer(lpAllocator);
    CreateSunCoronaBuffer(lpAllocator);
    CreateBackBuffer(lpAllocator, mu32ScreenWidth, mu32ScreenHeight);

    // Compile the depth-blit shader program pair.
    mpDepthBlitVertexProgramBuffer = reinterpret_cast<renderengine::ProgramBuffer*>(
        CreateBlitProgram(lpAllocator, gacIm2dDepthBlitVertexProgram, KU_DEPTH_BLIT_VS_SIZE, false));
    mpDepthBlitPixelProgramBuffer = reinterpret_cast<renderengine::ProgramBuffer*>(
        CreateBlitProgram(lpAllocator, gacIm2dDepthBlitPixelProgram, KU_DEPTH_BLIT_PS_SIZE, true));

    // Compile the composite-blit shader program pair.
    mpCompositeBlitVertexProgramBuffer = reinterpret_cast<renderengine::ProgramBuffer*>(
        CreateBlitProgram(lpAllocator, gacIm2dCompositeBlitVertexProgram, KU_COMPOSITE_BLIT_VS_SIZE, false));
    mpCompositeBlitPixelProgramBuffer = reinterpret_cast<renderengine::ProgramBuffer*>(
        CreateBlitProgram(lpAllocator, gacIm2dCompositeBlitPixelProgram, KU_COMPOSITE_BLIT_PS_SIZE, true));

    // Build the blit texture state (a clamped bilinear sampler) through the renderengine resource
    // pipeline, carving its backing block from the engine's global graphics-memory allocator.
    renderengine::TextureStateParameters lTextureStateParams = {};
    lTextureStateParams.mSampler.mu32FilterMode    = 2;
    lTextureStateParams.mSampler.mu32MaxAnisotropy = 0xD;
    lTextureStateParams.mSampler.mu32MipFilter     = 1;
    lTextureStateParams.mSampler.mfMinLod          = KF_BLIT_SAMPLER_LOD_BIAS;
    lTextureStateParams.mSampler.mfMaxLod          = KF_BLIT_SAMPLER_LOD_BIAS;
    lTextureStateParams.mSampler.mu8MinFilter      = 1;
    lTextureStateParams.mSampler.mu8MagFilter      = 1;

    // Construct only allocates the texture state's backing resource block (it is Initialize'd lazily
    // on first use); the seeded sampler params are carried until then. The X360 builds the descriptor,
    // carves the 5-base resource from the global graphics allocator, stores it, and leaves the texture
    // state pointer null.
    (void)lTextureStateParams;
    rw::BaseResourceDescriptors<5> lTextureStateDescriptor;
    renderengine::TextureState::GetResourceDescriptor(&lTextureStateDescriptor);
    mBlitTextureStateResource = BrnResource::Allocators::GetGlobalGraphicsAllocator()->DoAllocate(
        reinterpret_cast<const rw::ResourceDescriptor&>(lTextureStateDescriptor), nullptr);

    // The blit texture state object pointer is resolved lazily; null until first bound.
    mpBlitTextureState = nullptr;
}
#endif  // BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE

// FLAG PC bring-up scope (2026-08-12): NOT an X360 function -- see the
// BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner above for why Construct() cannot be linked yet.
// This builds the shadow-map slice of the pool and nothing else: the same pool clear Construct
// does, then the shadow-map target. No other pool slot is touched (they stay null, which is what
// the pool's own null-checks expect).
//
// IT IS NOT A CALL TO CreateShadowmapBuffer, and the difference is deliberate. The console
// describes the shadow target as THREE sections of 1280x640, because on the Xenos each cascade is
// rendered into one 1280x640 tiled EDRAM surface and RESOLVED into its own band of a 1280x1920
// texture. PC Direct3D 9 has neither the EDRAM surface nor the resolve: the depth TEXTURE is the
// surface that gets rendered into, so the surface is the full combined 1280x1920 and the per-
// cascade band is selected by the VIEWPORT.
//
// Describing that truthfully -- ONE section at the combined 1280x1920 -- has a second payoff: it
// makes BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap take its own SINGLE-section
// branch, which is console code that already packs the faces into vertical thirds
// (offsetY = index * height/3). So cascade n lands on rows [n*640, (n+1)*640) of one texture,
// which is exactly the 1x3 vertical-strip atlas the recovered ShadowMap_WorldToLight /
// ShadowMap_Constants matrices encode (Y scaled 1/3, offsets 0 / 1/3 / 2/3). Nothing in the
// console path is bent to fit; the PC surface is simply described as what it is.
//
// Every other store below is CreateShadowmapBuffer's, value for value.
void BrnRendererMemory::PCBringUpCreateShadowMapBufferOnly(rw::IResourceAllocator* lpAllocator)
{
    for (u32 luSlot = 0; luSlot < E_RENDER_TARGET_COUNT; ++luSlot)
    {
        mapRenderTarget[luSlot] = nullptr;
    }

    CgsRenderTarget* lpShadowMap = new CgsRenderTarget();
    mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] = lpShadowMap;

    // Depth-only: no colour section is in use, so the post-fx colour mode comes out NONE.
    for (u32 luSection = 0; luSection < CgsRenderTarget::KU_NUM_COLOUR_SECTIONS; ++luSection)
    {
        lpShadowMap->ClearColourTargetInUse(luSection);
    }

    // FLAG PC bring-up: the COMBINED extent + one section (see above). The console's
    // CreateShadowmapBuffer @0x823F6D98 sets (1280, 1920/3) and three sections.
    lpShadowMap->SetDimensions(KU_SHADOW_MAP_WIDTH, KU_SHADOW_MAP_COMBINED_HEIGHT);
    lpShadowMap->SetNumSections(1);

    lpShadowMap->SetNumMipMaps(1);
    lpShadowMap->SetMultisampleFormat(0);
    lpShadowMap->SetUseDepthStencilAsTexture(true);

    lpShadowMap->SetDepthTargetInUse(true);
    lpShadowMap->SetDepthTargetBufferFormat(KU_SHADOW_MAP_DEPTH_FORMAT);
    lpShadowMap->SetDepthTargetTextureFormat(KU_SHADOW_MAP_DEPTH_FORMAT);
    lpShadowMap->SetDepthTargetBaseEDRAM(0);
    lpShadowMap->SetDepthTargetTileIndex(0);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual). On PC that
    // reaches rw::graphics::postfx::RenderTarget::Initialize in
    // pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp, which creates the D3D9 depth texture.
    lpShadowMap->Construct(lpAllocator);
}

// 0x823F6D98 -- build the shadow-map render target: ONE combined depth-only surface carrying all
// three cascades, stored in pool slot 1 (E_RENDER_TARGET_SHADOW_MAP_0, the slot GetShadowMapBuffer(0)
// hands back). Every colour section is switched off and only the depth/stencil record is described,
// with the same packed depth format used for both the buffer and the sampled texture, so the finished
// target is a pure sampleable shadow depth buffer.
//
// The pool slot is written BEFORE the target is described (the X360 stores the new pointer to +0x04
// immediately after the constructor returns) - unlike CreateBackBuffer, which stores its slot last.
void BrnRendererMemory::CreateShadowmapBuffer(rw::IResourceAllocator* lpAllocator)
{
    const u32 luWidth  = KU_SHADOW_MAP_WIDTH;
    const u32 luHeight = KU_SHADOW_MAP_COMBINED_HEIGHT / KU_SHADOW_MAP_NUM_CASCADES;

    CgsRenderTarget* lpShadowMap = new CgsRenderTarget();
    mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] = lpShadowMap;

    // Depth-only: no colour section is in use, so the post-fx colour mode comes out NONE.
    for (u32 luSection = 0; luSection < CgsRenderTarget::KU_NUM_COLOUR_SECTIONS; ++luSection)
    {
        lpShadowMap->ClearColourTargetInUse(luSection);
    }

    lpShadowMap->SetDimensions(luWidth, luHeight);
    lpShadowMap->SetNumMipMaps(1);
    lpShadowMap->SetMultisampleFormat(0);
    // One section per cascade -- the combined-viewport layout ShadowMapRenderManager renders into.
    lpShadowMap->SetNumSections(static_cast<u8>(KU_SHADOW_MAP_NUM_CASCADES));
    lpShadowMap->SetUseDepthStencilAsTexture(true);

    lpShadowMap->SetDepthTargetInUse(true);
    lpShadowMap->SetDepthTargetBufferFormat(KU_SHADOW_MAP_DEPTH_FORMAT);
    lpShadowMap->SetDepthTargetTextureFormat(KU_SHADOW_MAP_DEPTH_FORMAT);
    lpShadowMap->SetDepthTargetBaseEDRAM(0);
    // Tile slot 0 (the constructor's -1 "unassigned" sentinel is overwritten here).
    lpShadowMap->SetDepthTargetTileIndex(0);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpShadowMap->Construct(lpAllocator);
}

// 0x823F6F78 -- build the back-buffer render target. A 1-section colour+depth target at the front-buffer
// resolution; once built, its post-fx render target shares the down-sample buffer's resolved depth
// surface and uses the engine default render-target state for section 0.
void BrnRendererMemory::CreateBackBuffer(rw::IResourceAllocator* lpAllocator, u32 luWidth, u32 luHeight)
{
    CgsRenderTarget* lpBackBuffer = new CgsRenderTarget();

    // Clear every colour section's in-use flag, then describe section 0 as the active colour surface.
    for (u32 luSection = 0; luSection < CgsRenderTarget::KU_NUM_COLOUR_SECTIONS; ++luSection)
    {
        lpBackBuffer->ClearColourTargetInUse(luSection);
    }

    lpBackBuffer->SetDimensions(luWidth, luHeight);
    lpBackBuffer->SetNumMipMaps(1);
    lpBackBuffer->SetMultisampleFormat(0);
    // NOTE: the X360 CreateBackBuffer @0x823F6F78 performs NO store to CgsRenderTarget +0x14
    // (mu8NumSections); the back buffer keeps the constructor's section-count default. The earlier
    // draft's SetNumSections(1) was added behavior the binary lacks -- removed.
    lpBackBuffer->SetUseDepthStencilAsTexture(false);

    lpBackBuffer->SetColourTargetInUse(0);
    lpBackBuffer->SetColourTargetBufferFormat(0, KU_BACK_BUFFER_BUFFER_FORMAT);
    lpBackBuffer->SetColourTargetTextureFormat(0, KU_BACK_BUFFER_TEXTURE_FORMAT);
    lpBackBuffer->SetColourTargetBaseEDRAM(0, 0);          // stw r30, 0x28(r31)

    // The back buffer describes no depth surface of its own -- it borrows the down-sample buffer's
    // below, so the depth record is explicitly switched off (stb r30, 0xDC(r31)).
    lpBackBuffer->SetDepthTargetInUse(false);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpBackBuffer->Construct(lpAllocator);

    // The back buffer borrows the down-sample buffer's depth surface, so that target must exist.
    CGS_ASSERT(mapRenderTarget[E_RENDER_TARGET_DOWN_SAMPLE] != nullptr, "GetDownSampleBuffer() != NULL");

    // Share the down-sample buffer's resolved depth-stencil section (section 4) and bind the engine
    // default render-target state for section 0.
    rw::graphics::postfx::RenderTarget* lpBackBufferTarget = lpBackBuffer->GetRenderTarget();
    rw::graphics::postfx::RenderTarget* lpDownSampleTarget =
        mapRenderTarget[E_RENDER_TARGET_DOWN_SAMPLE]->GetRenderTarget();
    lpBackBufferTarget->SetSectionRenderTargetState(4, lpDownSampleTarget->GetSectionRenderTargetState(4));
    lpBackBufferTarget->SetSectionRenderTargetState(0, rw::graphics::postfx::gpDefaultRenderTargetState);

    mapRenderTarget[E_RENDER_TARGET_BACK_BUFFER] = lpBackBuffer;
}

// 0x823F4910 -- fetch a shadow-map render target by index. The X360 asserts the index is in [0,4)
// then returns mapRenderTarget[liIndex + 1] (the +1 skips the anti-alias slot; the shadow-map pool
// begins at slot 1). The asm bounds the index but does NOT clamp the +1 read -- it is reproduced
// verbatim (no added guard on the read).
CgsRenderTarget* BrnRendererMemory::GetShadowMapBuffer(s32 liIndex)
{
    CGS_ASSERT(liIndex >= 0 && liIndex < 4, "liIndex >=0 && liIndex<4");
    return mapRenderTarget[liIndex + 1];
}
