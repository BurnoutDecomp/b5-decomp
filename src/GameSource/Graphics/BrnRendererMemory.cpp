#include "GameSource/Graphics/BrnRendererMemory.h"

#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"  // CgsRenderTarget (+ serialise-side setters)
#include "GameSource/Graphics/BrnAntiAliasTiling.h"         // BrnGraphics::AntiAliasTilingPlan + the two plans
#include "pc/gcm/renderengine/device.h"                     // renderengine::gDisplayWidth/gDisplayHeight (PC bring-up extent)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // postfx::RenderTarget + gpDefaultRenderTargetState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBuffer
#include "GameSource/Resource/BrnResourceAllocator.h"          // BrnResource::Allocators::GetGlobalGraphicsAllocator
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"              // renderengine::PCBringUpClearRenderTargetState
#include "pc/gcm/renderengine/Im2dBlitProgramsPC.h"           // the four authored PC blit program images
#include "pc/gcm/renderengine/VertexDescriptor.h"             // renderengine::VertexDescriptor(+Data)
#include "pc/gcm/renderengine/renderstates.h"                 // renderengine::TextureState
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log::WriteToLog ([qres] lines)
#include <cstdio>    // snprintf (the one-shot bring-up lines)
#include <cstring>   // memcpy (the constant rows -- the row pointer carries no alignment guarantee)

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

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- open one shader-constant
// row and return the write cursor (X360 r3 @0x822768D0). The SHARED declaration-only spelling this
// tree standardised on; the ONE definition is pc/gcm/renderengine/ImmediateModePCLeaf.cpp:729.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// The Xenon immediate-vertex ring intrinsics (defined for PC in
// pc/gcm/renderengine/XenonD3D9Shims.cpp:3646/3775). Declared at file scope exactly as
// BrnSunCorona.cpp, CgsIm2dUntex.cpp and BrnSkidVertex.cpp declare them.
struct D3DDevice;
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType,
                                         u32 luVertexCount, u32 luVertexStreamZeroStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);

namespace
{
    // The packed Xenos surface formats the pool seeds its colour sections with (X360 immediates,
    // lis 0x1828 / ori 0x186 and ori 0x106). POOL-WIDE, not back-buffer-specific: the anti-alias,
    // down-sample, work and back buffers all use the buffer form, and all but the work buffer use the
    // texture form. The two differ in exactly one bit (0x00000080, set in the buffer form), which is
    // what distinguishes "this surface is a render target" from "this surface is sampled".
    const u32 KU_COLOUR_SURFACE_BUFFER_FORMAT  = 0x18280186u;
    const u32 KU_COLOUR_SURFACE_TEXTURE_FORMAT = 0x18280106u;

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
    // format, which is what makes the target sampleable as a shadow map. Also used by the
    // anti-alias, down-sample and particle buffers -- it is the pool's one depth format.
    const u32 KU_DEPTH_SURFACE_FORMAT = 0x2D200196u;

    // --- the fixed extents the pool's smaller targets are seeded with (X360 immediates) -------------
    // Every one of these is a HARD-CODED immediate, not derived from the screen size. At the 1280x720
    // front buffer the 320x180 ones happen to be a quarter of the screen on each axis, but nothing
    // reads mu32ScreenWidth/mu32ScreenHeight, so a different display resolution leaves them alone.
    // (CreateParticleBuffer is the exception: it really does halve the screen members.)
    const u32 KU_WORK_BUFFER_WIDTH            = 320u;   // li 0x140 in CreateWorkBuffer @0x823F7148
    const u32 KU_WORK_BUFFER_HEIGHT           = 180u;   // li 0xB4
    const u32 KU_BLOOM_BUFFER_WIDTH           = 320u;   // li 0x140 in CreateBloomBuffer @0x823F7090
    const u32 KU_BLOOM_BUFFER_HEIGHT          = 180u;   // li 0xB4
    // Kept as their OWN constants rather than aliases of the bloom pair: the depth-of-field helper is
    // a separate compiled function seeded with the same numbers, so a later divergence in one must not
    // silently move the other. See the FLAGGED PAIR note on CreateDepthOfFieldBuffer.
    const u32 KU_DEPTH_OF_FIELD_BUFFER_WIDTH  = 320u;   // li 0x140 in @0x823F7200
    const u32 KU_DEPTH_OF_FIELD_BUFFER_HEIGHT = 180u;   // li 0xB4
    // The env map is the only CUBE target in the pool and the only one that is multisampled at all.
    // Its texture type 3 is renderengine::Texture::E_TYPE_CUBE (the committed enum in
    // pc/gcm/renderengine/texture.h); li r7, 3 @0x823F6CEC.
    const u32 KU_ENV_MAP_FACE_SIZE            = 128u;         // li r8, 0x80 -- width AND height
    const u32 KU_ENV_MAP_TEXTURE_TYPE_CUBE    = 3u;           // li r7, 3
    const s32 KI_ENV_MAP_MULTISAMPLE_FORMAT   = 2;            // li r6, 2 @0x823F6CF0
    // NOT the shadow map's 0x2D200196: the env map carries its own packed depth format, stored to both
    // the buffer and the texture slot (lis 0x1A22 / ori 0x197 @0x823F6D18-0x823F6D20).
    const u32 KU_ENV_MAP_DEPTH_FORMAT         = 0x1A220197u;
    // A BAKED tile count, not a computed one -- li r28, 0x40 @0x823F6CFC. This is the constant that
    // pins CountEDRAMTiles' arithmetic: 64 tiles is 4x the 16 that rule gives for a 1-sample 128x128
    // surface, which is CONSISTENT with multisample format 2 meaning four samples. (Consistent, not
    // proven -- nothing in the image maps format 2 to a sample count; see the note on the helper.)
    const u32 KU_ENV_MAP_DEPTH_BASE_EDRAM_TILES = 64u;
    const u32 KU_SUN_CORONA_BUFFER_EXTENT     = 1u;     // li r10,1 @0x823F7420 -- width, height AND
                                                        // mip count all take that one register

    // --- EDRAM tile arithmetic (CONSOLE FACT; the PC leaf drops all of it) --------------------------
    // The Xenos renders into 10 MB of on-die EDRAM addressed in TILES of 80x16 samples at 4 bytes each
    // = 5120 bytes, i.e. 2048 tiles. A surface's footprint is its extent rounded up to whole tiles, and
    // a target's depth surface is placed immediately AFTER its colour surface -- which is what
    // CreateAntiAliasBuffer, CreateDownSampleBuffer and CreateParticleBuffer compute into
    // mDepthTarget.mu32BaseEDRAM. The compiler inlined the same strength-reduced expression into all
    // three; it is un-inlined back to the one helper below.
    //
    // ⚠️ THE ROW ALIGNMENT IS A PARAMETER, NOT A CONSTANT. The anti-alias and down-sample helpers align
    // rows to 16 (clrrwi r10, r7, 4 @0x823F6C4C / @0x823F6F34) but the particle helper aligns them to
    // 32 (& 0xFFFFFFE0 @0x823F72B8). Hard-coding 16 for all three would silently halve the particle
    // buffer's depth base.
    //
    // NONE OF IT SURVIVES ON PC: PostFxRenderTargetPCLeaf.cpp honours no EDRAM base, tile index,
    // hierarchical-Z or compression-base field ("they describe hardware that does not exist here"), so
    // the number travels through Construct and is then ignored. It is reproduced because it is what the
    // binary computes, not because the D3D9 path needs it.
    const u32 KU_EDRAM_TILE_WIDTH             = 80u;      // li r8, 0x50 + addi r10, r10, 0x4F
    const u32 KU_EDRAM_TILE_HEIGHT            = 16u;      // addi r7, r11, 0xF + clrrwi r10, r7, 4
    const u32 KU_EDRAM_PARTICLE_ROW_ALIGNMENT = 32u;      // & 0xFFFFFFE0, CreateParticleBuffer only
    const u32 KU_EDRAM_BYTES_PER_TILE         = 0x1400u;  // li r8, 0x1400 (5120 == 80 * 16 * 4)
    const u32 KU_EDRAM_BYTES_PER_SAMPLE       = 4u;       // slwi r11, r11, 2 -- a HARD-CODED shift; the
                                                          // image does not derive it from the format

    u32 CountEDRAMTiles(u32 luWidth, u32 luHeight, u32 luRowAlignment)
    {
        const u32 luTiledWidth  = ((luWidth + KU_EDRAM_TILE_WIDTH - 1u) / KU_EDRAM_TILE_WIDTH)
                                  * KU_EDRAM_TILE_WIDTH;
        const u32 luTiledHeight = ((luHeight + luRowAlignment - 1u) / luRowAlignment) * luRowAlignment;
        return luTiledWidth * luTiledHeight * KU_EDRAM_BYTES_PER_SAMPLE / KU_EDRAM_BYTES_PER_TILE;
    }

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
    lpShadowMap->ClearColourTargetInUse();

    // FLAG PC bring-up: the COMBINED extent + one section (see above). The console's
    // CreateShadowmapBuffer @0x823F6D98 sets (1280, 1920/3) and three sections.
    lpShadowMap->SetDimensions(KU_SHADOW_MAP_WIDTH, KU_SHADOW_MAP_COMBINED_HEIGHT);
    lpShadowMap->SetNumSections(1);

    lpShadowMap->SetNumMipMaps(1);
    lpShadowMap->SetMultisampleFormat(0);
    lpShadowMap->SetUseDepthStencilAsTexture(true);

    lpShadowMap->SetDepthTargetInUse(true);
    lpShadowMap->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpShadowMap->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);
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
    lpShadowMap->ClearColourTargetInUse();

    lpShadowMap->SetDimensions(luWidth, luHeight);
    lpShadowMap->SetNumMipMaps(1);
    lpShadowMap->SetMultisampleFormat(0);
    // One section per cascade -- the combined-viewport layout ShadowMapRenderManager renders into.
    lpShadowMap->SetNumSections(static_cast<u8>(KU_SHADOW_MAP_NUM_CASCADES));
    lpShadowMap->SetUseDepthStencilAsTexture(true);

    lpShadowMap->SetDepthTargetInUse(true);
    lpShadowMap->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpShadowMap->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);
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
    lpBackBuffer->ClearColourTargetInUse();

    lpBackBuffer->SetDimensions(luWidth, luHeight);
    lpBackBuffer->SetNumMipMaps(1);
    lpBackBuffer->SetMultisampleFormat(0);
    // NOTE: the X360 CreateBackBuffer @0x823F6F78 performs NO store to CgsRenderTarget +0x14
    // (mu8NumSections); the back buffer keeps the constructor's section-count default. The earlier
    // draft's SetNumSections(1) was added behavior the binary lacks -- removed.
    lpBackBuffer->SetUseDepthStencilAsTexture(false);

    // The back buffer is the ONE pool target that is the device's own surface, so it is the only
    // caller that sets use-device; the filter mode and in-use flag come with it (the X360 stores
    // all three inline here). See the split-setter banner in CgsRenderTarget.h.
    lpBackBuffer->SetColourFilterMode(0, 1);
    lpBackBuffer->SetColourTargetInUse(0, true);
    lpBackBuffer->SetColourTargetUseDevice(0, true);
    lpBackBuffer->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBackBuffer->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
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

// [PC bring-up] Create the post-fx spine's two render targets. NOT a console function -- the console
// builds the whole pool in Construct @0x823FCA38, which is still gated out (see the
// BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner and the EnsurePostFxSceneTargets banner in
// BrnRendererModule.cpp for the two remaining blockers). This exists only so the spine can use the REAL
// creators without un-gating Construct, and it does nothing they do not do.
//
// ⚠️ MUST RUN AFTER PCBringUpCreateShadowMapBufferOnly, which nulls every pool slot. The caller
// enforces that; this asserts it, because getting it backwards would silently drop the shadow target.
//
// DELETE WITH THE BRING-UP, together with PCBringUpCreateShadowMapBufferOnly.
void BrnRendererMemory::PCBringUpCreatePostFxSceneTargets(rw::IResourceAllocator* lpAllocator,
                                                          bool lbEnableMSAA)
{
    CGS_ASSERT(mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] != nullptr,
               "GetShadowMapBuffer(0) != NULL -- slot-nulling bring-up must run first");

    // ⚠️ SEED THE SCREEN EXTENT FIRST. mu32ScreenWidth / mu32ScreenHeight are written in exactly ONE
    // place -- Construct @0x823FCA38, from renderengine::DeviceParameters::muFrontBufferWidth/Height --
    // and Construct is the function that is gated out. So on this build they are still ZERO here, and
    // the anti-alias and down-sample buffers both size themselves from them. The first boot of this
    // wave duly logged "[postfx-rt] target 0x0 ... colourTexture=NULL": a 0x0 CreateTexture fails, the
    // scene target owns no surface, and NOTHING reports an error. That is the placeholder-zero failure
    // mode this project keeps paying for, caught this time only because the log line prints the extent.
    //
    // The PC front buffer's real extent is renderengine::gDisplayWidth/gDisplayHeight -- the values
    // device.cpp hands D3DPRESENT_PARAMETERS::BackBufferWidth/Height, i.e. the actual swap chain.
    mu32ScreenWidth  = static_cast<u32>(renderengine::gDisplayWidth);
    mu32ScreenHeight = static_cast<u32>(renderengine::gDisplayHeight);

    // And REFUSE to build a degenerate target rather than logging one. A zero extent here means the
    // device is not up yet; the caller is value-latched, so returning leaves it to retry next frame.
    if (mu32ScreenWidth == 0u || mu32ScreenHeight == 0u)
    {
        return;
    }

    // THE ORDER HERE IS THE BRING-UP'S, NOT Construct's, and the comment that used to claim
    // otherwise was wrong (verify round, 2026-08-16). BrnRendererMemory::Construct @0x823FCA38 calls
    // CreateAntiAliasBuffer FIRST (`bl BrnRendererMemory__CreateAntiAliasBuffer` @0x823FCAB4, with
    // lbEnableMSAA already in r5 @0x823FCAAC) and CreateDownSampleBuffer SECOND (@0x823FCAC0) -- the
    // order the gated Construct in this file reproduces at :257-258. The inversion is harmless
    // (neither creator reads a slot the other writes; only CreateBackBuffer has an ordering
    // requirement and it runs after both either way), but it is a DEVIATION and is recorded as one.
    CreateDownSampleBuffer(lpAllocator);

    // THE ANTI-ALIAS (SCENE) BUFFER, at the console's own MSAA setting (anti-aliasing wave,
    // 2026-08-16). This call used to be `CreateAntiAliasBuffer(lpAllocator, false)` under a banner
    // reading "MSAA OFF. The console reads this from its display-mode setting". BOTH HALVES OF THAT
    // WERE WRONG, and the correction is the whole point of this wave:
    //
    //   * THE CONSOLE DOES NOT READ A DISPLAY-MODE SETTING. BrnRendererModule::Construct @0x8240A778
    //     stores a literal 1 into mbMultisampledBackbuffer (`li r28, 1` @0x8240A7B4 /
    //     `stb r28, 0(r29)` @0x8240A7C4, r29 = this+0xC400) and forwards that same byte into this
    //     pool's Construct as lbEnableMSAA (`lbz r11, 0(r29)` @0x8240A8BC -> sp+0x97 == `arg_97`).
    //     There is no branch and no settings read anywhere on that path. The shipped X360 build is
    //     MSAA ON, format 1 (Xenos 2x), always.
    //   * D3D9 DOES HAVE THE MACHINERY THAT MATTERS. What it lacks is PREDICATED TILING, which is an
    //     EDRAM budgeting device, not anti-aliasing: the two 1280x384/1280x336 rectangles of
    //     KMSAA_TILING_PLAN exist because a 2-sample 1280x720 colour+depth pair does not fit in the
    //     Xenos' 10 MB of EDRAM (BrnAntiAliasTiling.h). On PC the surface is whole, the two
    //     rectangles partition it exactly, and every per-tile call degrades correctly: BeginTiling
    //     clears both rects, SetPredication/EndTiling are honest no-ops, and each per-tile
    //     D3DDevice_Resolve is a StretchRect over its own band -- which, from a MULTISAMPLED source,
    //     IS D3D9's MSAA resolve.
    //
    // So the flag is threaded from the module rather than chosen here, and the PC sample COUNT (a
    // knob, defaulting to the console's own format) is decided one layer down, in the leaf's
    // RenderTarget::Initialize -- see renderengine::gAntiAliasing in Xbox2SurfaceShims.h.
    CreateAntiAliasBuffer(lpAllocator, lbEnableMSAA);

    // ---- the three targets the post-fx COMPOSITE reads or writes (gate-flip wave, 2026-08-15) ----
    // BrnPostFx::Render @0x8240A468 dereferences the BLOOM and DEPTH-OF-FIELD slots unconditionally
    // (`GetBloomBuffer()->GetRenderTarget()->GetTexture(0)` for SamplerBloom / SamplerDof -- the
    // X360 asm at 0x8240A560-0x8240A5E8 has no null test, and that is FAITHFUL: the console pool
    // cannot fail), and it composites INTO the BACK_BUFFER slot (`lpDestinationRenderTarget->
    // Begin(0)`), whose colour mode is USE_DEVICE_FOR_WRITE -- the swap chain, through
    // gpDefaultRenderTargetState (installed by Device::Start on this build too, see
    // PCInstallDefaultRenderTargetState). Same three console creators, same relative order as
    // Construct @0x823FCA38 (bloom, depth-of-field, ..., back buffer LAST -- CreateBackBuffer asserts
    // the down-sample buffer exists because it borrows its section-4 depth state). The bloom target's
    // texture MUST exist even while bloom is off: permutation 0's pixel program samples SamplerBloom
    // unconditionally and is neutral through BloomColour == 0, not through an empty binding.
    CreateBloomBuffer(lpAllocator);
    CreateDepthOfFieldBuffer(lpAllocator);
    // The WORK buffer (CreateWorkBuffer @0x823F7148, 320 wide): the console Construct's sixth creator. It
    // is the ping-pong target of every bloom / depth-of-field pass -- BrnPostFx::PrepareDownSampleBuffers
    // hands GetWorkBuffer()->GetRenderTarget() to BrnPostFxBloom::Render and DepthOfField::
    // DownSampleAndGaussianBlur -- so it was harmless to omit only while every effect bit was clear.
    // The first bloom-lit boot (rung 5, 2026-08-15) crashed at BrnPostFx::Render+0xE1 reading
    // null->+0x160 (== CgsRenderTarget::GetRenderTarget on a null pool slot) the frame E_FX_BLOOM
    // came on. Same allocator, same order as the console.
    CreateWorkBuffer(lpAllocator);
    CreateBackBuffer(lpAllocator, mu32ScreenWidth, mu32ScreenHeight);

    // ⚠️ FLAG PC bring-up INITIALISATION -- give both targets DEFINED contents before the first
    // frame renders into one. NOT a console behaviour: the Xenos clears its EDRAM scene surface
    // from the TILING PASS at the TOP of every frame (first frame included), whereas the PC
    // bracket's only clear is the resolve's, at the BOTTOM. Both surfaces above are
    // D3DPOOL_DEFAULT D3D9 objects, which D3D9 leaves UNDEFINED at creation, so without this the
    // first bracket frame renders the world into undefined colour over undefined depth -- and
    // that frame is copied and presented. Undefined depth can reject the whole world, which
    // looks like the bracket failing rather than like one bad frame.
    //
    // The DOWN-SAMPLE buffer is cleared as well as the anti-alias buffer because it is what the
    // present blit reads: its colour is rewritten every frame by the resolve's copy, but only
    // over the rectangle that copy is handed, and its depth is never written at all.
    //
    // DELETE WITH THE BRING-UP, when a frame-top clear exists again -- see the banner on
    // renderengine::PCBringUpClearRenderTargetState in pc/gcm/renderengine/ShadowPassPCLeaf.h,
    // which also carries the attestation for every value the clear writes.
    CgsRenderTarget* const lpDownSampleBuffer = GetDownSampleBuffer();
    CgsRenderTarget* const lpAntiAliasBuffer  = GetAntiAliasBuffer();
    rw::graphics::postfx::RenderTarget* const lpDownSampleTarget =
        (lpDownSampleBuffer != nullptr) ? lpDownSampleBuffer->GetRenderTarget() : nullptr;
    rw::graphics::postfx::RenderTarget* const lpAntiAliasTarget =
        (lpAntiAliasBuffer != nullptr) ? lpAntiAliasBuffer->GetRenderTarget() : nullptr;
    if (lpDownSampleTarget != nullptr)
    {
        renderengine::PCBringUpClearRenderTargetState(
            lpDownSampleTarget->GetSectionRenderTargetState(0));
    }
    if (lpAntiAliasTarget != nullptr)
    {
        renderengine::PCBringUpClearRenderTargetState(
            lpAntiAliasTarget->GetSectionRenderTargetState(0));
    }

    // The BLOOM and DEPTH-OF-FIELD targets are SAMPLED by the composite every frame, and nothing
    // writes them while their effects are off -- so their D3DPOOL_DEFAULT contents would otherwise
    // be undefined FOREVER, not just on the first frame. Bloom's neutral is `tap * BloomColour(0)`;
    // an undefined tap that happens to hold NaN bits makes that NaN, and NaN survives the screen
    // blend into the presented pixel. Cleared once, same helper. (The back buffer is the swap chain
    // -- FrameBegin clears it every frame -- so it is not on this list.)
    CgsRenderTarget* const lpBloomBuffer        = GetBloomBuffer();
    CgsRenderTarget* const lpDepthOfFieldBuffer = GetDepthOfFieldBuffer();
    if (lpBloomBuffer != nullptr && lpBloomBuffer->GetRenderTarget() != nullptr)
    {
        renderengine::PCBringUpClearRenderTargetState(
            lpBloomBuffer->GetRenderTarget()->GetSectionRenderTargetState(0));
    }
    if (lpDepthOfFieldBuffer != nullptr && lpDepthOfFieldBuffer->GetRenderTarget() != nullptr)
    {
        renderengine::PCBringUpClearRenderTargetState(
            lpDepthOfFieldBuffer->GetRenderTarget()->GetSectionRenderTargetState(0));
    }
}


// [PC bring-up] Create the ENVIRONMENT-MAP render target. NOT a console function -- the console
// builds it inside Construct @0x823FCA38 (`bl BrnRendererMemory__CreateEnvmapBuffer`), which is
// still gated out for the two reasons the BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner names.
// This is the same one-slot bring-up entry point PCBringUpCreatePostFxSceneTargets is, and it does
// nothing CreateEnvmapBuffer does not do.
//
// THREE ORDERING FACTS, all of them checked rather than assumed:
//  (1) It must run AFTER PCBringUpCreateShadowMapBufferOnly, which NULLS EVERY POOL SLOT. The assert
//      below is that precondition; the caller (BrnRendererModule::EnsureEnvMapTarget) enforces it by
//      refusing until the shadow target exists.
//  (2) It must NOT run twice. CreateEnvmapBuffer `new`s a CgsRenderTarget and publishes it into the
//      slot BEFORE describing it, so a second call would leak the first target AND hand the renderer
//      a different object than the one sampler 13 was bound to. The early return is that guard.
//  (3) It does NOT disturb the shadow-map latch in PostFxRenderTargetPCLeaf.cpp. That leaf picks the
//      COMPARE depth ladder only for a target whose colour mode is NONE (its own banner, ~:225: "the
//      SHADOW MAP is the only pool target with colour mode NONE that still resolves its depth to a
//      texture"). CreateEnvmapBuffer marks colour section 0 in use, so this target's colour mode is
//      CREATE and it takes the RAW ladder, exactly like the down-sample and particle buffers.
//
// DELETE WITH THE BRING-UP, together with its two siblings.
void BrnRendererMemory::PCBringUpCreateEnvMapBuffer(rw::IResourceAllocator* lpAllocator)
{
    CGS_ASSERT(mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] != nullptr,
               "GetShadowMapBuffer(0) != NULL -- slot-nulling bring-up must run first");

    if (mapRenderTarget[E_RENDER_TARGET_ENV_MAP] != nullptr)
    {
        return;
    }

    CreateEnvmapBuffer(lpAllocator);
}

// [PC bring-up] Create the SUN-CORONA render target (coronas step 2). NOT a console function --
// the console builds it inside Construct @0x823FCA38 (`bl BrnRendererMemory__CreateSunCoronaBuffer`
// @0x823FCB0C), which is still gated out for the two reasons the
// BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner names. This is the same one-slot bring-up entry
// point PCBringUpCreateEnvMapBuffer is, and it does nothing CreateSunCoronaBuffer does not do.
//
// It is NOT sized from the screen and does not care about the device's extent: the target is 1x1
// (CreateSunCoronaBuffer's single `li r10, 1` feeds width, height AND the mip count), so unlike
// PCBringUpCreatePostFxSceneTargets it has no mu32ScreenWidth precondition.
//
// DELETE WITH THE BRING-UP, together with its three siblings.
void BrnRendererMemory::PCBringUpCreateSunCoronaBuffer(rw::IResourceAllocator* lpAllocator)
{
    CGS_ASSERT(mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] != nullptr,
               "GetShadowMapBuffer(0) != NULL -- slot-nulling bring-up must run first");

    if (mapRenderTarget[E_RENDER_TARGET_SUN_CORONA] != nullptr)
    {
        return;
    }

    CreateSunCoronaBuffer(lpAllocator);
}

// =================================================================================================
// THE REST OF THE RENDER-TARGET POOL (post-fx spine wave, 2026-08-13).
//
// The eight sibling Create*Buffer helpers Construct @0x823FCA38 calls. Until this wave the tree had
// only CreateShadowmapBuffer and CreateBackBuffer, which is why Construct could not link and the
// build ran on PCBringUpCreateShadowMapBufferOnly instead; these eight were the bulk of the fourteen
// unresolved externals named in the BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner at the top of this
// file. Their ledger status said "reviewed"; none of them existed.
//
// Reconstructed from the X360 assembly, one helper per address:
//   CreateAntiAliasBuffer     @ 0x823F6B40   scene colour+depth, screen res, the tiling plans
//   CreateDownSampleBuffer    @ 0x823F6E60
//   CreateWorkBuffer          @ 0x823F7148
//   CreateEnvmapBuffer        @ 0x823F6C88
//   CreateBloomBuffer         @ 0x823F7090
//   CreateDepthOfFieldBuffer  @ 0x823F7200
//   CreateParticleBuffer      @ 0x823F72B8
//   CreateSunCoronaBuffer     @ 0x823F73C8
//
// TWO THINGS TO KNOW BEFORE EDITING ANY OF THEM.
//
// (1) THE mbUseDevice BIT IS THE WHOLE GAME. Every helper here marks colour section 0 in use with a
//     SINGLE store and leaves mbUseDevice clear. CgsRenderTarget::Construct turns mbUseDevice into
//     eRenderTarget_USE_DEVICE_FOR_WRITE instead of eRenderTarget_CREATE, and
//     PostFxRenderTargetPCLeaf.cpp creates a colour TEXTURE only for eRenderTarget_CREATE. So routing
//     any of these through a fused "set in use" setter that also sets use-device would make the target
//     render into the device back buffer instead of owning a surface -- i.e. it would look like the
//     post-fx chain doing nothing, with no error anywhere. That is why CgsRenderTarget's granular
//     DWARF-named setters replaced the old fused pair in this wave.
//
// (2) NO CONSOLE OFFSET, STRIDE OR GUEST SIZEOF APPEARS BELOW. The X360 asks operator new for 0x110
//     bytes (the guest sizeof(CgsRenderTarget)) and pokes raw displacements; here every field is
//     reached through a named setter and the host compiler sizes the object. The EDRAM tile numbers
//     are the one exception and they are not layout -- see the CountEDRAMTiles banner above.
// =================================================================================================

// 0x823F6B40 -- build the ANTI-ALIAS buffer: the off-screen colour+depth target the whole world pass
// renders into, at the front-buffer resolution, in pool slot 0 (E_RENDER_TARGET_ANTI_ALIAS). Two things
// make it unlike its siblings: it is described from one of two read-only predicated-tiling records in
// .rdata, chosen by lbEnableMSAA, and it is one of only two helpers that computes where the depth
// surface starts in EDRAM (see CountEDRAMTiles + the tiling-plan block above).
//
// The pool slot is written BEFORE the target is described (stw r3, 0(r30) at 0x823F6B7C, immediately
// after the constructor returns) -- the same order CreateShadowmapBuffer uses, and the opposite of
// CreateDownSampleBuffer / CreateWorkBuffer, which store their slot after Construct() returns.
void BrnRendererMemory::CreateAntiAliasBuffer(rw::IResourceAllocator* lpAllocator, bool lbEnableMSAA)
{
    // The X360 asks operator new for 0x110 bytes (li r3, 0x110 @0x823F6B50) -- that is the GUEST
    // sizeof(CgsRenderTarget). On the x64 host the compiler sizes the object; no size is written here.
    CgsRenderTarget* lpAntiAlias = new CgsRenderTarget();
    mapRenderTarget[E_RENDER_TARGET_ANTI_ALIAS] = lpAntiAlias;

    // Clear every colour section first (the X360 four-iteration loop lives inside this
    // one no-argument method -- see the split-setter banner in CgsRenderTarget.h).
    lpAntiAlias->ClearColourTargetInUse();

    // The scene buffer is the full front-buffer resolution (lwz 0x5C / 0x60 of this).
    lpAntiAlias->SetDimensions(mu32ScreenWidth, mu32ScreenHeight);
    lpAntiAlias->SetNumMipMaps(1);

    // Colour section 0 is in use. The asm sets ONLY the in-use byte (stb r11, 0x1C(r3) @0x823F6BBC):
    // it writes neither the use-device byte (+0x1E) nor the filter mode (+0x18, which keeps the
    // constructor's 1). Leaving use-device clear is load-bearing -- it is what makes
    // CgsRenderTarget::Construct derive colour mode CREATE (an off-screen surface) rather than
    // USE_DEVICE_FOR_WRITE. CreateBackBuffer is the only helper in the pool that sets it.
    lpAntiAlias->SetColourTargetInUse(0, true);
    lpAntiAlias->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpAntiAlias->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpAntiAlias->SetColourTargetBaseEDRAM(0, 0);   // the colour surface sits at EDRAM tile 0

    // It carries its own depth surface, and it is NOT flagged sampleable: the scene pass reads depth
    // back through the down-sample buffer, which is the target that sets that flag.
    lpAntiAlias->SetUseDepthStencilAsTexture(false);
    lpAntiAlias->SetDepthTargetInUse(true);
    lpAntiAlias->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpAntiAlias->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);

    // Pick the tiling plan (true -> the 2-tile multisampled one) and take the multisample format from
    // it. NOTE there is no store to +0x14: the target keeps the constructor's ONE section. Predicated
    // tiling is not sections -- the plan's rectangles are successive GPU passes over the same surface,
    // not bands of a taller one.
    const BrnGraphics::AntiAliasTilingPlan& lrPlan = lbEnableMSAA ? BrnGraphics::KMSAA_TILING_PLAN : BrnGraphics::KNO_MSAA_TILING_PLAN;
    lpAntiAlias->SetMultisampleFormat(lrPlan.mn32MultiSampleFormat);

    // CONSOLE EDRAM FACT (dropped by the PC leaf -- see the CountEDRAMTiles banner): the depth surface
    // is placed immediately after the colour surface, so its base is the colour surface's tile count.
    // The footprint is ONE rectangle of the plan, because predicated tiling holds a single tile in
    // EDRAM at a time. The multisample format is log2(sample count), and sample expansion costs EDRAM
    // on one axis per doubling: format >= 1 doubles the height, format == 2 doubles the width too
    // (cmpwi 1 / blt @0x823F6C20 and cmpwi 2 / bne @0x823F6C2C).
    u32 luEDRAMWidth  = lrPlan.maTile[0].mu32Right;
    u32 luEDRAMHeight = lrPlan.maTile[0].mu32Bottom;
    if (lrPlan.mn32MultiSampleFormat >= 1)
    {
        luEDRAMHeight *= 2;
    }
    if (lrPlan.mn32MultiSampleFormat == 2)
    {
        luEDRAMWidth *= 2;
    }
    lpAntiAlias->SetDepthTargetBaseEDRAM(CountEDRAMTiles(luEDRAMWidth, luEDRAMHeight, KU_EDRAM_TILE_HEIGHT));

    // The asm does NOT touch the depth record's tile index (+0xF0), unlike CreateShadowmapBuffer --
    // it keeps the constructor's -1 "unassigned" sentinel. No setter call is added for it.

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual: lwz r9,0(r3) /
    // lwz r7,0(r9) / bctrl @0x823F6C38-0x823F6C7C).
    lpAntiAlias->Construct(lpAllocator);
}

// 0x823F6E60 -- build the DOWN-SAMPLE buffer: a full-resolution, single-sample colour+depth target in
// pool slot 4 (E_RENDER_TARGET_DOWN_SAMPLE). It is the target the multisampled scene buffer resolves
// into, and the only one in the pool whose depth/stencil is flagged sampleable-as-a-texture, which is
// what lets the post-fx chain read scene depth. CreateBackBuffer @0x823F6F78 asserts this slot is
// non-null and then shares this target's depth-stencil section.
//
// The pool slot is written LAST, after Construct() returns (stw r31, 0x10(r30) @0x823F6F6C).
void BrnRendererMemory::CreateDownSampleBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpDownSample = new CgsRenderTarget();

    // Clear every colour section first (the X360 four-iteration loop lives inside this
    // one no-argument method -- see the split-setter banner in CgsRenderTarget.h).
    lpDownSample->ClearColourTargetInUse();

    lpDownSample->SetDimensions(mu32ScreenWidth, mu32ScreenHeight);
    lpDownSample->SetNumMipMaps(1);
    // The resolve destination is never multisampled.
    lpDownSample->SetMultisampleFormat(0);

    // Colour section 0 in use, with its filter mode written explicitly (stb r11,0x1C @0x823F6EEC and
    // stw r11,0x18 @0x823F6EF0, both r11 == 1). The filter-mode store is REDUNDANT against the
    // surface constructor, which already seeds mu32FilterMode = 1 (CgsRenderTarget.cpp) -- it is
    // reproduced because the image performs it, not because it changes the result. As in
    // CreateAntiAliasBuffer there is NO use-device store, so the colour mode comes out CREATE.
    lpDownSample->SetColourTargetInUse(0, true);
    lpDownSample->SetColourFilterMode(0, 1);
    lpDownSample->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpDownSample->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpDownSample->SetColourTargetBaseEDRAM(0, 0);   // the colour surface sits at EDRAM tile 0

    // This is the target whose depth the post-fx chain samples and the back buffer borrows.
    lpDownSample->SetUseDepthStencilAsTexture(true);
    lpDownSample->SetDepthTargetInUse(true);
    lpDownSample->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpDownSample->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);

    // CONSOLE EDRAM FACT (dropped by the PC leaf): the depth surface starts immediately after the
    // colour surface. Unlike the anti-alias buffer there is no tiling plan and no sample expansion --
    // the footprint is the whole screen (lwz 0x5C / 0x60 of this, re-read at 0x823F6F04-0x823F6F18).
    lpDownSample->SetDepthTargetBaseEDRAM(CountEDRAMTiles(mu32ScreenWidth, mu32ScreenHeight, KU_EDRAM_TILE_HEIGHT));

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpDownSample->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_DOWN_SAMPLE] = lpDownSample;
}

// 0x823F7148 -- build the WORK buffer: the small colour-only scratch target the post-fx chain
// ping-pongs through, in pool slot 8 (E_RENDER_TARGET_WORK). Fixed 320x180 -- a quarter of 1280x720 on
// each axis, a sixteenth of the area -- so it is NOT sized from the screen like its siblings.
//
// It is the simplest helper in the pool: no depth surface, no multisampling, no tiling plan and no
// EDRAM arithmetic at all. The pool slot is written LAST, after Construct() returns
// (stw r31, 0x20(r29) @0x823F71F4).
void BrnRendererMemory::CreateWorkBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpWorkBuffer = new CgsRenderTarget();

    // Clear every colour section first (the X360 four-iteration loop lives inside this
    // one no-argument method -- see the split-setter banner in CgsRenderTarget.h).
    lpWorkBuffer->ClearColourTargetInUse();

    lpWorkBuffer->SetDimensions(KU_WORK_BUFFER_WIDTH, KU_WORK_BUFFER_HEIGHT);
    lpWorkBuffer->SetNumMipMaps(1);
    lpWorkBuffer->SetMultisampleFormat(0);
    lpWorkBuffer->SetUseDepthStencilAsTexture(false);

    // Colour section 0 in use -- again ONLY the in-use byte (stb r10, 0x1C(r31) @0x823F71CC): no
    // use-device store, so the colour mode comes out CREATE, and the filter mode keeps the surface
    // constructor's 1.
    lpWorkBuffer->SetColourTargetInUse(0, true);
    // BOTH formats get the BUFFER-form immediate here. The X360 keeps 0x18280186 in r11 and stores it
    // twice (stw r11, 0x20(r31) then stw r11, 0x24(r31) @0x823F71DC-0x823F71E0) -- so unlike every
    // other colour target in the pool the work buffer's texture format is NOT the 0x18280106 form.
    // That is the binary, not a copy/paste slip in this reconstruction.
    lpWorkBuffer->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpWorkBuffer->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpWorkBuffer->SetColourTargetBaseEDRAM(0, 0);

    // Colour only: the depth record is switched off (stb r30, 0xDC(r31) @0x823F71D0), so
    // CgsRenderTarget::Construct derives depth mode NONE. The asm makes NO store to the depth base
    // EDRAM (+0xE8) or the depth tile index (+0xF0), and none is added here -- the depth record's
    // base-EDRAM field is left exactly as the surface constructor leaves it (which is: untouched,
    // per the ctor's own note in CgsRenderTarget.cpp). It is dead because the depth mode is NONE.
    lpWorkBuffer->SetDepthTargetInUse(false);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpWorkBuffer->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_WORK] = lpWorkBuffer;
}

// 0x823F6C88 -- build the ENVIRONMENT-MAP buffer: a 128x128 CUBE colour target with its own
// multisampled depth surface, in pool slot 3 (E_RENDER_TARGET_ENV_MAP). This is the odd one out of the
// pool in four separate ways, all of them real:
//
//  1. IT IS A CUBE. Colour section 0's texture type is set to 3 (li r7, 3 -> the colour record's
//     mu32TextureType @0x823F6D40), which is renderengine::Texture::E_TYPE_CUBE. Every other pool
//     target leaves the surface constructor's E_TYPE_2D. 128x128 is therefore the FACE size, rendered
//     one face at a time -- which is why BrnRendererModule::BeginRenderEnvironmentMapFace exists and
//     why CgsRenderTarget::SetRenderTargetStateInvertDepth (its inverted-depth viewport) is only
//     called from there.
//
//  2. IT IS THE ONLY MULTISAMPLED TARGET IN THE POOL. mn32MultiSampleFormat = 2 (li r6, 2). That the
//     2 means FOUR samples is not an assumption -- it falls out of (4) below.
//
//  3. ITS DEPTH FORMAT IS ITS OWN. 0x1A220197 goes into both the depth buffer and the depth texture
//     slot -- a different word from the 0x2D200196 the shadow map and the particle buffer share. The
//     depth is sampleable (mbUseDepthStencilAsTexture = 1).
//
//  4. ITS DEPTH EDRAM BASE IS A BAKED TILE COUNT: 64 (li r28, 0x40). Run
//     CreateParticleBuffer's recovered rule on this surface -- alignUp(128,80) * alignUp(128,32) * 4 /
//     5120 = 160 * 128 * 4 / 5120 = 16 tiles -- and 64 is exactly 16 x 4. So the depth surface starts
//     immediately after a FOUR-SAMPLE 128x128 colour surface, which independently confirms both the
//     tile arithmetic and that multisample format 2 == 4 samples. Nothing is guessed here: the two
//     helpers cross-check each other.
//
// STORE ORDER also differs: this helper publishes the pool slot FIRST (stw r3, 0xC(r31) @0x823F6CC0,
// before the colour-clear loop) and then re-reads the slot before every single subsequent store, the
// CreateShadowmapBuffer ordering rather than the store-last ordering of its four post-fx siblings. The
// re-read itself is a compiler aliasing artifact, not source structure, so it is folded into one local
// below; the load-bearing fact -- the slot is live before the target is described -- is preserved.
void BrnRendererMemory::CreateEnvmapBuffer(rw::IResourceAllocator* lpAllocator)
{
    // Published before the description (see the store-order note above).
    mapRenderTarget[E_RENDER_TARGET_ENV_MAP] = new CgsRenderTarget();
    CgsRenderTarget* lpEnvMap = mapRenderTarget[E_RENDER_TARGET_ENV_MAP];

    lpEnvMap->ClearColourTargetInUse();

    // 128x128 is the CUBE FACE size; the section count stays at the constructor's 1.
    lpEnvMap->SetDimensions(KU_ENV_MAP_FACE_SIZE, KU_ENV_MAP_FACE_SIZE);
    lpEnvMap->SetNumMipMaps(1);
    lpEnvMap->SetMultisampleFormat(KI_ENV_MAP_MULTISAMPLE_FORMAT);
    lpEnvMap->SetUseDepthStencilAsTexture(true);

    // Colour section 0: in use only (no filter-mode store, mbUseDevice left cleared -- see
    // CreateBloomBuffer's USE_DEVICE_FOR_WRITE note), and typed as a cube.
    lpEnvMap->SetColourTargetInUse(0, true);
    lpEnvMap->SetTextureType(0, renderengine::Texture::E_TYPE_CUBE);
    lpEnvMap->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpEnvMap->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpEnvMap->SetColourTargetBaseEDRAM(0, 0);

    // The depth/stencil surface sits immediately after the 4-sample colour surface: 64 EDRAM tiles in.
    // The tile INDEX is not set (the constructor's -1 sentinel stands), unlike CreateShadowmapBuffer.
    lpEnvMap->SetDepthTargetInUse(true);
    lpEnvMap->SetDepthTargetBufferFormat(KU_ENV_MAP_DEPTH_FORMAT);
    lpEnvMap->SetDepthTargetTextureFormat(KU_ENV_MAP_DEPTH_FORMAT);
    lpEnvMap->SetDepthTargetBaseEDRAM(KU_ENV_MAP_DEPTH_BASE_EDRAM_TILES);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual). Note this
    // helper makes NO store to the pool slot afterwards -- it was published up front.
    lpEnvMap->Construct(lpAllocator);
}

// 0x823F7090 -- build the BLOOM accumulation buffer: a fixed 320x180 colour-only target in pool
// slot 6 (E_RENDER_TARGET_BLOOM). No depth surface, no MSAA, one mip, and the section count left at
// the constructor's 1 (the X360 makes no store to mu8NumSections, exactly like CreateBackBuffer).
//
// THE DIMENSIONS ARE HARD-CODED IMMEDIATES, NOT DERIVED. `li r11, 0x140` / `li r9, 0xB4`
// @0x823F70E8/0x823F70F4 -- 320x180. At the 1280x720 front buffer Construct passes they happen to be
// a quarter of the screen in each axis, but nothing here reads mu32ScreenWidth/mu32ScreenHeight, so a
// different display resolution leaves this buffer at 320x180. (CreateParticleBuffer, by contrast,
// really does derive its size from those members.)
//
// The colour section is marked in use with a SINGLE store (stb 1 -> record 0 +0x04 @0x823F7114): the
// filter mode keeps the surface constructor's 1 and mbUseDevice stays CLEARED by the loop above. That
// is not cosmetic -- CgsRenderTarget::Construct turns mbUseDevice into eRenderTarget_USE_DEVICE_FOR_WRITE
// instead of eRenderTarget_CREATE, and PostFxRenderTargetPCLeaf.cpp:584 creates a colour texture ONLY
// for eRenderTarget_CREATE. Routing this through CreateBackBuffer's composite SetColourTargetInUse
// would therefore make the bloom buffer render into the device back buffer instead of its own surface.
//
// Unlike the back buffer, the texture-format slot gets the BUFFER format word (0x18280186 is stored to
// both +0x20 and +0x24), not the 0x18280106 variant.
void BrnRendererMemory::CreateBloomBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpBloom = new CgsRenderTarget();

    // Clear every colour section first (the X360 four-iteration loop lives inside this
    // one no-argument method -- see the split-setter banner in CgsRenderTarget.h).
    lpBloom->ClearColourTargetInUse();

    lpBloom->SetDimensions(KU_BLOOM_BUFFER_WIDTH, KU_BLOOM_BUFFER_HEIGHT);
    lpBloom->SetNumMipMaps(1);
    lpBloom->SetMultisampleFormat(0);
    lpBloom->SetUseDepthStencilAsTexture(false);

    // Colour section 0: in use only -- no filter-mode store, no use-device store (see the note above).
    lpBloom->SetColourTargetInUse(0, true);
    lpBloom->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBloom->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBloom->SetColourTargetBaseEDRAM(0, 0);

    // Colour-only: the depth/stencil record is explicitly switched off (stb 0, 0xDC @0x823F7118).
    lpBloom->SetDepthTargetInUse(false);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpBloom->Construct(lpAllocator);

    // The pool slot is written LAST, after Construct returns (stw r31, 0x18(r29) @0x823F713C) -- the
    // CreateBackBuffer ordering, not CreateShadowmapBuffer's/CreateEnvmapBuffer's store-first ordering.
    mapRenderTarget[E_RENDER_TARGET_BLOOM] = lpBloom;
}

// 0x823F7200 -- build the DEPTH-OF-FIELD buffer: pool slot 7 (E_RENDER_TARGET_DEPTH_OF_FIELD).
//
// FLAGGED PAIR: this function is INSTRUCTION-FOR-INSTRUCTION identical to CreateBloomBuffer
// @0x823F7090 except for one displacement -- the pool slot it publishes (0x1C = slot 7 here,
// 0x18 = slot 6 there). Same 320x180 immediates, same single in-use byte, same 0x18280186 in both
// format slots, same MSAA 0, same depth-off, same store ordering. The two listings are genuinely two
// distinct compiled functions and not a duplicated dossier paste: if the DoF listing were a paste of
// the bloom one, its final store would read 0x18(r29), and it reads 0x1C(r29). So the difference
// between the bloom and depth-of-field buffers on this build is the POOL SLOT alone -- the original
// source had two separate helpers seeded with the same numbers. The dimensions are kept as two
// separate named constants (one per helper's own immediates) so that a later divergence in one does
// not silently move the other.
//
// Everything else reads exactly as CreateBloomBuffer: hard-coded 320x180 (NOT screen-derived), colour
// section 0 marked in use with a single store (filter mode and mbUseDevice untouched -- see the
// USE_DEVICE_FOR_WRITE note on CreateBloomBuffer), no depth surface, section count left at the
// constructor's 1.
void BrnRendererMemory::CreateDepthOfFieldBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpDepthOfField = new CgsRenderTarget();

    lpDepthOfField->ClearColourTargetInUse();

    lpDepthOfField->SetDimensions(KU_DEPTH_OF_FIELD_BUFFER_WIDTH, KU_DEPTH_OF_FIELD_BUFFER_HEIGHT);
    lpDepthOfField->SetNumMipMaps(1);
    lpDepthOfField->SetMultisampleFormat(0);
    lpDepthOfField->SetUseDepthStencilAsTexture(false);

    lpDepthOfField->SetColourTargetInUse(0, true);
    lpDepthOfField->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpDepthOfField->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpDepthOfField->SetColourTargetBaseEDRAM(0, 0);

    lpDepthOfField->SetDepthTargetInUse(false);

    lpDepthOfField->Construct(lpAllocator);

    // Slot written last, after Construct (stw r31, 0x1C(r29) @0x823F72AC).
    mapRenderTarget[E_RENDER_TARGET_DEPTH_OF_FIELD] = lpDepthOfField;
}

// 0x823F72B8 -- build the PARTICLE buffer: a HALF-resolution colour target with its own depth
// surface, and the only pool target whose depth is BOTH written and sampled. Pool slot 9
// (E_RENDER_TARGET_PARTICLE).
//
// This is the one creator in this group whose size is derived rather than immediate: the width and
// height are mu32ScreenWidth/mu32ScreenHeight >> 1, i.e. half the front-buffer resolution in each axis
// (a quarter of the pixels -- which is what BrnRendererModule's own phase counter calls
// miQuarterResParticles). Those two members are the ones Construct seeded from the device parameters,
// and they are read BY NAME here, never at the console's +0x5C/+0x60.
//
// mbUseDepthStencilAsTexture is TRUE and the depth record carries the same packed depth format the
// shadow map uses (0x2D200196 in both the buffer and the texture slot), so the half-res depth buffer is
// resolvable to a sampleable texture -- the soft-particle depth fade reads it.
//
// THE DEPTH SURFACE IS PLACED IMMEDIATELY AFTER THE COLOUR SURFACE IN EDRAM. The value stored to the
// depth record's base-EDRAM field is a TILE COUNT: the number of Xenos EDRAM tiles the colour surface
// occupies. The X360 computes it strength-reduced (a divwu by 80, then q + 4q shifted left 4 to get
// back to 80q, then a byte scale and a divwu by 0x1400); de-optimised, that is
//     tiles = alignUp(width, 80) * alignUp(height, 32) * 4 bytes / 5120
// and 5120 == 80 * 16 * 4 is one EDRAM tile (80x16 samples at 4 bytes each). For the 1280x720 front
// buffer that is 640x360 -> alignUp 640x384 -> 192 tiles. The same rule appears as a baked constant in
// CreateEnvmapBuffer (64 tiles), which is what pins the arithmetic.
void BrnRendererMemory::CreateParticleBuffer(rw::IResourceAllocator* lpAllocator)
{
    // Half the screen in each axis (unsigned shift right by one on each member).
    const u32 luWidth  = mu32ScreenWidth  / 2u;
    const u32 luHeight = mu32ScreenHeight / 2u;

    // The colour surface's EDRAM footprint, in tiles -- the depth surface's base.
    //
    // ⚠️ THIS HELPER TAKES ITS ROW ALIGNMENT AS A PARAMETER because the pool is NOT consistent about
    // it: CreateAntiAliasBuffer @0x823F6B40 and CreateDownSampleBuffer @0x823F6E60 align rows to 16
    // (clrrwi r10, r7, 4 @0x823F6C4C / @0x823F6F34) but THIS function aligns them to 32
    // (& 0xFFFFFFE0). Folding all three onto one hard-coded 16 would have silently halved the
    // particle buffer's depth base.
    const u32 luColourSurfaceTiles =
        CountEDRAMTiles(luWidth, luHeight, KU_EDRAM_PARTICLE_ROW_ALIGNMENT);

    CgsRenderTarget* lpParticle = new CgsRenderTarget();

    lpParticle->ClearColourTargetInUse();

    lpParticle->SetDimensions(luWidth, luHeight);
    lpParticle->SetNumMipMaps(1);
    lpParticle->SetMultisampleFormat(0);
    // The depth surface is resolved to a sampleable texture (the soft-particle depth read).
    lpParticle->SetUseDepthStencilAsTexture(true);

    // Colour section 0: in use only -- no filter-mode store and mbUseDevice left cleared, so
    // CgsRenderTarget::Construct derives eRenderTarget_CREATE (see CreateBloomBuffer's note).
    lpParticle->SetColourTargetInUse(0, true);
    lpParticle->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpParticle->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpParticle->SetColourTargetBaseEDRAM(0, 0);

    // The depth/stencil surface, immediately after the colour surface in EDRAM. Note the tile INDEX is
    // NOT set here (the constructor's -1 "unassigned" sentinel stands) -- unlike CreateShadowmapBuffer,
    // which pins tile slot 0.
    lpParticle->SetDepthTargetInUse(true);
    lpParticle->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpParticle->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);
    lpParticle->SetDepthTargetBaseEDRAM(luColourSurfaceTiles);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpParticle->Construct(lpAllocator);

    // Slot written last, after Construct (stw r31, 0x24(r30) @0x823F73B8).
    mapRenderTarget[E_RENDER_TARGET_PARTICLE] = lpParticle;
}

// 0x823F73C8 -- build the SUN-CORONA buffer: a 1x1 colour-only render target in pool slot 10
// (E_RENDER_TARGET_SUN_CORONA).
//
// ONE PIXEL, and that is not a placeholder: the X360 loads a single `li r10, 1` @0x823F7420 and stores
// that same register to the width, the height AND the mip count. It is the smallest possible target,
// which is what the sun-corona visibility test needs -- BrnRendererModule's own render-phase counter
// names the pass miSunCoronaVisibilityTest, and a 1x1 resolved surface is the read-back the test
// consumes. No depth surface, no MSAA, section count left at the constructor's 1.
//
// Like the bloom and depth-of-field buffers (and unlike the back buffer, particle buffer and env map)
// it stores the BUFFER format word into the texture-format slot too -- 0x18280186 goes to both.
// Colour section 0 is again marked in use with a single store, leaving the constructor's filter mode
// and the cleared mbUseDevice alone (see CreateBloomBuffer's USE_DEVICE_FOR_WRITE note).
void BrnRendererMemory::CreateSunCoronaBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpSunCorona = new CgsRenderTarget();

    lpSunCorona->ClearColourTargetInUse();

    lpSunCorona->SetDimensions(KU_SUN_CORONA_BUFFER_EXTENT, KU_SUN_CORONA_BUFFER_EXTENT);
    lpSunCorona->SetNumMipMaps(1);
    lpSunCorona->SetMultisampleFormat(0);
    lpSunCorona->SetUseDepthStencilAsTexture(false);

    lpSunCorona->SetColourTargetInUse(0, true);
    lpSunCorona->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpSunCorona->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpSunCorona->SetColourTargetBaseEDRAM(0, 0);

    lpSunCorona->SetDepthTargetInUse(false);

    // Realise the post-fx render target (CgsRenderTarget::Construct, the first virtual).
    lpSunCorona->Construct(lpAllocator);

    // Slot written last, after Construct (stw r31, 0x28(r29) @0x823F746C).
    mapRenderTarget[E_RENDER_TARGET_SUN_CORONA] = lpSunCorona;
}

// =================================================================================================
// THE BLIT DRAW SURFACE (2026-09-05, the quarter-res particle chain).
//
// Everything the two blits need that the console reaches through CgsGraphics::Im2d. The PC Im2d
// (CgsIm2d.cpp:150-193) is the fixed-function 2D GUI path -- it selects the fixed-function
// pipeline, rewrites every vertex from 1280x720 logical coordinates, and cannot carry a bound
// vertex/pixel program pair -- so the two blits draw the way BrnSunCorona's two passes already do
// on this backend: D3DDevice_BeginVertices with an explicit descriptor, in NDC.
// =================================================================================================
namespace
{
    // The three constant names BlitComposite resolves through
    // renderengine::ProgramBuffer::GetVariableHandleByName. gQuincunxOffsets is the console's
    // third; it is NOT looked up here because the PC pixel program does not declare it (see
    // BlitComposite's banner), and a name with no declaration would resolve to a count-0 handle.
    const u8 gszBlitUVOffsets[] = "gUVOffsets";
    const u8 gszBlitUVScales[]  = "gUVScales";

    // The Xenos vertex-format words + element-type lanes of the shared blit declaration -- the sun
    // corona's, value for value (BrnSunCorona.cpp:113-121).
    const u32 KU_BLIT_ELEMENT_FORMAT_FLOAT3 = 0x2A23B9u;
    const u32 KU_BLIT_ELEMENT_FORMAT_FLOAT2 = 0x2C23A5u;
    const u8  KU8_BLIT_ELEMENT_TYPE_POSITION  = 1u;
    const u8  KU8_BLIT_ELEMENT_TYPE_TEXCOORD0 = 6u;

    // 6 == TRIANGLESTRIP (XenonD3D9Shims.cpp:125 maps it straight to D3DPT_TRIANGLESTRIP); the
    // console's Basic2dColouredTexturedVertex_::Render is handed the same 6 and the same 4.
    const u32 KU_BLIT_PRIMITIVE_TRIANGLESTRIP = 6u;
    const u32 KU_BLIT_QUAD_VERTEX_COUNT       = 4u;

    // The overlay sampler unit -- the console's own (`sub_8227D158(v14, 1)` in BlitComposite), and
    // the register the PC recompile's CTAB puts OverlaySampler at.
    const u32 KU_BLIT_OVERLAY_SAMPLER_UNIT = 1u;
    // renderengine::PostFxSourceSampler_ApplyState's min/mag filter word: 1 == LINEAR, which is
    // what Construct @0x823FCA38 seeds the blit texture state with (mu8MinFilter = mu8MagFilter = 1).
    const u32 KU_BLIT_SAMPLER_FILTER_LINEAR = 1u;

    // BlitComposite's own 0.5 -- the two `v65[0] = 0.5 / v66[0] = 0.5` splats it multiplies the two
    // reciprocal-extent vectors by.
    const f32 KF_BLIT_HALF_TEXEL = 0.5f;

    // The shared two-element vertex declaration both program pairs draw through. File-scope because
    // it belongs to the pair of programs, not to a render target, and because the console's own
    // equivalent (the Im2d renderer's declaration) is likewise built once and reused.
    renderengine::VertexDescriptorData* gpBlitVertexDescriptor = nullptr;

    // The 20-byte vertex the shared declaration auto-packs to: POSITION0 FLOAT3 at +0, TEXCOORD0
    // FLOAT2 at +12. The layout MUST byte-match what VertexDescriptor::Initialize packs.
    struct BlitVertex
    {
        f32 mafPosition[3];   // +0x00  POSITION0 -- ALREADY IN NDC
        f32 mafUv[2];         // +0x0C  TEXCOORD0
    };
    static_assert(sizeof(BlitVertex) == 20, "the blit vertex is the shared 20-byte record");

    // The console's unit-square quad, in the console's own order. BlitComposite builds four
    // Basic2dColouredTexturedVertex records at (0,0) (0,1) (1,0) (1,1) with matching uvs and a
    // 0xFFFFFFFF colour, and draws them as a TRIANGLESTRIP; here the xy pair is mapped to NDC
    // (x*2-1, 1-y*2) because the Im2d transform that did that on the console is gone, and the
    // colour lane is dropped because neither PC program declares a colour input.
    void DrawBlitQuad()
    {
        BlitVertex* lpVertex = static_cast<BlitVertex*>(
            D3DDevice_BeginVertices(0, KU_BLIT_PRIMITIVE_TRIANGLESTRIP,
                                    KU_BLIT_QUAD_VERTEX_COUNT, sizeof(BlitVertex)));
        if (lpVertex != nullptr)
        {
            static const f32 kaafQuad[KU_BLIT_QUAD_VERTEX_COUNT][5] =
            {
                // NDC x, NDC y, z, u, v      (console vertex, in its own order)
                { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },   // (0,0)
                { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },   // (0,1)
                {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },   // (1,0)
                {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },   // (1,1)
            };
            for (u32 luCorner = 0; luCorner < KU_BLIT_QUAD_VERTEX_COUNT; ++luCorner)
            {
                lpVertex[luCorner].mafPosition[0] = kaafQuad[luCorner][0];
                lpVertex[luCorner].mafPosition[1] = kaafQuad[luCorner][1];
                lpVertex[luCorner].mafPosition[2] = kaafQuad[luCorner][2];
                lpVertex[luCorner].mafUv[0]       = kaafQuad[luCorner][3];
                lpVertex[luCorner].mafUv[1]       = kaafQuad[luCorner][4];
            }
        }
        D3DDevice_EndVertices(0);
    }
}

// =================================================================================================
// THE QUARTER-RES PARTICLE COMPOSITE CHAIN (2026-09-05).
//
// The console draws Lion particles into a CLEARED, SEPARATE half-by-half buffer and composites
// that buffer OVER the scene with a (1 - accumulatedAlpha) term; this build drew them straight
// onto the scene with no such term, so a saturating plume ADDED its background instead of
// REPLACING it -- which is what left the boost flame with a white core after the white-level fix
// in a4c7670b. The chain is five console functions:
//     BrnRendererMemory::CreateParticleBuffer        @0x823F72B8  (already in this file)
//     BrnRendererModule::BeginQuarterResBuffer       @0x82408C38  (BrnRendererModule.cpp)
//     ParticleModule::RenderQuarterResParticles      @0x82294A20  (ParticleModule.cpp)
//     BrnRendererModule::EndRenderAntiAliased        @0x82408B00  (BrnRendererModule.cpp)
//     BrnRendererMemory::BlitComposite               @0x82406A68  (here)
// plus BrnRendererMemory::BlitDepth @0x82406EA8 (here), which is what keeps the particles
// depth-tested once they leave the scene target's own depth buffer.
// =================================================================================================

// [PC bring-up] Build pool slot 9 and the four blit programs. NOT a console function -- both halves
// are Construct @0x823FCA38's, which is gated out for the two reasons the
// BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner names.
//
// The RENDER TARGET half calls the real CreateParticleBuffer, exactly as its three sibling
// bring-ups call theirs. The PROGRAM half is the one thing Construct does that has no console body
// available on PC: its four gacIm2d*BlitProgram blobs are Xenos microcode inside the guest image.
// They are ADOPTED from the authored D3D9 recompiles instead (pc/gcm/renderengine/
// Im2dBlitProgramsPC.cpp, generated from tools/assets/shaders/brn_im2dblit.fx) -- the identical
// route BrnSunCorona::Construct takes for its own four executable-embedded programs, and the reason
// this cannot simply be `CreateBlitProgram(gacIm2dDepthBlitVertexProgram, ...)`.
//
// Also NOT done here: mBlitTextureStateResource / mpBlitTextureState. The console's BlitDepth
// Initialize's that state per call over the source's depth texture; on PC the source render target
// already owns a depth TextureState carrying the same texture, so BlitDepth reads that instead and
// nothing has to be carved. See BlitDepth's banner.
//
// IDEMPOTENT, and it must be: CreateParticleBuffer `new`s a CgsRenderTarget and publishes it into
// the slot, so a second call would leak the first AND hand the composite a different object than
// the particle pass rendered into (the same hazard PCBringUpCreateSunCoronaBuffer's early return
// exists for). DELETE WITH THE BRING-UP, together with its three siblings.
bool BrnRendererMemory::PCBringUpCreateParticleCompositeChain(rw::IResourceAllocator* lpAllocator)
{
    CGS_ASSERT(mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] != nullptr,
               "GetShadowMapBuffer(0) != NULL -- slot-nulling bring-up must run first");

    // The pool sizes this target from mu32ScreenWidth/mu32ScreenHeight >> 1, so it needs the same
    // precondition PCBringUpCreatePostFxSceneTargets has: the screen extent must be seeded.
    if (mu32ScreenWidth == 0u || mu32ScreenHeight == 0u)
    {
        return false;
    }

    if (mapRenderTarget[E_RENDER_TARGET_PARTICLE] == nullptr)
    {
        CreateParticleBuffer(lpAllocator);

        CgsRenderTarget* const lpParticle = mapRenderTarget[E_RENDER_TARGET_PARTICLE];
        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[qres] particle buffer created: %ux%u rt=%d colourState=%d depthState=%d\n",
                      (unsigned)(lpParticle != nullptr ? lpParticle->GetWidth() : 0u),
                      (unsigned)(lpParticle != nullptr ? lpParticle->GetHeight() : 0u),
                      (int)(lpParticle != nullptr && lpParticle->GetRenderTarget() != nullptr),
                      (int)(lpParticle != nullptr && lpParticle->GetRenderTarget() != nullptr
                            && lpParticle->GetRenderTarget()->maColourTargets[0].mpTextureState != nullptr),
                      (int)(lpParticle != nullptr && lpParticle->GetRenderTarget() != nullptr
                            && lpParticle->GetRenderTarget()->GetDepthTextureState() != nullptr));
        CgsDev::Log::WriteToLog(lacMessage);
    }

    if (mpCompositeBlitVertexProgramBuffer == nullptr)
    {
        mpDepthBlitVertexProgramBuffer = renderengine::ProgramBufferPC_Adopt(
            renderengine::gauIm2dDepthBlitVertexProgramPC,
            renderengine::guIm2dDepthBlitVertexProgramPCSize, 0u);
        mpDepthBlitPixelProgramBuffer = renderengine::ProgramBufferPC_Adopt(
            renderengine::gauIm2dDepthBlitPixelProgramPC,
            renderengine::guIm2dDepthBlitPixelProgramPCSize, 1u);
        mpCompositeBlitVertexProgramBuffer = renderengine::ProgramBufferPC_Adopt(
            renderengine::gauIm2dCompositeBlitVertexProgramPC,
            renderengine::guIm2dCompositeBlitVertexProgramPCSize, 0u);
        mpCompositeBlitPixelProgramBuffer = renderengine::ProgramBufferPC_Adopt(
            renderengine::gauIm2dCompositeBlitPixelProgramPC,
            renderengine::guIm2dCompositeBlitPixelProgramPCSize, 1u);

        // The shared two-element declaration both pairs draw through (the sun corona's, value for
        // value -- see brn_im2dblit.fx's VERTEX INPUT note). The offset lane is left at the ctor's
        // 0xFFFF, which is the auto-pack request that produces 0 / 12.
        renderengine::VertexDescriptor::Parameters lVertexDecl;
        lVertexDecl.maElements[0].mu16Stream    = 0;
        lVertexDecl.maElements[0].miOffset      = static_cast<s32>(KU_BLIT_ELEMENT_FORMAT_FLOAT3);
        lVertexDecl.maElements[0].mu8UsageIndex = KU8_BLIT_ELEMENT_TYPE_POSITION;
        lVertexDecl.maElements[1].mu16Stream    = 0;
        lVertexDecl.maElements[1].miOffset      = static_cast<s32>(KU_BLIT_ELEMENT_FORMAT_FLOAT2);
        lVertexDecl.maElements[1].mu8UsageIndex = KU8_BLIT_ELEMENT_TYPE_TEXCOORD0;
        gpBlitVertexDescriptor = renderengine::VertexDescriptor::Initialize(0, &lVertexDecl);

        // [FLAG PC bring-up gate] A resolved-but-not-found handle reads mu8RegisterCount == 0 and
        // RenderEngineDeviceBeginShaderStates routes its row to the discard row -- the constant
        // never reaches the shader, the composite samples uv (0,0) forever, and NOTHING errors.
        // Refuse the whole chain instead, exactly as BrnSunCorona::Construct refuses its own.
        renderengine::ProgramVariableHandle lUvOffsets;
        renderengine::ProgramVariableHandle lUvScales;
        if (mpCompositeBlitVertexProgramBuffer != nullptr)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                mpCompositeBlitVertexProgramBuffer, gszBlitUVOffsets, &lUvOffsets);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                mpCompositeBlitVertexProgramBuffer, gszBlitUVScales, &lUvScales);
        }

        char lacMessage[256];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[qres] blit programs adopted: depthVs=%d depthPs=%d compVs=%d compPs=%d"
                      " decl=%d gUVOffsets{reg=%u,count=%u} gUVScales{reg=%u,count=%u}\n",
                      (int)(mpDepthBlitVertexProgramBuffer != nullptr),
                      (int)(mpDepthBlitPixelProgramBuffer != nullptr),
                      (int)(mpCompositeBlitVertexProgramBuffer != nullptr),
                      (int)(mpCompositeBlitPixelProgramBuffer != nullptr),
                      (int)(gpBlitVertexDescriptor != nullptr),
                      (unsigned)lUvOffsets.mu8RegisterIndex, (unsigned)lUvOffsets.mu8RegisterCount,
                      (unsigned)lUvScales.mu8RegisterIndex, (unsigned)lUvScales.mu8RegisterCount);
        CgsDev::Log::WriteToLog(lacMessage);

        if (lUvOffsets.mu8RegisterCount == 0u || lUvScales.mu8RegisterCount == 0u)
        {
            CgsDev::Log::WriteToLog(
                "[qres] the authored composite vertex program does NOT declare gUVOffsets /"
                " gUVScales by those exact names -- the quarter-res particle chain stays off\n");
            mpDepthBlitVertexProgramBuffer     = nullptr;
            mpDepthBlitPixelProgramBuffer      = nullptr;
            mpCompositeBlitVertexProgramBuffer = nullptr;
            mpCompositeBlitPixelProgramBuffer  = nullptr;
        }
    }

    return PCBringUpParticleCompositeChainReady();
}

// A pure query -- see the declaration. Every surface and program the two blits dereference without
// a test of their own is checked here, ONCE, so the renderer's routing decision and the blits agree
// about what "live" means.
bool BrnRendererMemory::PCBringUpParticleCompositeChainReady() const
{
    CgsRenderTarget* const lpParticle = mapRenderTarget[E_RENDER_TARGET_PARTICLE];
    if (lpParticle == nullptr || lpParticle->GetRenderTarget() == nullptr)
        return false;
    if (lpParticle->GetRenderTarget()->maColourTargets[0].mpTextureState == nullptr)
        return false;

    return mpDepthBlitVertexProgramBuffer     != nullptr
        && mpDepthBlitPixelProgramBuffer      != nullptr
        && mpCompositeBlitVertexProgramBuffer != nullptr
        && mpCompositeBlitPixelProgramBuffer  != nullptr
        && gpBlitVertexDescriptor             != nullptr;
}

// =================================================================================================
// BrnRendererMemory::BlitDepth -- X360 @0x82406EA8.
//
// Console body, in order: read the source's postfx RenderTarget; SetShaderGPRAllocation; load the
// Im2d blit transform out of flt_830112D0 and hand it to Basic2dColouredTexturedVertex_::
// BeginRendering + SetTransform; build a clamped bilinear TextureState over the SOURCE'S DEPTH
// TEXTURE (`v47 = *(v3 + 136)` == mDepthTarget.mpTexture) into mpBlitTextureState; bind it to
// sampler 0; install mpDepthBlitVertexProgramBuffer / mpDepthBlitPixelProgramBuffer; draw the unit
// quad; ResetShadowing. The pixel program is three instructions --
//     tfetch r0.x, uv, tf0 ; sgts export0.x, -r0.x ; maxs export61.x, r0.x [clamp]
// -- and export61 IS the Xenos depth export, so the whole pass is "write the sampled depth as this
// fragment's depth". The caller supplies the state: BeginQuarterResBuffer @0x82408C38 pushes
// depth-stencil slot 3 (ZON_ZALL_ZWRITEON), rasteriser slot 2 (CullModeNone) and blend slot 7
// (NoColourWrite_NoAlphaTest) immediately before calling it.
//
// FOUR PC SUBSTITUTIONS, each named where it happens:
//  (1) the Im2d renderer is not used (its PC realisation is the fixed-function 2D GUI path and
//      cannot carry a bound program pair -- brn_im2dblit.fx's VERTEX INPUT note), so the quad is
//      drawn through D3DDevice_BeginVertices with an explicit descriptor and arrives in NDC. That
//      also makes the console's transform load and gOffsetXYZ/gRightUp dead.
//  (2) mpBlitTextureState is not built. The source render target already owns a depth TextureState
//      over the very texture the console's per-call state would wrap (PostFxRenderTargetPCLeaf.cpp
//      builds it from mDepthTarget's hi-Z-else-depth texture), and on this backend a TextureState's
//      sampler words are stored, not applied -- so a second object over the same raster would
//      differ in nothing and would have to be carved every frame.
//  (3) D3DDevice_SetTexture forces POINT/CLAMP on any unit that receives a raw-depth texture
//      (ApplyRawDepthSamplerState), which is what a depth fetch must have and what the console's
//      own CreateStates writes (mag = min = 0). Nothing extra is installed here.
//  (4) SetShaderGPRAllocation is dropped, as this tree drops the other four occurrences: it is a
//      Xenos GPR-partition hint with no D3D9 counterpart.
// =================================================================================================
void BrnRendererMemory::BlitDepth(CgsGraphics::Im2d& /*lIm2d*/, CgsRenderTarget* lpSource)
{
    if (lpSource == nullptr || lpSource->GetRenderTarget() == nullptr)
        return;

    renderengine::TextureState* const lpSourceDepth =
        lpSource->GetRenderTarget()->GetDepthTextureState();
    if (lpSourceDepth == nullptr)
    {
        static bool sbLoggedNoDepth = false;
        if (!sbLoggedNoDepth)
        {
            sbLoggedNoDepth = true;
            CgsDev::Log::WriteToLog(
                "[qres] BlitDepth REFUSED: the source target has no sampleable depth TextureState"
                " -- the quarter-res particles will not be occluded by the world\n");
        }
        return;
    }

    shadow::Device::SetVertexProgram(mpDepthBlitVertexProgramBuffer);
    shadow::Device::SetPixelProgram(mpDepthBlitPixelProgramBuffer);

    shadow::Device::SetState(lpSourceDepth, 0u);
    shadow::Device::SetVertexDescriptor(gpBlitVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    DrawBlitQuad();

    // The console's own last act (`dword_83010F9C = 0; shadow::Device::ResetShadowing()` at the
    // tail of 0x82406EA8, the same pair BlitComposite ends with). It matters here for a reason the
    // console never had to think about: this pass leaves the SOURCE'S DEPTH TEXTURE bound at unit
    // 0, which is the unit LionParticleRender::SetMaterial keys its own bind on -- and the pass
    // that runs next, in the bracket this one opens, IS the Lion particle pass.
    shadow::Device::ResetShadowing();
}

// =================================================================================================
// BrnRendererMemory::BlitComposite -- X360 @0x82406A68.
//
// Console body, in order: read both sources' postfx RenderTargets; Im2d BeginRendering +
// SetTransform; bind source0's colour TextureState (`v7[11]` == maColourTargets[0].mpTextureState)
// to sampler 0 and source1's to sampler 1; install the composite program pair; build the unit-square
// quad; then THREE shader constants --
//     gUVOffsets  (VERTEX c2) = gbHalfTexelBlitOffset ? 0.5 * (1/w0, 1/h0, 1/w1, 1/h1) : 0
//     gUVScales   (VERTEX c3) = (1, 1, 1, 1)
//     gQuincunxOffsets (PIXEL c0) = (0.166, -0.166, 0, 0) * (1/w0, 1/h0, 1/w0, 1/h0)
// -- and Render(6 /*TRIANGLESTRIP*/, quad, 4), ResetShadowing. gbHalfTexelBlitOffset is
// byte_82F2423E, which dumps as 0x01 (the dword at 0x82F2423C is 0x01010101), so the half-texel
// arm is the console's live one; it is also exactly the correction D3D9's texel-centre rule needs,
// so it is applied here rather than being a console-only quirk.
//
// The caller supplies the state: EndRenderAntiAliased @0x82408B00 binds the DOWN-SAMPLE target and
// pushes depth-stencil slot 1 (ZOFF_ZALL_ZWRITEOFF), rasteriser slot 2 (CullModeNone) and blend
// slot 0 (Opaque_Modulate_NoAlphaTest_DestRGBA) immediately before calling it.
//
// ⚠ THE TWO PC DEVIATIONS ARE IN Im2dCompositeBlit_ApplyRopState (ShadowPassPCLeaf.h) AND IN THE
// PIXEL PROGRAM (brn_im2dblit.fx). In one sentence each: the scene term moves onto the D3D9 blend
// unit because a texture cannot be sampled while it is the bound render target on this backend, so
// BaseSampler and gQuincunxOffsets are not used and the two SCENE taps' 0.5 average is dropped;
// and the write is masked to RGB because this backend carries the motion-blur mask in the scene
// target's alpha, where the console carries it in the stencil. Everything else -- the particle
// term, the two UV constants, the quad, the sampler unit numbers -- is the console's.
//
// lbFlag0 / lbFlag1 are the two trailing bool arguments the X360 call site passes as 0
// (`li r7, 0` @0x82408C00 / `li r8, 0` @0x82408BF8). Hex-Rays loses them and no path in the
// reconstructed body reads them, so they are carried and unused rather than invented behaviour.
// =================================================================================================
void BrnRendererMemory::BlitComposite(CgsGraphics::Im2d& /*lIm2d*/, CgsRenderTarget* lpSource0,
                                      CgsRenderTarget* lpSource1, bool /*lbFlag0*/, bool /*lbFlag1*/)
{
    if (lpSource0 == nullptr || lpSource0->GetRenderTarget() == nullptr
        || lpSource1 == nullptr || lpSource1->GetRenderTarget() == nullptr)
        return;

    rw::graphics::postfx::RenderTarget* const lpBase    = lpSource0->GetRenderTarget();
    rw::graphics::postfx::RenderTarget* const lpOverlay = lpSource1->GetRenderTarget();

    renderengine::TextureState* const lpOverlayState = lpOverlay->maColourTargets[0].mpTextureState;
    if (lpOverlayState == nullptr)
    {
        static bool sbLoggedNoOverlay = false;
        if (!sbLoggedNoOverlay)
        {
            sbLoggedNoOverlay = true;
            CgsDev::Log::WriteToLog(
                "[qres] BlitComposite REFUSED: the overlay target has no colour TextureState\n");
        }
        return;
    }

    shadow::Device::SetVertexProgram(mpCompositeBlitVertexProgramBuffer);
    shadow::Device::SetPixelProgram(mpCompositeBlitPixelProgramBuffer);

    // ---- gUVOffsets (VERTEX c2): the half-texel correction, per source ---------------------------
    // The console's `1.0 / width` pair for each source, splatted through the two vrlimi128 pairs and
    // multiplied by the two 0.5 splats. Both sources are read at their OWN extent, which is why the
    // overlay lane uses the half-res particle buffer's numbers and not the scene's.
    const f32 lfBaseWidth     = static_cast<f32>(lpBase->muWidth);
    const f32 lfBaseHeight    = static_cast<f32>(lpBase->muHeight);
    const f32 lfOverlayWidth  = static_cast<f32>(lpOverlay->muWidth);
    const f32 lfOverlayHeight = static_cast<f32>(lpOverlay->muHeight);
    if (lfBaseWidth <= 0.0f || lfBaseHeight <= 0.0f
        || lfOverlayWidth <= 0.0f || lfOverlayHeight <= 0.0f)
        return;

    const f32 lafUvOffsets[4] =
    {
        KF_BLIT_HALF_TEXEL / lfBaseWidth,     KF_BLIT_HALF_TEXEL / lfBaseHeight,
        KF_BLIT_HALF_TEXEL / lfOverlayWidth,  KF_BLIT_HALF_TEXEL / lfOverlayHeight,
    };
    // gUVScales (VERTEX c3): `vspltisw v0, 1 ; vcsxwfp128 v126, v0, 0` -- an all-ones float4.
    const f32 lafUvScales[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    renderengine::ProgramVariableHandle lUvOffsets;
    renderengine::ProgramVariableHandle lUvScales;
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpCompositeBlitVertexProgramBuffer, gszBlitUVOffsets, &lUvOffsets);
    renderengine::ProgramBuffer::GetVariableHandleByName(
        mpCompositeBlitVertexProgramBuffer, gszBlitUVScales, &lUvScales);

    {
        void* lpRow = 0;
        RenderEngineDeviceBeginShaderStates(&lUvOffsets, &lpRow);
        if (lpRow != 0)
            std::memcpy(lpRow, lafUvOffsets, sizeof(lafUvOffsets));
    }
    {
        void* lpRow = 0;
        RenderEngineDeviceBeginShaderStates(&lUvScales, &lpRow);
        if (lpRow != 0)
            std::memcpy(lpRow, lafUvScales, sizeof(lafUvScales));
    }

    // Sampler 1 is the console's own unit for the overlay (`sub_8227D158(v14, 1)`), and the PC
    // recompile's CTAB puts OverlaySampler at s1 to match. Sampler 0 (BaseSampler) is deliberately
    // NOT bound -- see the deviation note above; binding the scene there while it is the render
    // target is the undefined case this whole re-association exists to avoid.
    shadow::Device::SetState(lpOverlayState, KU_BLIT_OVERLAY_SAMPLER_UNIT);
    renderengine::PostFxSourceSampler_ApplyState(KU_BLIT_OVERLAY_SAMPLER_UNIT,
                                                 KU_BLIT_SAMPLER_FILTER_LINEAR, 1u);
    shadow::Device::SetVertexDescriptor(gpBlitVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    renderengine::Im2dCompositeBlit_ApplyRopState();

    DrawBlitQuad();

    // The console's own last act (`shadow::Device::ResetShadowing`) -- and on PC it is what stops
    // the raw D3D9 words Im2dCompositeBlit_ApplyRopState just wrote from being remembered as a
    // state object that a later SetState would then skip as redundant.
    shadow::Device::ResetShadowing();

    {
        static bool sbLoggedFirst = false;
        if (!sbLoggedFirst)
        {
            sbLoggedFirst = true;
            char lacMessage[224];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[qres] first composite: base %ux%u overlay %ux%u"
                          " uvOffsets=(%.6f %.6f %.6f %.6f)\n",
                          (unsigned)lpBase->muWidth, (unsigned)lpBase->muHeight,
                          (unsigned)lpOverlay->muWidth, (unsigned)lpOverlay->muHeight,
                          lafUvOffsets[0], lafUvOffsets[1], lafUvOffsets[2], lafUvOffsets[3]);
            CgsDev::Log::WriteToLog(lacMessage);
        }
    }
}
