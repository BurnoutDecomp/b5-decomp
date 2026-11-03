#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include <cstring>

namespace
{
// ARTIST 0x8200F1B8, 228 bytes.
const u32 gaCoronaPixelProgram[] =
{
    0x102A1100u, 0x000000B4u, 0x00000030u, 0x00000000u, 0x00000024u, 0x00000000u,
    0x0000008Cu, 0x00000000u, 0x00000000u, 0x00000064u, 0x0000001Cu, 0x00000057u,
    0xFFFF0300u, 0x00000001u, 0x0000001Cu, 0x00000008u, 0x00000050u, 0x00000030u,
    0x00030000u, 0x00010000u, 0x00000040u, 0x00000000u, 0x636F726Fu, 0x6E615465u,
    0x78747572u, 0x6500ABABu, 0x0004000Cu, 0x00010001u, 0x00010000u, 0x00000000u,
    0x70735F33u, 0x5F300032u, 0x2E302E36u, 0x3533342Eu, 0x3100ABABu, 0x00000000u,
    0x00000030u, 0x10000100u, 0x00000008u, 0x00000000u, 0x00001842u, 0x00010003u,
    0x00000001u, 0x00003050u, 0x0000F1A0u, 0x00000000u, 0x2001C400u, 0x22000009u,
    0x10080001u, 0x1F1FF688u, 0x00004000u, 0xC80F8000u, 0x00000000u, 0xE1000100u,
    0x00000000u, 0x00000000u, 0x00000000u,
};
// ARTIST 0x8200F2A0, 788 bytes.
const u32 gaCoronaVertexProgram[] =
{
    0x102A1101u, 0x0000016Cu, 0x000001A8u, 0x00000000u, 0x00000024u, 0x000000F8u,
    0x00000120u, 0x00000000u, 0x00000000u, 0x000000D0u, 0x0000001Cu, 0x000000C3u,
    0xFFFE0300u, 0x00000003u, 0x0000001Cu, 0x00000008u, 0x000000BCu, 0x00000058u,
    0x00020004u, 0x00010000u, 0x00000078u, 0x00000000u, 0x00000088u, 0x00020000u,
    0x00040000u, 0x000000A0u, 0x00000000u, 0x000000B0u, 0x00020005u, 0x00010000u,
    0x00000078u, 0x00000000u, 0x63616D65u, 0x7261506Fu, 0x73697469u, 0x6F6E506Cu,
    0x75734272u, 0x69676874u, 0x6E657373u, 0x00ABABABu, 0x00010003u, 0x00010004u,
    0x00010000u, 0x00000000u, 0x76696577u, 0x50726F6Au, 0x65637469u, 0x6F6E4D61u,
    0x74726978u, 0x00ABABABu, 0x00020003u, 0x00040004u, 0x00010000u, 0x00000000u,
    0x76696577u, 0x58795363u, 0x616C6500u, 0x76735F33u, 0x5F300032u, 0x2E302E36u,
    0x3533342Eu, 0x3100ABABu, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u,
    0x00000014u, 0x00FC0010u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000040u, 0x00000168u, 0x00110005u, 0x00000000u, 0x00000000u, 0x00001842u,
    0x00000001u, 0x00000004u, 0x00000003u, 0x00000290u, 0x00100004u, 0x00003005u,
    0x00005006u, 0x0030A007u, 0x00003050u, 0x0001F1A0u, 0x0000101Bu, 0x0000001Au,
    0x0000101Cu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0xBFC00000u, 0x3F93CD3Eu, 0x00000000u, 0x00000000u, 0xF0554004u,
    0x00001200u, 0xC2000000u, 0x00006008u, 0x600E1200u, 0x12000000u, 0x00006014u,
    0x00001200u, 0xC4000000u, 0x0000301Au, 0x00002200u, 0x00000000u, 0x05F82000u,
    0x00000688u, 0x00000000u, 0x05F83000u, 0x00000447u, 0x00000000u, 0x05F81000u,
    0x00000688u, 0x00000000u, 0x05F80000u, 0x00000688u, 0x00000000u, 0xC8070004u,
    0x04C0C000u, 0xA0020400u, 0xC8010003u, 0x00BEBE00u, 0xF0040400u, 0x58100300u,
    0x0000006Cu, 0xE2000083u, 0xC8070004u, 0x00C06C00u, 0xE1040300u, 0xC8070005u,
    0x00C01BC0u, 0xEB040202u, 0xC80F0002u, 0x00C6F8F8u, 0x8B050203u, 0xC80F0002u,
    0x00B1D034u, 0xAB050102u, 0xC80F0002u, 0x006C3EFFu, 0xAB050002u, 0xC80C803Eu,
    0x002C2C00u, 0xE2020200u, 0xC8010003u, 0x041B6CC6u, 0xAB02FF02u, 0x08240303u,
    0x00BEC36Cu, 0xF0040303u, 0xC8020003u, 0x00B1B1B1u, 0xEB020203u, 0x59210302u,
    0x00C6B1B1u, 0xA103FF83u, 0xC8010003u, 0x006CB100u, 0xE1030300u, 0xC8020003u,
    0x00B1B100u, 0xE1020300u, 0xC80F0003u, 0x00BC0700u, 0xE1030100u, 0x00120303u,
    0x02B11B66u, 0xE0030303u, 0xC803803Eu, 0x00B0B0C5u, 0xAB030502u, 0xC8088001u,
    0x006C1B00u, 0xE1020000u, 0xC8038000u, 0x00B0B000u, 0xE2010100u, 0xC8078001u,
    0x00C01B00u, 0xA1000400u, 0x00000000u, 0x00000000u, 0x00000000u,
};

rw::Resource gsPixelProgramResource;
rw::Resource gsVertexProgramResource;
rw::Resource gsVertexDescriptorResource;

struct CoronaVertex
{
    f32 position[4];
    f32 direction[3];
    f32 corner[4];
    renderengine::RGBA8 colour;
};
CGS_STATIC_ASSERT((sizeof(CoronaVertex) == 48), "ARTIST corona vertex stride");
}

namespace renderengine
{
    u32                       CoronaRenderer::s_numSubImages               = 0;
    bool                      CoronaRenderer::s_beginLock                  = false;
    bool                      CoronaRenderer::s_beginBatchLock             = false;
    VertexDescriptorData*     CoronaRenderer::s_vertexDescriptor           = 0;
    ProgramBufferData*        CoronaRenderer::s_pixelProgram               = 0;
    ProgramBufferData*        CoronaRenderer::s_vertexProgram              = 0;
    ProgramVariableHandle     CoronaRenderer::s_cameraPositionHandle       = { 0, 0, 0, 0 };
    ProgramVariableHandle     CoronaRenderer::s_viewProjectionMatrixHandle = { 0, 0, 0, 0 };
    ProgramVariableHandle     CoronaRenderer::s_viewXyScaleHandle          = { 0, 0, 0, 0 };
    const Vector2*            CoronaRenderer::s_atlasUVs                   = 0;
    const TextureState*       CoronaRenderer::s_textureState               = 0;
    const BlendMaterialState* CoronaRenderer::s_blendState                 = 0;
    const DepthStencilState*  CoronaRenderer::s_enabledZTestState          = 0;
    const DepthStencilState*  CoronaRenderer::s_disabledZTestState         = 0;


void CoronaRenderer::Initialize(rw::IResourceAllocator& lrAllocator)
{
    ProgramBufferParameters lProgram;
    lProgram.muFunction = reinterpret_cast<u32>(gaCoronaPixelProgram);
    lProgram.muShaderType = 1;
    lProgram.muReserved8 = sizeof(gaCoronaPixelProgram);
    lProgram.muMicrocodePart1 = 0;
    lProgram.muMicrocodePart3 = 0;
    lProgram.muConstantTableSize = 0;
    lProgram.muNumVariables = 0;
    rw::ResourceDescriptor lDescriptor;
    ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lProgram);
    gsPixelProgramResource = lrAllocator.DoAllocate(lDescriptor, 0);
    s_pixelProgram = ProgramBuffer::Initialize(&gsPixelProgramResource, &lProgram);

    lProgram.muFunction = reinterpret_cast<u32>(gaCoronaVertexProgram);
    lProgram.muShaderType = 0;
    lProgram.muReserved8 = sizeof(gaCoronaVertexProgram);
    ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lProgram);
    gsVertexProgramResource = lrAllocator.DoAllocate(lDescriptor, 0);
    s_vertexProgram = ProgramBuffer::Initialize(&gsVertexProgramResource, &lProgram);
    ProgramBuffer::GetVariableHandleByName(s_vertexProgram, reinterpret_cast<const u8*>("viewProjectionMatrix"), &s_viewProjectionMatrixHandle);
    ProgramBuffer::GetVariableHandleByName(s_vertexProgram, reinterpret_cast<const u8*>("cameraPositionPlusBrightness"), &s_cameraPositionHandle);
    ProgramBuffer::GetVariableHandleByName(s_vertexProgram, reinterpret_cast<const u8*>("viewXyScale"), &s_viewXyScaleHandle);

    VertexDescriptor::Parameters lVertexDecl;
    lVertexDecl.maElements[0].mu16Stream = 0;
    lVertexDecl.maElements[0].miOffset = 0x1A23A6;
    lVertexDecl.maElements[0].mu8UsageIndex = 1;
    lVertexDecl.maElements[1].mu16Stream = 0;
    lVertexDecl.maElements[1].miOffset = 0x2A23B9;
    lVertexDecl.maElements[1].mu8UsageIndex = 3;
    lVertexDecl.maElements[2].mu16Stream = 0;
    lVertexDecl.maElements[2].miOffset = 0x1A23A6;
    lVertexDecl.maElements[2].mu8UsageIndex = 6;
    lVertexDecl.maElements[3].mu16Stream = 0;
    lVertexDecl.maElements[3].miOffset = 0x14C86;
    lVertexDecl.maElements[3].mu8UsageIndex = 4;
    VertexDescriptor::GetResourceDescriptor(&lDescriptor, &lVertexDecl);
    gsVertexDescriptorResource = lrAllocator.DoAllocate(lDescriptor, 0);
    s_vertexDescriptor = VertexDescriptor::Initialize(&gsVertexDescriptorResource, &lVertexDecl);
}

void CoronaRenderer::SetBlendState(const BlendMaterialState* lpState) { s_blendState = lpState; }
void CoronaRenderer::SetDepthStencilStates(const DepthStencilState* lpEnabled, const DepthStencilState* lpDisabled)
{
    s_enabledZTestState = lpEnabled;
    s_disabledZTestState = lpDisabled;
}
void CoronaRenderer::SetTextureAtlas(const TextureState* lpState, u32 luCount, const Vector2* lpUVs)
{
    s_textureState = lpState;
    s_numSubImages = luCount;
    s_atlasUVs = lpUVs;
}

void CoronaRenderer::Begin(const RenderParameters& lrParameters)
{
    s_beginLock = true;
    shadow::Device::SetState(s_textureState, 0);
    shadow::Device::SetState(s_blendState);
    shadow::Device::SetVertexProgram(s_vertexProgram);
    shadow::Device::SetPixelProgram(s_pixelProgram);
    FloatShaderStateIterator lIterator;
    Device::BeginShaderStates(s_viewProjectionMatrixHandle, lIterator);
    lIterator.m_dest[0] = lrParameters.mViewProjectionMatrix.xAxis;
    lIterator.m_dest[1] = lrParameters.mViewProjectionMatrix.yAxis;
    lIterator.m_dest[2] = lrParameters.mViewProjectionMatrix.zAxis;
    lIterator.m_dest[3] = lrParameters.mViewProjectionMatrix.wAxis;
    Device::BeginShaderStates(s_cameraPositionHandle, lIterator);
    *lIterator.m_dest = lrParameters.mvCameraPositionPlusWhiteLevel;
    Device::BeginShaderStates(s_viewXyScaleHandle, lIterator);
    *lIterator.m_dest = lrParameters.mvViewXyScale;
}
void CoronaRenderer::End() { s_beginLock = false; }

void CoronaRenderer::Dispatch(const BatchParameters& lrParameters, CoronaBuffer* lpBuffer)
{
    shadow::Device::SetState((lrParameters.muFlags & BatchParameters::FLAG_OCCLUSION_ZTEST)
                            ? s_enabledZTestState : s_disabledZTestState);
    u32 luMaxVertices = (Device::GetSecondaryRingBufferSizeForReconstruction() >> 2) / 48;
    if (luMaxVertices > 0xffff) luMaxVertices = 0xffff;
    const u32 luMaxCoronas = luMaxVertices >> 2;
    u32 luLeft = lrParameters.muNumCoronas;
    const Corona* lpCorona = lpBuffer->GetCoronas();
    shadow::Device::SetVertexDescriptor(s_vertexDescriptor);
    shadow::Device::FlushVertexProgramState();
    D3DDevice* const lpDevice = gpD3DDevice;
    while (luLeft)
    {
        const u32 luCount = luLeft <= luMaxCoronas ? luLeft : luMaxCoronas;
        if (192 * luCount > 0x80000) D3DDevice_InsertFence(lpDevice);
        CoronaVertex* lpVertex = static_cast<CoronaVertex*>(D3DDevice_BeginVertices(lpDevice, D3DPT_QUADLIST, 4 * luCount, 48));
        for (u32 i = 0; i < luCount; ++i, ++lpCorona)
        {
            const f32 x = lpCorona->mvSize.mV.x;
            const f32 y = lpCorona->mvSize.mV.y;
            const Vector2* const lpUV = s_atlasUVs + (lpCorona->miTextureID << 2);
            const f32 afX[4] = {-x, x, x, -x};
            const f32 afY[4] = {y, y, -y, -y};
            for (u32 j = 0; j < 4; ++j, ++lpVertex)
            {
                lpVertex->position[0] = lpCorona->mvPosition.GetX().GetFloat();
                lpVertex->position[1] = lpCorona->mvPosition.GetY().GetFloat();
                lpVertex->position[2] = lpCorona->mvPosition.GetZ().GetFloat();
                lpVertex->position[3] = lpCorona->mfDistance;
                lpVertex->direction[0] = lpCorona->mvDirection.GetX().GetFloat();
                lpVertex->direction[1] = lpCorona->mvDirection.GetY().GetFloat();
                lpVertex->direction[2] = lpCorona->mvDirection.GetZ().GetFloat();
                lpVertex->corner[0] = lpUV[j].mV.x;
                lpVertex->corner[1] = lpUV[j].mV.y;
                lpVertex->corner[2] = afX[j];
                lpVertex->corner[3] = afY[j];
                lpVertex->colour = lpCorona->muColour;
            }
        }
        D3DDevice_EndVertices(lpDevice);
        luLeft -= luCount;
    }
}
}
