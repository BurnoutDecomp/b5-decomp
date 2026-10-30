#include "GameSource/Graphics/BrnRendererMemory.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"
#include "GameSource/Graphics/BrnAntiAliasTiling.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "GameSource/Resource/BrnResourceAllocator.h"

namespace
{

    const u32 KU_COLOUR_SURFACE_BUFFER_FORMAT  = 0x18280186u;
    const u32 KU_COLOUR_SURFACE_TEXTURE_FORMAT = 0x18280106u;

    const u32 KU_SHADOW_MAP_WIDTH            = 1280u;
    const u32 KU_SHADOW_MAP_COMBINED_HEIGHT  = 1920u;
    const u32 KU_SHADOW_MAP_NUM_CASCADES     = 3u;

    const u32 KU_DEPTH_SURFACE_FORMAT = 0x2D200196u;

    const u32 KU_WORK_BUFFER_WIDTH            = 320u;
    const u32 KU_WORK_BUFFER_HEIGHT           = 180u;
    const u32 KU_BLOOM_BUFFER_WIDTH           = 320u;
    const u32 KU_BLOOM_BUFFER_HEIGHT          = 180u;

    const u32 KU_DEPTH_OF_FIELD_BUFFER_WIDTH  = 320u;
    const u32 KU_DEPTH_OF_FIELD_BUFFER_HEIGHT = 180u;

    const u32 KU_ENV_MAP_FACE_SIZE            = 128u;
    const u32 KU_ENV_MAP_TEXTURE_TYPE_CUBE    = 3u;
    const s32 KI_ENV_MAP_MULTISAMPLE_FORMAT   = 2;

    const u32 KU_ENV_MAP_DEPTH_FORMAT         = 0x1A220197u;

    const u32 KU_ENV_MAP_DEPTH_BASE_EDRAM_TILES = 64u;
    const u32 KU_SUN_CORONA_BUFFER_EXTENT     = 1u;

    const u32 KU_EDRAM_TILE_WIDTH             = 80u;
    const u32 KU_EDRAM_TILE_HEIGHT            = 16u;
    const u32 KU_EDRAM_PARTICLE_ROW_ALIGNMENT = 32u;
    const u32 KU_EDRAM_BYTES_PER_TILE         = 0x1400u;
    const u32 KU_EDRAM_BYTES_PER_SAMPLE       = 4u;

    u32 CountEDRAMTiles(u32 luWidth, u32 luHeight, u32 luRowAlignment)
    {
        const u32 luTiledWidth  = ((luWidth + KU_EDRAM_TILE_WIDTH - 1u) / KU_EDRAM_TILE_WIDTH)
                                  * KU_EDRAM_TILE_WIDTH;
        const u32 luTiledHeight = ((luHeight + luRowAlignment - 1u) / luRowAlignment) * luRowAlignment;
        return luTiledWidth * luTiledHeight * KU_EDRAM_BYTES_PER_SAMPLE / KU_EDRAM_BYTES_PER_TILE;
    }
}

namespace
{
// ARTIST 8203DAA8, 340 bytes, unchanged native shader data.
const u32 kaDepthBlitVertexProgram[] =
{
    0x102A1101u, 0x000000DCu, 0x00000078u, 0x00000000u, 0x00000024u, 0x00000000u, 0x000000A4u, 0x00000000u,
    0x00000000u, 0x0000007Cu, 0x0000001Cu, 0x00000070u, 0xFFFE0300u, 0x00000002u, 0x0000001Cu, 0x00000008u,
    0x00000069u, 0x00000044u, 0x00020000u, 0x00010000u, 0x00000050u, 0x00000000u, 0x00000060u, 0x00020001u,
    0x00010000u, 0x00000050u, 0x00000000u, 0x674F6666u, 0x73657458u, 0x595A00ABu, 0x00010003u, 0x00010004u,
    0x00010000u, 0x00000000u, 0x67526967u, 0x68745570u, 0x0076735Fu, 0x335F3000u, 0x322E302Eu, 0x36353334u,
    0x2E3100ABu, 0x00000000u, 0x00000078u, 0x00010000u, 0x00000000u, 0x00000000u, 0x00000821u, 0x00000001u,
    0x00000002u, 0x00000001u, 0x00000290u, 0x00000003u, 0x00005007u, 0x00003050u, 0x00001008u, 0x10011003u,
    0x00001200u, 0xC2000000u, 0x00003004u, 0x00001200u, 0xC4000000u, 0x10092007u, 0x00002200u, 0x00000000u,
    0x05F80000u, 0x00000E47u, 0x00000000u, 0x14C8C03Eu, 0x000000C6u, 0xC2000000u, 0xC801803Eu, 0x00B26E6Cu,
    0x91000100u, 0xC802803Eu, 0x00B2B3B1u, 0x91000100u, 0x05F80000u, 0x00000FC8u, 0x00000000u, 0xC8038000u,
    0x00B0B000u, 0xE2000000u, 0x00000000u, 0x00000000u, 0x00000000u,
};

// ARTIST 8203DC00, 236 bytes, unchanged native shader data.
const u32 kaDepthBlitPixelProgram[] =
{
    0x102A1100u, 0x000000B0u, 0x0000003Cu, 0x00000000u, 0x00000024u, 0x00000000u, 0x0000008Cu, 0x00000000u,
    0x00000000u, 0x00000064u, 0x0000001Cu, 0x00000057u, 0xFFFF0300u, 0x00000001u, 0x0000001Cu, 0x00000008u,
    0x00000050u, 0x00000030u, 0x00030000u, 0x00010000u, 0x00000040u, 0x00000000u, 0x44696666u, 0x75736553u,
    0x616D706Cu, 0x657200ABu, 0x0004000Cu, 0x00010001u, 0x00010000u, 0x00000000u, 0x70735F33u, 0x5F300032u,
    0x2E302E36u, 0x3533342Eu, 0x3100ABABu, 0x00000000u, 0x0000003Cu, 0x18000000u, 0x00000004u, 0x00000000u,
    0x00000821u, 0x00010001u, 0x00000011u, 0x00003050u, 0x00000000u, 0x3001C400u, 0x22000009u, 0x10080001u,
    0x1F1FFFF8u, 0x00004000u, 0x2010C000u, 0x0100006Cu, 0xE2000080u, 0x1610803Du, 0x0000006Cu, 0xE2000000u,
    0x00000000u, 0x00000000u, 0x00000000u,
};

// ARTIST 8203DCF0, 400 bytes, unchanged native shader data.
const u32 kaCompositeBlitVertexProgram[] =
{
    0x102A1101u, 0x00000118u, 0x00000078u, 0x00000000u, 0x00000024u, 0x00000000u, 0x000000E0u, 0x00000000u,
    0x00000000u, 0x000000B8u, 0x0000001Cu, 0x000000ADu, 0xFFFE0300u, 0x00000004u, 0x0000001Cu, 0x00000008u,
    0x000000A6u, 0x0000006Cu, 0x00020000u, 0x00010000u, 0x00000078u, 0x00000000u, 0x00000088u, 0x00020001u,
    0x00010000u, 0x00000078u, 0x00000000u, 0x00000091u, 0x00020002u, 0x00010000u, 0x00000078u, 0x00000000u,
    0x0000009Cu, 0x00020003u, 0x00010000u, 0x00000078u, 0x00000000u, 0x674F6666u, 0x73657458u, 0x595A00ABu,
    0x00010003u, 0x00010004u, 0x00010000u, 0x00000000u, 0x67526967u, 0x68745570u, 0x00675556u, 0x4F666673u,
    0x65747300u, 0x67555653u, 0x63616C65u, 0x73007673u, 0x5F335F30u, 0x00322E30u, 0x2E363533u, 0x342E3100u,
    0x00000000u, 0x00000078u, 0x00010000u, 0x00000000u, 0x00000000u, 0x00001021u, 0x00000001u, 0x00000002u,
    0x00000001u, 0x00000290u, 0x00000003u, 0x00005007u, 0x0000F050u, 0x00001008u, 0x10011003u, 0x00001200u,
    0xC2000000u, 0x00003004u, 0x00001200u, 0xC4000000u, 0x10092007u, 0x00002200u, 0x00000000u, 0x05F80000u,
    0x00000E47u, 0x00000000u, 0x14C8C03Eu, 0x000000C6u, 0xC2000000u, 0xC801803Eu, 0x00B26E6Cu, 0x91000100u,
    0xC802803Eu, 0x00B2B3B1u, 0x91000100u, 0x05F80000u, 0x00000FC8u, 0x00000000u, 0xC80F8000u, 0x00A00000u,
    0x8B000302u, 0x00000000u, 0x00000000u, 0x00000000u,
};

// ARTIST 8203DE80, 508 bytes, unchanged native shader data.
const u32 kaCompositeBlitPixelProgram[] =
{
    0x102A1100u, 0x0000012Cu, 0x000000D0u, 0x00000000u, 0x00000024u, 0x000000E0u, 0x00000108u, 0x00000000u,
    0x00000000u, 0x000000B8u, 0x0000001Cu, 0x000000ABu, 0xFFFF0300u, 0x00000003u, 0x0000001Cu, 0x00000008u,
    0x000000A4u, 0x00000058u, 0x00030000u, 0x00010000u, 0x00000064u, 0x00000000u, 0x00000074u, 0x00030001u,
    0x00010000u, 0x00000064u, 0x00000000u, 0x00000083u, 0x00020000u, 0x00010000u, 0x00000094u, 0x00000000u,
    0x42617365u, 0x53616D70u, 0x6C657200u, 0x0004000Cu, 0x00010001u, 0x00010000u, 0x00000000u, 0x4F766572u,
    0x6C617953u, 0x616D706Cu, 0x65720067u, 0x5175696Eu, 0x63756E78u, 0x4F666673u, 0x65747300u, 0x00010003u,
    0x00010004u, 0x00010000u, 0x00000000u, 0x70735F33u, 0x5F300032u, 0x2E302E36u, 0x3533342Eu, 0x3100ABABu,
    0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000014u, 0x01FC0010u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000040u, 0x00000090u, 0x10000200u, 0x00000004u, 0x00000000u, 0x00001021u,
    0x00010001u, 0x00000001u, 0x0000F050u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6002C400u, 0x12000950u, 0x00003008u, 0x00002200u,
    0x00000000u, 0xC8030001u, 0x00B0B000u, 0xA0000000u, 0xC80C0001u, 0x02ACAC00u, 0xA0000000u, 0xB8180001u,
    0x1F1FF688u, 0x00004000u, 0xB8082021u, 0x1F1FFE88u, 0x00004000u, 0x10081021u, 0x1F1FFE88u, 0x00004000u,
    0xC8070001u, 0x00C0C000u, 0xE0020100u, 0xC8070001u, 0x00C06C00u, 0xA101FF00u, 0xC8070000u, 0x00C0C000u,
    0xE0010000u, 0xC88FC000u, 0x04C01BC0u, 0xEB010000u, 0x00000000u, 0x00000000u, 0x00000000u,
};

renderengine::ProgramBufferData* CreateBlitProgram(rw::IResourceAllocator* allocator,
    const void* microcode, u32 size, u32 shaderType)
{
    renderengine::ProgramBufferParameters parameters;
    parameters.muFunction = reinterpret_cast<u32>(microcode);
    parameters.muShaderType = shaderType;
    parameters.muReserved8 = size;
    parameters.muMicrocodePart1 = 0;
    parameters.muMicrocodePart3 = 0;
    parameters.muConstantTableSize = 0;
    parameters.muNumVariables = 0;
    rw::ResourceDescriptor descriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&descriptor, &parameters);
    rw::Resource resource = allocator->DoAllocate(descriptor, 0);
    return renderengine::ProgramBuffer::Initialize(
        reinterpret_cast<renderengine::ProgramResourceLayout*>(&resource), &parameters);
}
}

// ARTIST823FCA38. Device parameters are passed by value using the original
// seven GPR doublewords,44stack bytes, allocatorSP+8C and boolSP+97.
void BrnRendererMemory::Construct(renderengine::Device::Parameters parameters,
    rw::IResourceAllocator* allocator, bool enableMSAA)
{
    mu8TileIndex = 0;
    muTileCompressionAddress = 0;
    mu8ZCullIndex = 0;
    muZCullAddress = 0;
    const u32 width = parameters.GetFrontBufferWidth();
    const u32 height = parameters.GetFrontBufferHeight();
    mu32ScreenWidth = width;
    mu32ScreenHeight = height;
    for (u32 i = 0; i != E_RENDER_TARGET_COUNT; ++i) mapRenderTarget[i] = 0;

    CreateAntiAliasBuffer(allocator, enableMSAA);
    CreateDownSampleBuffer(allocator);
    CreateShadowmapBuffer(allocator);
    CreateBloomBuffer(allocator);
    CreateDepthOfFieldBuffer(allocator);
    CreateWorkBuffer(allocator);
    CreateEnvmapBuffer(allocator);
    CreateParticleBuffer(allocator);
    CreateSunCoronaBuffer(allocator);
    CreateBackBuffer(allocator, width, height);

    mpDepthBlitVertexProgramBuffer = CreateBlitProgram(allocator, kaDepthBlitVertexProgram,
        sizeof(kaDepthBlitVertexProgram), 0);
    mpDepthBlitPixelProgramBuffer = CreateBlitProgram(allocator, kaDepthBlitPixelProgram,
        sizeof(kaDepthBlitPixelProgram), 1);
    mpCompositeBlitVertexProgramBuffer = CreateBlitProgram(allocator, kaCompositeBlitVertexProgram,
        sizeof(kaCompositeBlitVertexProgram), 0);
    mpCompositeBlitPixelProgramBuffer = CreateBlitProgram(allocator, kaCompositeBlitPixelProgram,
        sizeof(kaCompositeBlitPixelProgram), 1);

    rw::ResourceDescriptor descriptor;
    renderengine::TextureState::GetResourceDescriptor(&descriptor);
    rw::IResourceAllocator* const graphicsAllocator = BrnResource::GetGraphicsAllocator();
    mBlitTextureStateResource = graphicsAllocator->DoAllocate(descriptor, 0);
    mpBlitTextureState = 0;
}

void BrnRendererMemory::CreateShadowmapBuffer(rw::IResourceAllocator* lpAllocator)
{
    const u32 luWidth  = KU_SHADOW_MAP_WIDTH;
    const u32 luHeight = KU_SHADOW_MAP_COMBINED_HEIGHT / KU_SHADOW_MAP_NUM_CASCADES;

    CgsRenderTarget* lpShadowMap = new CgsRenderTarget();
    mapRenderTarget[E_RENDER_TARGET_SHADOW_MAP_0] = lpShadowMap;

    lpShadowMap->ClearColourTargetInUse();

    lpShadowMap->SetDimensions(luWidth, luHeight);
    lpShadowMap->SetNumMipMaps(1);
    lpShadowMap->SetMultisampleFormat(0);

    lpShadowMap->SetNumSections(static_cast<u8>(KU_SHADOW_MAP_NUM_CASCADES));
    lpShadowMap->SetUseDepthStencilAsTexture(true);

    lpShadowMap->SetDepthTargetInUse(true);
    lpShadowMap->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpShadowMap->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);
    lpShadowMap->SetDepthTargetBaseEDRAM(0);

    lpShadowMap->SetDepthTargetTileIndex(0);

    lpShadowMap->Construct(lpAllocator);
}

void BrnRendererMemory::CreateBackBuffer(rw::IResourceAllocator* lpAllocator, u32 luWidth, u32 luHeight)
{
    CgsRenderTarget* lpBackBuffer = new CgsRenderTarget();

    lpBackBuffer->ClearColourTargetInUse();

    lpBackBuffer->SetDimensions(luWidth, luHeight);
    lpBackBuffer->SetNumMipMaps(1);
    lpBackBuffer->SetMultisampleFormat(0);

    lpBackBuffer->SetUseDepthStencilAsTexture(false);

    lpBackBuffer->SetColourFilterMode(0, 1);
    lpBackBuffer->SetColourTargetInUse(0, true);
    lpBackBuffer->SetColourTargetUseDevice(0, true);
    lpBackBuffer->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBackBuffer->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpBackBuffer->SetColourTargetBaseEDRAM(0, 0);

    lpBackBuffer->SetDepthTargetInUse(false);

    lpBackBuffer->Construct(lpAllocator);

    if (!mapRenderTarget[E_RENDER_TARGET_DOWN_SAMPLE])
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("GetDownSampleBuffer() != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererMemory.cpp", 856);
        CgsDev::Assert::EndAssert();
    }

    rw::graphics::postfx::RenderTarget* lpBackBufferTarget = lpBackBuffer->GetRenderTarget();
    rw::graphics::postfx::RenderTarget* lpDownSampleTarget =
        mapRenderTarget[E_RENDER_TARGET_DOWN_SAMPLE]->GetRenderTarget();
    typedef rw::graphics::postfx::RenderTarget NativeRenderTarget;
    typedef rw::graphics::postfx::Target NativeTarget;
    CGS_STATIC_ASSERT((offsetof(NativeRenderTarget, maColourTargets) +
        offsetof(NativeTarget, mpTextureState) == 0x2C), "ARTIST shared colour sampler");
    CGS_STATIC_ASSERT((offsetof(NativeRenderTarget, mpSection0State) == 0x1C), "ARTIST section-zero state");

    // ARTIST823F7064..7080 aliases the down-sample colour sampling state.
    // This is a Target member, not a fifth entry in a section-state array.
    lpBackBufferTarget->maColourTargets[0].mpTextureState =
        lpDownSampleTarget->maColourTargets[0].mpTextureState;
    lpBackBufferTarget->mpSection0State = const_cast<renderengine::RenderTargetState*>(
        renderengine::Device::GetDefaultRenderTargetState());

    mapRenderTarget[E_RENDER_TARGET_BACK_BUFFER] = lpBackBuffer;
}

CgsRenderTarget* BrnRendererMemory::GetShadowMapBuffer(s32 liIndex)
{
    if (static_cast<u32>(liIndex) >= 4)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("liIndex >=0 && liIndex<4",
            "..\\..\\..\\GameSource\\Graphics/BrnRendererMemory.h", 359);
        CgsDev::Assert::EndAssert();
    }
    return mapRenderTarget[liIndex + 1];
}

void BrnRendererMemory::CreateAntiAliasBuffer(rw::IResourceAllocator* lpAllocator, bool lbEnableMSAA)
{

    CgsRenderTarget* lpAntiAlias = new CgsRenderTarget();
    mapRenderTarget[E_RENDER_TARGET_ANTI_ALIAS] = lpAntiAlias;

    lpAntiAlias->ClearColourTargetInUse();

    lpAntiAlias->SetDimensions(mu32ScreenWidth, mu32ScreenHeight);
    lpAntiAlias->SetNumMipMaps(1);

    lpAntiAlias->SetColourTargetInUse(0, true);
    lpAntiAlias->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpAntiAlias->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpAntiAlias->SetColourTargetBaseEDRAM(0, 0);

    lpAntiAlias->SetUseDepthStencilAsTexture(false);
    lpAntiAlias->SetDepthTargetInUse(true);
    lpAntiAlias->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpAntiAlias->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);

    const BrnGraphics::AntiAliasTilingPlan& lrPlan = lbEnableMSAA ? BrnGraphics::KMSAA_TILING_PLAN : BrnGraphics::KNO_MSAA_TILING_PLAN;
    lpAntiAlias->SetMultisampleFormat(lrPlan.mn32MultiSampleFormat);

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

    lpAntiAlias->Construct(lpAllocator);
}

void BrnRendererMemory::CreateDownSampleBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpDownSample = new CgsRenderTarget();

    lpDownSample->ClearColourTargetInUse();

    lpDownSample->SetDimensions(mu32ScreenWidth, mu32ScreenHeight);
    lpDownSample->SetNumMipMaps(1);

    lpDownSample->SetMultisampleFormat(0);

    lpDownSample->SetColourTargetInUse(0, true);
    lpDownSample->SetColourFilterMode(0, 1);
    lpDownSample->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpDownSample->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpDownSample->SetColourTargetBaseEDRAM(0, 0);

    lpDownSample->SetUseDepthStencilAsTexture(true);
    lpDownSample->SetDepthTargetInUse(true);
    lpDownSample->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpDownSample->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);

    lpDownSample->SetDepthTargetBaseEDRAM(CountEDRAMTiles(mu32ScreenWidth, mu32ScreenHeight, KU_EDRAM_TILE_HEIGHT));

    lpDownSample->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_DOWN_SAMPLE] = lpDownSample;
}

void BrnRendererMemory::CreateWorkBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpWorkBuffer = new CgsRenderTarget();

    lpWorkBuffer->ClearColourTargetInUse();

    lpWorkBuffer->SetDimensions(KU_WORK_BUFFER_WIDTH, KU_WORK_BUFFER_HEIGHT);
    lpWorkBuffer->SetNumMipMaps(1);
    lpWorkBuffer->SetMultisampleFormat(0);
    lpWorkBuffer->SetUseDepthStencilAsTexture(false);

    lpWorkBuffer->SetColourTargetInUse(0, true);

    lpWorkBuffer->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpWorkBuffer->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpWorkBuffer->SetColourTargetBaseEDRAM(0, 0);

    lpWorkBuffer->SetDepthTargetInUse(false);

    lpWorkBuffer->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_WORK] = lpWorkBuffer;
}

void BrnRendererMemory::CreateEnvmapBuffer(rw::IResourceAllocator* lpAllocator)
{

    mapRenderTarget[E_RENDER_TARGET_ENV_MAP] = new CgsRenderTarget();
    CgsRenderTarget* lpEnvMap = mapRenderTarget[E_RENDER_TARGET_ENV_MAP];

    lpEnvMap->ClearColourTargetInUse();

    lpEnvMap->SetDimensions(KU_ENV_MAP_FACE_SIZE, KU_ENV_MAP_FACE_SIZE);
    lpEnvMap->SetNumMipMaps(1);
    lpEnvMap->SetMultisampleFormat(KI_ENV_MAP_MULTISAMPLE_FORMAT);
    lpEnvMap->SetUseDepthStencilAsTexture(true);

    lpEnvMap->SetColourTargetInUse(0, true);
    lpEnvMap->SetTextureType(0, renderengine::Texture::TYPE_CUBE);
    lpEnvMap->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpEnvMap->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpEnvMap->SetColourTargetBaseEDRAM(0, 0);

    lpEnvMap->SetDepthTargetInUse(true);
    lpEnvMap->SetDepthTargetBufferFormat(KU_ENV_MAP_DEPTH_FORMAT);
    lpEnvMap->SetDepthTargetTextureFormat(KU_ENV_MAP_DEPTH_FORMAT);
    lpEnvMap->SetDepthTargetBaseEDRAM(KU_ENV_MAP_DEPTH_BASE_EDRAM_TILES);

    lpEnvMap->Construct(lpAllocator);
}

void BrnRendererMemory::CreateBloomBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpBloom = new CgsRenderTarget();

    lpBloom->ClearColourTargetInUse();

    lpBloom->SetDimensions(KU_BLOOM_BUFFER_WIDTH, KU_BLOOM_BUFFER_HEIGHT);
    lpBloom->SetNumMipMaps(1);
    lpBloom->SetMultisampleFormat(0);
    lpBloom->SetUseDepthStencilAsTexture(false);

    lpBloom->SetColourTargetInUse(0, true);
    lpBloom->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBloom->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBloom->SetColourTargetBaseEDRAM(0, 0);

    lpBloom->SetDepthTargetInUse(false);

    lpBloom->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_BLOOM] = lpBloom;
}

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

    mapRenderTarget[E_RENDER_TARGET_DEPTH_OF_FIELD] = lpDepthOfField;
}

void BrnRendererMemory::CreateParticleBuffer(rw::IResourceAllocator* lpAllocator)
{

    const u32 luWidth  = mu32ScreenWidth  / 2u;
    const u32 luHeight = mu32ScreenHeight / 2u;

    const u32 luColourSurfaceTiles =
        CountEDRAMTiles(luWidth, luHeight, KU_EDRAM_PARTICLE_ROW_ALIGNMENT);

    CgsRenderTarget* lpParticle = new CgsRenderTarget();

    lpParticle->ClearColourTargetInUse();

    lpParticle->SetDimensions(luWidth, luHeight);
    lpParticle->SetNumMipMaps(1);
    lpParticle->SetMultisampleFormat(0);

    lpParticle->SetUseDepthStencilAsTexture(true);

    lpParticle->SetColourTargetInUse(0, true);
    lpParticle->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpParticle->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpParticle->SetColourTargetBaseEDRAM(0, 0);

    lpParticle->SetDepthTargetInUse(true);
    lpParticle->SetDepthTargetBufferFormat(KU_DEPTH_SURFACE_FORMAT);
    lpParticle->SetDepthTargetTextureFormat(KU_DEPTH_SURFACE_FORMAT);
    lpParticle->SetDepthTargetBaseEDRAM(luColourSurfaceTiles);

    lpParticle->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_PARTICLE] = lpParticle;
}

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

    lpSunCorona->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_SUN_CORONA] = lpSunCorona;
}
