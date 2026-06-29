#include "GameSource/Graphics/BrnRendererMemory.h"

#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"  // CgsRenderTarget (+ serialise-side setters)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // postfx::RenderTarget + gpDefaultRenderTargetState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBuffer
#include "GameSource/Resource/BrnResourceAllocator.h"          // BrnResource::Allocators::GetGlobalGraphicsAllocator

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnRendererMemory::Construct        @ 0x823FCA38  (EXECUTED in the boot trace)
//   BrnRendererMemory::CreateBackBuffer @ 0x823F6F78  (EXECUTED in the boot trace)
//
// BrnRendererMemory owns the renderer's render-target pool and the blit shader programs. Construct
// builds every render target through the Create* helpers, then compiles the four blit shader programs
// (depth blit vertex/pixel, composite blit vertex/pixel) and the blit texture state through the
// renderengine resource pipeline (GetResourceDescriptor -> allocator DoAllocate -> Initialize), the
// same idiom the post-fx effects use. CreateBackBuffer builds the back-buffer render target and patches
// its post-fx render target to share the down-sample buffer's depth-stencil section.

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

namespace
{
    // The clamp LOD-bias the blit sampler is seeded with (X360 flt_82001CC0).
    const f32 KF_BLIT_SAMPLER_LOD_BIAS = 0.0f;

    // The packed Xenos surface formats CreateBackBuffer seeds the colour section with (X360 immediates).
    const u32 KU_BACK_BUFFER_BUFFER_FORMAT  = 0x18280186u;
    const u32 KU_BACK_BUFFER_TEXTURE_FORMAT = 0x18280106u;

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
}

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

// 0x823F6F78 -- build the back-buffer render target. A 1-section colour+depth target at the front-buffer
// resolution; once built, its post-fx render target shares the down-sample buffer's resolved depth
// surface and uses the engine default render-target state for section 0.
void BrnRendererMemory::CreateBackBuffer(rw::IResourceAllocator* lpAllocator, u32 luWidth, u32 luHeight)
{
    CgsRenderTarget* lpBackBuffer = new CgsRenderTarget();

    // Clear every colour section's in-use flag, then describe section 0 as the active colour surface.
    for (u32 luSection = 0; luSection < 5; ++luSection)
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
