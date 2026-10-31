#include "GameSource/Graphics/BrnSunCorona.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <vectorintrinsics.h>

namespace
{
const char gszSourceFile[] = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnSunCorona.cpp";

// ARTIST 8203E118, 204 bytes.
const u32 gaOcclusionVertexProgram[] =
{
    0x102A1101u, 0x00000084u, 0x00000048u, 0x00000000u, 0x00000024u, 0x00000000u,
    0x00000058u, 0x00000000u, 0x00000000u, 0x00000030u, 0x0000001Cu, 0x00000023u,
    0xFFFE0300u, 0x00000000u, 0x00000000u, 0x00000008u, 0x0000001Cu, 0x76735F33u,
    0x5F300032u, 0x2E302E36u, 0x3533342Eu, 0x3100ABABu, 0x00000000u, 0x00000048u,
    0x00010000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000001u,
    0x00000000u, 0x00000290u, 0x00000003u, 0x10011003u, 0x00001200u, 0xC2000000u,
    0x00001004u, 0x00001200u, 0xC4000000u, 0x00000003u, 0x00002200u, 0x00000000u,
    0x05F80000u, 0x00000A88u, 0x00000000u, 0xC80F803Eu, 0x00000000u, 0xE2000000u,
    0x00000000u, 0x00000000u, 0x00000000u,
};

// ARTIST 8203E208, 524 bytes.
const u32 gaOcclusionPixelProgram[] =
{
    0x102A1100u, 0x00000118u, 0x000000F4u, 0x00000000u, 0x00000024u, 0x000000C4u,
    0x000000F8u, 0x00000000u, 0x00000000u, 0x0000009Cu, 0x0000001Cu, 0x0000008Fu,
    0xFFFF0300u, 0x00000002u, 0x0000001Cu, 0x00000008u, 0x00000088u, 0x00000044u,
    0x00030000u, 0x00010000u, 0x00000054u, 0x00000000u, 0x00000064u, 0x00020000u,
    0x00010000u, 0x00000078u, 0x00000000u, 0x53616D70u, 0x6C657253u, 0x6F757263u,
    0x6500ABABu, 0x0004000Cu, 0x00010001u, 0x00010000u, 0x00000000u, 0x6B557653u,
    0x74617274u, 0x416E644Fu, 0x66667365u, 0x7400ABABu, 0x00010003u, 0x00010004u,
    0x00010000u, 0x00000000u, 0x70735F33u, 0x5F300032u, 0x2E302E36u, 0x3533342Eu,
    0x3100ABABu, 0x00000000u, 0x00000001u, 0x00000003u, 0x00000000u, 0x00000020u,
    0x01FC0010u, 0x00000000u, 0x00000000u, 0x23980002u, 0x00000007u, 0x00000007u,
    0x00000000u, 0x00000000u, 0x00000040u, 0x000000B4u, 0x10000100u, 0x00000000u,
    0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x3F800000u, 0x3CA72F05u, 0x00000000u, 0x00000000u, 0x377F0001u, 0x3F7F0001u,
    0x3B7F0001u, 0x40400000u, 0x00002005u, 0x00081200u, 0x7000001Fu, 0x00001007u,
    0x00061200u, 0x7000001Eu, 0x00094008u, 0x00041200u, 0x8400001Eu, 0x0000100Cu,
    0x00021200u, 0x8400001Fu, 0x00000000u, 0x100DC400u, 0x22000000u, 0x20400000u,
    0x0100006Cu, 0xE2000080u, 0xC8020000u, 0x041B1BB1u, 0x0B00FF00u, 0x14100000u,
    0x0000006Cu, 0xC2000000u, 0x10081001u, 0x1F1FFEC8u, 0x00004000u, 0xC8080000u,
    0x0065C000u, 0xB001FF00u, 0xC8080000u, 0x001B6C00u, 0xA600FE00u, 0xB0140000u,
    0x001BC6C0u, 0xC0000000u, 0xB0200000u, 0x00000001u, 0xC2000000u, 0xC88FC000u,
    0x00C6B100u, 0xA100FE00u, 0x00000000u, 0x00000000u, 0x00000000u,
};

// ARTIST 8203E438, 240 bytes.
const u32 gaFlareVertexProgram[] =
{
    0x102A1101u, 0x00000090u, 0x00000060u, 0x00000000u, 0x00000024u, 0x00000000u,
    0x00000058u, 0x00000000u, 0x00000000u, 0x00000030u, 0x0000001Cu, 0x00000023u,
    0xFFFE0300u, 0x00000000u, 0x00000000u, 0x00000008u, 0x0000001Cu, 0x76735F33u,
    0x5F300032u, 0x2E302E36u, 0x3533342Eu, 0x3100ABABu, 0x00000000u, 0x00000060u,
    0x00010001u, 0x00000000u, 0x00000000u, 0x00000821u, 0x00000001u, 0x00000002u,
    0x00000001u, 0x00000290u, 0x00000003u, 0x00005005u, 0x00003050u, 0x00001006u,
    0x10011003u, 0x00001200u, 0xC2000000u, 0x00001004u, 0x00001200u, 0xC4000000u,
    0x10092005u, 0x00002200u, 0x00000000u, 0x05F81000u, 0x00000A88u, 0x00000000u,
    0xC80F803Eu, 0x00000000u, 0xE2010100u, 0x05F80000u, 0x00000FC8u, 0x00000000u,
    0xC8038000u, 0x00B0B000u, 0xE2000000u, 0x00000000u, 0x00000000u, 0x00000000u,
};

// ARTIST 8203E528, 464 bytes.
const u32 gaFlarePixelProgram[] =
{
    0x102A1100u, 0x0000010Cu, 0x000000C4u, 0x00000000u, 0x00000024u, 0x000000C0u,
    0x000000E8u, 0x00000000u, 0x00000000u, 0x00000098u, 0x0000001Cu, 0x0000008Bu,
    0xFFFF0300u, 0x00000002u, 0x0000001Cu, 0x00000008u, 0x00000084u, 0x00000044u,
    0x00030000u, 0x00010000u, 0x00000054u, 0x00000000u, 0x00000064u, 0x00020000u,
    0x00010000u, 0x00000074u, 0x00000000u, 0x4F63636Cu, 0x7573696Fu, 0x6E536F75u,
    0x72636500u, 0x0004000Cu, 0x00010001u, 0x00010000u, 0x00000000u, 0x6B436F6Cu,
    0x6F757241u, 0x6E64506Fu, 0x77657200u, 0x00010003u, 0x00010004u, 0x00010000u,
    0x00000000u, 0x70735F33u, 0x5F300032u, 0x2E302E36u, 0x3533342Eu, 0x3100ABABu,
    0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000014u, 0x01FC0010u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000040u, 0x00000084u,
    0x10000100u, 0x00000004u, 0x00000000u, 0x00000821u, 0x00010001u, 0x00000001u,
    0x00003050u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x6002C400u, 0x12000024u, 0x00002008u, 0x00002200u, 0x00000000u, 0x20400000u,
    0x0100006Cu, 0xE2000080u, 0xA8081001u, 0x1F1FF1FFu, 0x00004000u, 0xC8020000u,
    0x00B0B0B1u, 0xD10000FFu, 0xBA200000u, 0x00000041u, 0xC20000FFu, 0xA8100000u,
    0x00000041u, 0xC2000000u, 0x08110001u, 0x006CB1B1u, 0xE1000000u, 0xC8060001u,
    0x006CC100u, 0xA1000000u, 0xC80F8000u, 0x00000000u, 0xE2010100u, 0x00000000u,
    0x00000000u, 0x00000000u,
};

void AssertSunCorona(bool lbCondition, const char* lpExpression, s32 liLine)
{
    if (!lbCondition)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lpExpression, gszSourceFile, liLine);
        CgsDev::Assert::EndAssert();
    }
}

renderengine::ProgramBufferData* CreateProgram(rw::IResourceAllocator* lpAllocator,
    const u32* lpBinary, u32 luType, u32 luSize)
{
    renderengine::ProgramBufferParameters lParameters;
    lParameters.muFunction = reinterpret_cast<u32>(lpBinary);
    lParameters.muShaderType = luType;
    lParameters.muReserved8 = luSize;
    lParameters.muMicrocodePart1 = 0;
    lParameters.muMicrocodePart3 = 0;
    lParameters.muConstantTableSize = 0;
    lParameters.muNumVariables = 0;

    rw::ResourceDescriptor lDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);
    rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
    // ARTIST82400B0C/B98/C30/CB0 passes the actual five-lane resource.
    // ProgramBuffer::Initialize consumes its existing lane-zero/lane-two view.
    return renderengine::ProgramBuffer::Initialize(
        reinterpret_cast<renderengine::ProgramResourceLayout*>(&lResource), &lParameters);
}

struct SunCoronaVertex
{
    f32 mafPosition[3];
    f32 mafUv[2];
};
CGS_STATIC_ASSERT((sizeof(SunCoronaVertex) == 20), "ARTIST sun vertex stride");

void WriteVertex(SunCoronaVertex& lrVertex, f32 x, f32 y, f32 u, f32 v)
{
    lrVertex.mafPosition[0] = x;
    lrVertex.mafPosition[1] = y;
    lrVertex.mafPosition[2] = 0.0f;
    lrVertex.mafUv[0] = u;
    lrVertex.mafUv[1] = v;
}

f32 FirstLane(__vector4 lValue)
{
    __declspec(align(16)) f32 lafLanes[4];
    __stvx(lValue, lafLanes, 0);
    return lafLanes[0];
}
}

// ARTIST824009B0.
void BrnSunCorona::Construct(rw::IResourceAllocator* lpAllocator)
{
    mfXPos = 0.0f;
    mfYPos = 0.0f;
    mpAllocator = lpAllocator;
    mbVisible = false;
    mbRenderSunCorona = true;
    mfSunVectorYMultiplier = 0.5f;
    mfOcclusionSize = 2.0f;
    mfSunFlarePow = 2.0f;
    mfSunBrightness = 0.30000001192092896f;
    mfSunSize = 0.30000001192092896f;

    renderengine::VertexDescriptor::Parameters lVertexParameters;
    lVertexParameters.maElements[0].mu16Stream = 0;
    lVertexParameters.maElements[0].miOffset = 0x002A23B9;
    lVertexParameters.maElements[0].mu8UsageIndex = 1;
    lVertexParameters.maElements[1].mu16Stream = 0;
    lVertexParameters.maElements[1].miOffset = 0x002C23A5;
    lVertexParameters.maElements[1].mu8UsageIndex = 6;
    rw::ResourceDescriptor lVertexDescriptor;
    renderengine::VertexDescriptor::GetResourceDescriptor(&lVertexDescriptor, &lVertexParameters);
    mVertexDescriptorResource = lpAllocator->DoAllocate(lVertexDescriptor, 0);
    mpVertexDescriptor = renderengine::VertexDescriptor::Initialize(&mVertexDescriptorResource, &lVertexParameters);

    mpOcclusionVertexProgram = CreateProgram(lpAllocator, gaOcclusionVertexProgram, 0, sizeof(gaOcclusionVertexProgram));
    AssertSunCorona(mpOcclusionVertexProgram != 0, "NULL != mpOcclusionVertexProgram", 97);
    mpOcclusionPixelProgram = CreateProgram(lpAllocator, gaOcclusionPixelProgram, 1, sizeof(gaOcclusionPixelProgram));
    AssertSunCorona(mpOcclusionPixelProgram != 0, "NULL != mpOcclusionPixelProgram", 109);
    renderengine::ProgramBuffer::GetVariableHandleByName(mpOcclusionPixelProgram,
        reinterpret_cast<const u8*>("kUvStartAndOffset"), &mOcclusionPixelVariableHandleUvStartOffset);

    mpFlareVertexProgram = CreateProgram(lpAllocator, gaFlareVertexProgram, 0, sizeof(gaFlareVertexProgram));
    AssertSunCorona(mpFlareVertexProgram != 0, "NULL != mpFlareVertexProgram", 132);
    mpFlarePixelProgram = CreateProgram(lpAllocator, gaFlarePixelProgram, 1, sizeof(gaFlarePixelProgram));
    AssertSunCorona(mpFlarePixelProgram != 0, "NULL != mpFlarePixelProgram", 144);
    renderengine::ProgramBuffer::GetVariableHandleByName(mpFlarePixelProgram,
        reinterpret_cast<const u8*>("kColourAndPower"), &mFlarePixelVariableHandleColourAndPower);
}

// ARTIST823F8298. Only the descriptor resource is released here.
void BrnSunCorona::Destruct()
{
    AssertSunCorona(mpAllocator != 0, "mpAllocator", 170);
    renderengine::VertexDescriptor::Release(mpVertexDescriptor);
    mpAllocator->DoFree(mVertexDescriptorResource);
}

// ARTIST823FD798. Preserve the native VMX estimates, refinement and fused math.
void BrnSunCorona::ComputeSunPositionOnScreen(Matrix44 lViewProjectionMatrix,
    Vector3 lViewPosition, Vector3 lSunDir)
{
    const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
    const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
    const __vector4 lMultiplier = { mfSunVectorYMultiplier, 0.0f, 0.0f, 0.0f };
    __vector4 lLocalSunDir = __lvx(&lSunDir, 0);
    lLocalSunDir = __vrlimi(lLocalSunDir,
        __vmulfp(__vspltw(lLocalSunDir, 1), __vspltw(lMultiplier, 0)), 4, 0);
    const __vector4 lLengthSquared = __vmsum3fp(lLocalSunDir, lLocalSunDir);
    __vector4 lInverseLength = __vrsqrtefp(lLengthSquared);
    __vector4 lSquared = __vmulfp(lInverseLength, lInverseLength);
    __vector4 lHalfInverse = __vmulfp(lInverseLength, lHalf);
    lInverseLength = __vmaddfp(lHalfInverse, __vnmsubfp(lLengthSquared, lSquared, lOne), lInverseLength);
    lSquared = __vmulfp(lInverseLength, lInverseLength);
    lHalfInverse = __vmulfp(lInverseLength, lHalf);
    lInverseLength = __vmaddfp(lHalfInverse, __vnmsubfp(lLengthSquared, lSquared, lOne), lInverseLength);
    const __vector4 lDistance = { -10000.0f, -10000.0f, -10000.0f, -10000.0f };
    __vector4 lSunPosition = __vmaddfp(__vmulfp(lLocalSunDir, lInverseLength), lDistance, __lvx(&lViewPosition, 0));
    lSunPosition = __vrlimi(lSunPosition, lOne, 1, 0);

    __vector4 lProjected = __vmulfp(__lvx(&lViewProjectionMatrix, 0), __vspltw(lSunPosition, 0));
    lProjected = __vmaddfp(__lvx(&lViewProjectionMatrix, 16), __vspltw(lSunPosition, 1), lProjected);
    lProjected = __vmaddfp(__lvx(&lViewProjectionMatrix, 32), __vspltw(lSunPosition, 2), lProjected);
    lProjected = __vmaddfp(__lvx(&lViewProjectionMatrix, 48), __vspltw(lSunPosition, 3), lProjected);
    const __vector4 lW = __vspltw(lProjected, 3);
    __vector4 lInverseX = __vrefp(lW);
    __vector4 lInverseY = __vrefp(lW);
    lInverseX = __vmaddfp(lInverseX, __vnmsubfp(lInverseX, lW, lOne), lInverseX);
    lInverseY = __vmaddfp(lInverseY, __vnmsubfp(lInverseY, lW, lOne), lInverseY);
    lInverseX = __vmaddfp(lInverseX, __vnmsubfp(lInverseX, lW, lOne), lInverseX);
    lInverseY = __vmaddfp(lInverseY, __vnmsubfp(lInverseY, lW, lOne), lInverseY);
    const f32 lfZNonProjected = FirstLane(__vspltw(lProjected, 2));
    const f32 lfX = FirstLane(__vmulfp(lInverseX, __vspltw(lProjected, 0)));
    const f32 lfY = FirstLane(__vmulfp(lInverseY, __vspltw(lProjected, 1)));
    const f32 lfHalf = FirstLane(lHalf);
    const f32 lfNegativeHalf = FirstLane(__vcfsx(__vspltisw(-1), 1));
    mfXPos = lfX * lfHalf + lfHalf;
    mfYPos = lfY * lfNegativeHalf + lfHalf;
    mbVisible = lfZNonProjected > 0.0f;
}

// ARTIST82400CF8.
void BrnSunCorona::GenerateOcclusionBuffer(CgsRenderTarget* lpOcclusionRt,
    CgsRenderTarget* lpSourceDepthRt)
{
    AssertSunCorona(lpOcclusionRt != 0, "lpOcclusionRt != NULL", 250);
    AssertSunCorona(lpSourceDepthRt != 0, "lpSourceDepthRt != NULL", 251);
    if (!mbRenderSunCorona)
        return;
    rw::graphics::postfx::RenderTarget* const lpDepthRenderTarget = lpSourceDepthRt->GetRenderTarget();
    AssertSunCorona(lpDepthRenderTarget != 0, "lpDepthRenderTarget", 260);
    lpOcclusionRt->Begin();
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));
    shadow::Device::SetVertexProgram(mpOcclusionVertexProgram);
    shadow::Device::SetPixelProgram(mpOcclusionPixelProgram);
    const f32 lfStepU = (1.0f / static_cast<f32>(lpSourceDepthRt->GetWidth())) * mfOcclusionSize;
    const f32 lfStepV = (1.0f / static_cast<f32>(lpSourceDepthRt->GetHeight())) * mfOcclusionSize;
    const f32 lfStartU = mfXPos;
    const f32 lfStartV = mfYPos;
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(mOcclusionPixelVariableHandleUvStartOffset, lIterator);
    lIterator.m_dest->x = lfStartU;
    lIterator.m_dest->y = lfStartV;
    lIterator.m_dest->z = lfStepU;
    lIterator.m_dest->w = lfStepV;
    shadow::Device::SetState(lpDepthRenderTarget->mDepthTarget.mpTextureState, 0);
    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();
    D3DDevice* const lpDevice = ::gpD3DDevice;
    SunCoronaVertex* const lpVertex = static_cast<SunCoronaVertex*>(
        D3DDevice_BeginVertices(lpDevice, D3DPT_TRIANGLESTRIP, 4, sizeof(SunCoronaVertex)));
    WriteVertex(lpVertex[0], -1.0f, -1.0f, 0.0f, 0.0f);
    WriteVertex(lpVertex[1], 1.0f, -1.0f, 0.0f, 0.0f);
    WriteVertex(lpVertex[2], -1.0f, 1.0f, 0.0f, 0.0f);
    WriteVertex(lpVertex[3], 1.0f, 1.0f, 0.0f, 0.0f);
    D3DDevice_EndVertices(lpDevice);
    lpOcclusionRt->End();
}

// ARTIST824010A8. The current target has already been opened by the caller.
void BrnSunCorona::RenderOccludedFlare(CgsRenderTarget* lpOcclusionRt,
    Vector3 lSunColour, f32 lfWhiteLevel, f32 lfDebugOverrideBrightness,
    bool lbDebugOverrideBrightness)
{
    AssertSunCorona(lpOcclusionRt != 0, "lpOcclusionRt != NULL", 369);
    if (!mbVisible || !mbRenderSunCorona)
        return;
    rw::graphics::postfx::RenderTarget* const lpOcclusionRenderTarget = lpOcclusionRt->GetRenderTarget();
    AssertSunCorona(lpOcclusionRenderTarget != 0, "lpOcclusionRenderTarget", 378);
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF));
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB));
    shadow::Device::SetVertexProgram(mpFlareVertexProgram);
    shadow::Device::SetPixelProgram(mpFlarePixelProgram);
    const f32 lfBrightness = lbDebugOverrideBrightness ? lfDebugOverrideBrightness : mfSunBrightness;
    const __vector4 lWhite = { lfWhiteLevel, lfWhiteLevel, lfWhiteLevel, lfWhiteLevel };
    const __vector4 lBrightness = { lfBrightness, lfBrightness, lfBrightness, lfBrightness };
    const __vector4 lColour = __vmulfp(__vmulfp(__lvx(&lSunColour, 0), lWhite), lBrightness);
    const f32 lfRed = FirstLane(__vspltw(lColour, 0));
    const f32 lfGreen = FirstLane(__vspltw(lColour, 1));
    const f32 lfBlue = FirstLane(__vspltw(lColour, 2));
    const f32 lfPower = mfSunFlarePow;
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(mFlarePixelVariableHandleColourAndPower, lIterator);
    lIterator.m_dest->x = lfRed;
    lIterator.m_dest->y = lfGreen;
    lIterator.m_dest->z = lfBlue;
    lIterator.m_dest->w = lfPower;
    shadow::Device::SetState(lpOcclusionRenderTarget->maColourTargets[0].mpTextureState, 0);
    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();
    const f32 lfLeft = (mfXPos - mfSunSize) * 2.0f - 1.0f;
    const f32 lfRight = (mfXPos + mfSunSize) * 2.0f - 1.0f;
    const f32 lfHalfHeight = mfSunSize * 1.7777777910232544f;
    const f32 lfTop = ((1.0f - mfYPos) + lfHalfHeight) * 2.0f - 1.0f;
    const f32 lfBottom = ((1.0f - mfYPos) - lfHalfHeight) * 2.0f - 1.0f;
    D3DDevice* const lpDevice = ::gpD3DDevice;
    SunCoronaVertex* const lpVertex = static_cast<SunCoronaVertex*>(
        D3DDevice_BeginVertices(lpDevice, D3DPT_TRIANGLESTRIP, 4, sizeof(SunCoronaVertex)));
    WriteVertex(lpVertex[0], lfLeft, lfTop, -1.0f, 1.0f);
    WriteVertex(lpVertex[1], lfRight, lfTop, 1.0f, 1.0f);
    WriteVertex(lpVertex[2], lfLeft, lfBottom, -1.0f, -1.0f);
    WriteVertex(lpVertex[3], lfRight, lfBottom, 1.0f, -1.0f);
    D3DDevice_EndVertices(lpDevice);
    lpOcclusionRt->End();
}
