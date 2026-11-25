#include "GameSource/Graphics/BrnRendererMemory.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"
#include "GameSource/Graphics/BrnAntiAliasTiling.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"

// ARTIST 82F2423E has initial image value 1. Both native blits read this byte;
// the descriptive name records its role where no original symbol survived.
bool gBlitHalfPixelOffset82F2423E = true;

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
    parameters.mpFunction = microcode;
    parameters.miShaderType = shaderType;
    parameters.muProgramSize = size;
    parameters.mMicrocodeParts.cbCachedPartSize = 0;
    parameters.mMicrocodeParts.cbPhysicalPartSize = 0;
    parameters.muConstantTableSize = 0;
    parameters.muNumVariables = 0;
    const rw::ResourceDescriptor descriptor =
        renderengine::ProgramBuffer::GetResourceDescriptor(parameters);
    rw::Resource resource = allocator->DoAllocate(descriptor, 0);
    return renderengine::ProgramBuffer::Initialize(
        &resource, &parameters);
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

    renderengine::TextureState::Parameters textureStateParameters;
    const rw::ResourceDescriptor descriptor = renderengine::TextureState::GetResourceDescriptor(textureStateParameters);
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
    lpBackBuffer->SetUseDepthStencilAsTexture(false);
    lpBackBuffer->SetNumMipMaps(1);
    lpBackBuffer->SetColourTargetInUse(0, true);
    lpBackBuffer->SetDepthTargetInUse(false);
    lpBackBuffer->SetColourFilterMode(0, 1);
    lpBackBuffer->SetMultisampleFormat(0);
    lpBackBuffer->SetColourTargetUseDevice(0, true);
    lpBackBuffer->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBackBuffer->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    lpBackBuffer->SetColourTargetBaseEDRAM(0, 0);

    lpBackBuffer->Construct(lpAllocator);

    if (GetDownSampleBuffer() == NULL)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("GetDownSampleBuffer() != NULL",
            CGS_ASSERT_LOCATION("d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnRendererMemory.cpp", 856));
        CgsDev::Assert::EndAssert();
    }

    rw::graphics::postfx::RenderTarget* lpBackBufferTarget = lpBackBuffer->GetRenderTarget();
    rw::graphics::postfx::RenderTarget* lpDownSampleTarget =
        GetDownSampleBuffer()->GetRenderTarget();
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
    // ARTIST 0x823F492C/0x823F4934 test both bounds signed (cmpwi 0 ; cmpwi 4), the assert text.
    if (!(liIndex >= 0 && liIndex < 4))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("liIndex >=0 && liIndex<4",
            CGS_ASSERT_LOCATION("..\\..\\..\\GameSource\\Graphics/BrnRendererMemory.h", 359));
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

    // Same store order as CreateBloomBuffer (ARTIST @0x823F71A4 / @0x823F725C).
    lpWorkBuffer->SetUseDepthStencilAsTexture(false);
    lpWorkBuffer->SetDimensions(KU_WORK_BUFFER_WIDTH, KU_WORK_BUFFER_HEIGHT);
    lpWorkBuffer->SetNumMipMaps(1);
    lpWorkBuffer->SetColourTargetInUse(0, true);
    lpWorkBuffer->SetDepthTargetInUse(false);
    lpWorkBuffer->SetMultisampleFormat(0);
    lpWorkBuffer->SetColourTargetBaseEDRAM(0, 0);
    lpWorkBuffer->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpWorkBuffer->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);

    lpWorkBuffer->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_WORK] = lpWorkBuffer;
}

void BrnRendererMemory::CreateEnvmapBuffer(rw::IResourceAllocator* lpAllocator)
{
    mapRenderTarget[E_RENDER_TARGET_ENV_MAP] = new CgsRenderTarget();
    GetEnvMapBuffer()->ClearColourTargetInUse();
    GetEnvMapBuffer()->SetDimensions(KU_ENV_MAP_FACE_SIZE, KU_ENV_MAP_FACE_SIZE);
    GetEnvMapBuffer()->SetNumMipMaps(1);
    GetEnvMapBuffer()->SetUseDepthStencilAsTexture(true);
    GetEnvMapBuffer()->SetColourTargetInUse(0, true);
    GetEnvMapBuffer()->SetDepthTargetInUse(true);
    GetEnvMapBuffer()->SetTextureType(0, renderengine::Texture::TYPE_CUBE);
    GetEnvMapBuffer()->SetMultisampleFormat(KI_ENV_MAP_MULTISAMPLE_FORMAT);
    GetEnvMapBuffer()->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    GetEnvMapBuffer()->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_TEXTURE_FORMAT);
    GetEnvMapBuffer()->SetColourTargetBaseEDRAM(0, 0);
    GetEnvMapBuffer()->SetDepthTargetBufferFormat(KU_ENV_MAP_DEPTH_FORMAT);
    GetEnvMapBuffer()->SetDepthTargetTextureFormat(KU_ENV_MAP_DEPTH_FORMAT);
    GetEnvMapBuffer()->SetDepthTargetBaseEDRAM(KU_ENV_MAP_DEPTH_BASE_EDRAM_TILES);
    GetEnvMapBuffer()->Construct(lpAllocator);
}

void BrnRendererMemory::CreateBloomBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpBloom = new CgsRenderTarget();

    lpBloom->ClearColourTargetInUse();

    // Statement order follows ARTIST's store order (identical in CreateBloomBuffer @0x823F70EC,
    // CreateWorkBuffer @0x823F71A4 and CreateDepthOfFieldBuffer @0x823F725C): +0x15, the
    // dimensions and mip count, the colour section, +0xDC, +0x10, then the EDRAM base and formats.
    lpBloom->SetUseDepthStencilAsTexture(false);
    lpBloom->SetDimensions(KU_BLOOM_BUFFER_WIDTH, KU_BLOOM_BUFFER_HEIGHT);
    lpBloom->SetNumMipMaps(1);
    lpBloom->SetColourTargetInUse(0, true);
    lpBloom->SetDepthTargetInUse(false);
    lpBloom->SetMultisampleFormat(0);
    lpBloom->SetColourTargetBaseEDRAM(0, 0);
    lpBloom->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpBloom->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);

    lpBloom->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_BLOOM] = lpBloom;
}

void BrnRendererMemory::CreateDepthOfFieldBuffer(rw::IResourceAllocator* lpAllocator)
{
    CgsRenderTarget* lpDepthOfField = new CgsRenderTarget();

    lpDepthOfField->ClearColourTargetInUse();

    // Same store order as CreateBloomBuffer (ARTIST @0x823F71A4 / @0x823F725C).
    lpDepthOfField->SetUseDepthStencilAsTexture(false);
    lpDepthOfField->SetDimensions(KU_DEPTH_OF_FIELD_BUFFER_WIDTH, KU_DEPTH_OF_FIELD_BUFFER_HEIGHT);
    lpDepthOfField->SetNumMipMaps(1);
    lpDepthOfField->SetColourTargetInUse(0, true);
    lpDepthOfField->SetDepthTargetInUse(false);
    lpDepthOfField->SetMultisampleFormat(0);
    lpDepthOfField->SetColourTargetBaseEDRAM(0, 0);
    lpDepthOfField->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpDepthOfField->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);

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

    // Statement order follows ARTIST's store order (0x823F7424..0x823F7458): +0x15, the
    // dimensions and mip count, the colour section, +0xDC, +0x10, the formats, then the EDRAM base.
    lpSunCorona->SetUseDepthStencilAsTexture(false);
    lpSunCorona->SetDimensions(KU_SUN_CORONA_BUFFER_EXTENT, KU_SUN_CORONA_BUFFER_EXTENT);
    lpSunCorona->SetNumMipMaps(1);
    lpSunCorona->SetColourTargetInUse(0, true);
    lpSunCorona->SetDepthTargetInUse(false);
    lpSunCorona->SetMultisampleFormat(0);
    lpSunCorona->SetColourTargetBufferFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpSunCorona->SetColourTargetTextureFormat(0, KU_COLOUR_SURFACE_BUFFER_FORMAT);
    lpSunCorona->SetColourTargetBaseEDRAM(0, 0);

    lpSunCorona->Construct(lpAllocator);

    mapRenderTarget[E_RENDER_TARGET_SUN_CORONA] = lpSunCorona;
}

// ARTIST 82406A68. The two quincunx booleans are not read by this X360 body.
void BrnRendererMemory::BlitComposite(CgsGraphics::Im2d& im2d,
    CgsRenderTarget* sourceBase, CgsRenderTarget* sourceOverlay, bool, bool)
{
    using namespace CgsGraphics;
    typedef rw::math::vpu::Vector4 Vector4;
    rw::graphics::postfx::RenderTarget* const base = sourceBase->GetRenderTarget();
    rw::graphics::postfx::RenderTarget* const overlay = sourceOverlay->GetRenderTarget();
    const renderengine::TextureState* const baseState = base->maColourTargets[0].mpTextureState;
    const renderengine::TextureState* const overlayState = overlay->maColourTargets[0].mpTextureState;
    const Im2dTransform transform = Im2dTransform::GetDefault();
    im2d.BeginRendering();
    im2d.SetTransform(transform);
    im2d.SetState(baseState, 0);
    im2d.SetState(overlayState, 1);

    const renderengine::ProgramBufferData* const vertexProgram = mpCompositeBlitVertexProgramBuffer;
    const renderengine::ProgramBufferData* const pixelProgram = mpCompositeBlitPixelProgramBuffer;
    shadow::Device::SetVertexProgram(vertexProgram);
    shadow::Device::SetPixelProgram(pixelProgram);

    Basic2dColouredTexturedVertex vertices[4] =
    {
        { { 0.0f, 0.0f }, { 255, 255, 255, 255 }, { 0.0f, 0.0f } },
        { { 0.0f, 1.0f }, { 255, 255, 255, 255 }, { 0.0f, 1.0f } },
        { { 1.0f, 0.0f }, { 255, 255, 255, 255 }, { 1.0f, 0.0f } },
        { { 1.0f, 1.0f }, { 255, 255, 255, 255 }, { 1.0f, 1.0f } }
    };
    Vector4 uvOffsets(0.0f, 0.0f, 0.0f, 0.0f);
    const Vector4 uvScales(1.0f, 1.0f, 1.0f, 1.0f);
    if (gBlitHalfPixelOffset82F2423E)
    {
        uvOffsets.SetX((1.0f / static_cast<f32>(base->muWidth)) * 0.5f);
        uvOffsets.SetY((1.0f / static_cast<f32>(base->muHeight)) * 0.5f);
        uvOffsets.SetZ((1.0f / static_cast<f32>(overlay->muWidth)) * 0.5f);
        uvOffsets.SetW((1.0f / static_cast<f32>(overlay->muHeight)) * 0.5f);
    }

    renderengine::ProgramVariableHandle offsetsHandle;
    renderengine::ProgramVariableHandle scalesHandle;
    renderengine::ProgramBuffer::GetVariableHandleByName(mpCompositeBlitVertexProgramBuffer,
        reinterpret_cast<const u8*>("gUVOffsets"), &offsetsHandle);
    renderengine::ProgramBuffer::GetVariableHandleByName(mpCompositeBlitVertexProgramBuffer,
        reinterpret_cast<const u8*>("gUVScales"), &scalesHandle);
    renderengine::FloatShaderStateIterator iterator;
    renderengine::Device::BeginShaderStates(offsetsHandle, iterator);
    *iterator.m_dest++ = uvOffsets;
    renderengine::Device::BeginShaderStates(scalesHandle, iterator);
    *iterator.m_dest++ = uvScales;

    const f32 inverseWidth = 1.0f / static_cast<f32>(base->muWidth);
    const f32 inverseHeight = 1.0f / static_cast<f32>(base->muHeight);
    const Vector4 quincunxOffsets(
        0.166f * inverseWidth, -0.166f * inverseHeight,
        0.0f * inverseWidth, 0.0f * inverseHeight
    );
    renderengine::ProgramVariableHandle quincunxHandle;
    renderengine::ProgramBuffer::GetVariableHandleByName(mpCompositeBlitPixelProgramBuffer,
        reinterpret_cast<const u8*>("gQuincunxOffsets"), &quincunxHandle);
    renderengine::Device::BeginShaderStates(quincunxHandle, iterator);
    *iterator.m_dest = quincunxOffsets;

    im2d.Render(static_cast<renderengine::PrimitiveType>(D3DPT_TRIANGLESTRIP), vertices, 4);
    im2d.EndRendering();
    shadow::Device::ResetShadowing();
}

// ARTIST 82406EA8. Reinitializes the shared blit sampler for the live depth texture.
void BrnRendererMemory::BlitDepth(CgsGraphics::Im2d& im2d, CgsRenderTarget* source)
{
    using namespace CgsGraphics;
    rw::graphics::postfx::RenderTarget* const target = source->GetRenderTarget();
    D3DDevice_SetShaderGPRAllocation(::gpD3DDevice, 0, 64, 64);
    const Im2dTransform transform = Im2dTransform::GetDefault();
    im2d.BeginRendering();
    im2d.SetTransform(transform);

    renderengine::TextureState::Parameters parameters;
    parameters.muMagFilter = 0;
    parameters.muMinFilter = 0;
    parameters.mpTexture = target->mDepthTarget.mpTexture;
    mpBlitTextureState = renderengine::TextureState::Initialize(mBlitTextureStateResource, parameters);
    im2d.SetState(mpBlitTextureState);
    const renderengine::ProgramBufferData* const vertexProgram = mpDepthBlitVertexProgramBuffer;
    const renderengine::ProgramBufferData* const pixelProgram = mpDepthBlitPixelProgramBuffer;
    shadow::Device::SetVertexProgram(vertexProgram);
    shadow::Device::SetPixelProgram(pixelProgram);

    f32 offsetX = 0.0f;
    f32 offsetY = 0.0f;
    if (gBlitHalfPixelOffset82F2423E)
    {
        offsetX = (1.0f / static_cast<f32>(target->muWidth)) * 0.5f;
        offsetY = (1.0f / static_cast<f32>(target->muHeight)) * 0.5f;
    }
    Basic2dColouredTexturedVertex vertices[4] =
    {
        { { 0.0f, 0.0f }, { 255, 255, 255, 255 }, { offsetX, offsetY } },
        { { 0.0f, 1.0f }, { 255, 255, 255, 255 }, { offsetX, offsetY + 1.0f } },
        { { 1.0f, 1.0f }, { 255, 255, 255, 255 }, { offsetX + 1.0f, offsetY + 1.0f } },
        { { 1.0f, 0.0f }, { 255, 255, 255, 255 }, { offsetX + 1.0f, offsetY } }
    };
    im2d.Render(static_cast<renderengine::PrimitiveType>(D3DPT_RECTLIST), vertices, 3);
    im2d.EndRendering();
    D3DDevice_SetShaderGPRAllocation(::gpD3DDevice, 0, 0, 0);
    shadow::Device::ResetShadowing();
}
