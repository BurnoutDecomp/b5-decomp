#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include <vectorintrinsics.h>
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImNativePrograms.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the constant-resolve witness
#include <cstdio>

namespace CgsGraphics
{
namespace
{

    const u32 KU_POSITION_ELEMENT_WORD = 0x2A23B9u;
    const u32 KU_COLOUR_ELEMENT_WORD   = 0x14C86u;
    const u32 KU_TEXCOORD_ELEMENT_WORD = 0x2C23A5u;
}

} // namespace CgsGraphics

namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

namespace CgsGraphics
{

template <typename V>
s8 ImRenderer<V>::AddProgram(rw::IResourceAllocator* lpAllocator,
                             const void* lpVertexProgramBinary, u32 luVertexProgramSize,
                             const void* lpPixelProgramBinary, u32 luPixelProgramSize)
{
    s8 li8ProgramIndex = 0;
    while (mapVertexProgramBuffer[li8ProgramIndex] != 0)
    {
        li8ProgramIndex = static_cast<s8>(li8ProgramIndex + 1);
        if (li8ProgramIndex >= KI8_MAX_PROGRAMS)
        {
            break;
        }
    }

    if (li8ProgramIndex < KI8_MAX_PROGRAMS)
    {
        CGS_ASSERT(mapPixelProgramBuffer[li8ProgramIndex] == 0,
                   "mapPixelProgramBuffer[ li8ProgramIndex ] == NULL");
    }

    CGS_ASSERT(li8ProgramIndex < KI8_MAX_PROGRAMS,
               "Adding too many shader programs to the immediate mode renderer");

    renderengine::ProgramBufferParameters lVertexParams;
    lVertexParams.muShaderType        = 0;
    lVertexParams.muMicrocodePart1    = 0;
    lVertexParams.muMicrocodePart3    = 0;
    lVertexParams.muConstantTableSize = 0;
    lVertexParams.muNumVariables      = 0;
    lVertexParams.muFunction          = static_cast<u32>(reinterpret_cast<uintptr_t>(lpVertexProgramBinary));
    lVertexParams.muReserved8         = luVertexProgramSize;

    rw::BaseResourceDescriptors<5> lVertexDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lVertexDescriptor, &lVertexParams);

    renderengine::ProgramResourceLayout lVertexLayout = {};
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &lVertexLayout, &lVertexDescriptor);
    mapVertexProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lVertexLayout, &lVertexParams));

    renderengine::ProgramBufferParameters lPixelParams;
    lPixelParams.muMicrocodePart1    = 0;
    lPixelParams.muMicrocodePart3    = 0;
    lPixelParams.muConstantTableSize = 0;
    lPixelParams.muNumVariables      = 0;
    lPixelParams.muFunction          = static_cast<u32>(reinterpret_cast<uintptr_t>(lpPixelProgramBinary));
    lPixelParams.muReserved8         = luPixelProgramSize;
    lPixelParams.muShaderType        = 1;

    rw::BaseResourceDescriptors<5> lPixelDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lPixelDescriptor, &lPixelParams);

    renderengine::ProgramResourceLayout lPixelLayout = {};
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &lPixelLayout, &lPixelDescriptor);
    mapPixelProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lPixelLayout, &lPixelParams));

    return li8ProgramIndex;
}

template <typename V>
void ImRenderer<V>::Construct(rw::IResourceAllocator* lpAllocator,
                              const void* const* lapVertexProgramBinary,
                              const u32* lauVertexProgramSize,
                              const void* const* lapPixelProgramBinary,
                              const u32* lauPixelProgramSize,
                              s8 li8NumberPrograms)
{
    if (!mgbStateLibraryConstructed)
    {
        ConstructOnceOnly(lpAllocator);
        mgbStateLibraryConstructed = true;
    }

    renderengine::VertexDescriptor::Parameters lParameters;

    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_POSITION_ELEMENT_WORD);
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 12;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_COLOUR_ELEMENT_WORD);
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 10;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 4;

    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = 16;
    lParameters.maElements[2].miOffset      = static_cast<s32>(KU_TEXCOORD_ELEMENT_WORD);
    lParameters.maElements[2].mu8Type       = 0;
    lParameters.maElements[2].mu8Pad1       = 5;
    lParameters.maElements[2].mu8Usage      = 0;
    lParameters.maElements[2].mu8UsageIndex = 6;

    u8 lauDescriptor[144] = {};
    renderengine::VertexDescriptor::GetResourceDescriptor(lauDescriptor, &lParameters);

    rw::Resource lDescriptorResource;
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &lDescriptorResource, lauDescriptor);
    mpVertexDescriptor =
        reinterpret_cast<renderengine::VertexDescriptor*>(
            renderengine::VertexDescriptor::Initialize(&lDescriptorResource, &lParameters));

    CGS_ASSERT(li8NumberPrograms <= KI8_MAX_PROGRAMS, "li8NumberPrograms<=KI8_MAX_PROGRAMS");

    mi8CurrentProgram = 0;
    for (s32 liSlot = 0; liSlot < KI8_MAX_PROGRAMS; ++liSlot)
    {
        mapVertexProgramBuffer[liSlot] = 0;
        mapPixelProgramBuffer[liSlot]  = 0;
    }

    for (s32 liProgram = 0; liProgram < li8NumberPrograms; ++liProgram)
    {
        CGS_ASSERT(lapVertexProgramBinary[liProgram] != 0,
                   "lapVertexProgramBinary[ li8ProgramIndex ] != NULL");
        CGS_ASSERT(lapPixelProgramBinary[liProgram] != 0,
                   "lapPixelProgramBinary[ li8ProgramIndex ] != NULL");
        if (lauVertexProgramSize[liProgram] == 0)
        {
            CGS_ASSERT(lauVertexProgramSize[liProgram] > 0,
                       "lauVertexProgramSize[ li8ProgramIndex ] > 0");
            CGS_ASSERT(lauVertexProgramSize[liProgram] > 0,
                       "lauVertexProgramSize[ li8ProgramIndex ] > 0");
        }

        AddProgram(lpAllocator,
                   lapVertexProgramBinary[liProgram], lauVertexProgramSize[liProgram],
                   lapPixelProgramBinary[liProgram], lauPixelProgramSize[liProgram]);
    }
}

template <typename V>
bool ImRenderer<V>::SetProgram(s8 li8Program)
{
    CGS_ASSERT(mapVertexProgramBuffer[li8Program] != 0,
               "mapVertexProgramBuffer[ li8Program ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[li8Program] != 0,
               "mapPixelProgramBuffer[ li8Program ] != NULL");
    if (shadow::Device::SetVertexProgram(
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[li8Program])))
    {
        mi8CurrentProgram = li8Program;
        shadow::Device::SetPixelProgram(
            reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[li8Program]));
        return true;
    }
    return false;
}

template <typename V>
void ImRenderer<V>::EndRendering()
{
    CGS_ASSERT(mgpActiveRenderer == static_cast<ImRendererBase*>(this), "mgpActiveRenderer == this");
    mgpActiveRenderer = 0;
}

template <typename V>
void ImRenderer<V>::BeginRendering()
{
    CGS_ASSERT(mapVertexProgramBuffer[0] != 0, "mapVertexProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[0] != 0, "mapPixelProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mgpActiveRenderer == 0, "mgpActiveRenderer == NULL");
    mgpActiveRenderer = this;
    shadow::Device::ResetShadowing();
    const renderengine::ProgramBufferData* const lpVertex =
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]);
    const renderengine::ProgramBufferData* const lpPixel =
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[0]);
    mi8CurrentProgram = 0;
    shadow::Device::SetVertexProgram(lpVertex);
    shadow::Device::SetPixelProgram(lpPixel);
    shadow::Device::SetVertexDescriptor(
        reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor));
}

template <typename V>
void ImRenderer<V>::Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount)
{
    CGS_ASSERT(mgpActiveRenderer == static_cast<ImRendererBase*>(this), "mgpActiveRenderer == this");
    mDirectDrawParameters.SetPrimitiveType(lePrimitiveType);
    shadow::Device::FlushVertexProgramState();
    const D3DPRIMITIVETYPE leType = static_cast<D3DPRIMITIVETYPE>(mDirectDrawParameters.GetPrimitiveType());
    D3DDevice* const lpDevice = gpD3DDevice;
    CGS_ASSERT(lpVertices != 0, "lpVertices");
    if (24u * luCount > 0x80000u)
        D3DDevice_InsertFence(lpDevice);
    struct PackedVertex
    {
        Vector3F mPosition;
        RGBA8 mColour;
        Vector2F mUv;
    };
    CGS_STATIC_ASSERT((sizeof(PackedVertex) == 24), "ARTIST GPU vertex stride");
    PackedVertex* lpOutput = static_cast<PackedVertex*>(D3DDevice_BeginVertices(
        lpDevice, leType, luCount, sizeof(PackedVertex)));
        for (u32 luVertex = 0; luVertex < luCount; ++luVertex)
            {
                lpOutput[luVertex].mPosition.x = lpVertices[luVertex].mv3Pos.x;
                lpOutput[luVertex].mPosition.y = lpVertices[luVertex].mv3Pos.y;
                lpOutput[luVertex].mPosition.z = lpVertices[luVertex].mv3Pos.z;
                lpOutput[luVertex].mColour = lpVertices[luVertex].mv4Colour;
                lpOutput[luVertex].mUv = lpVertices[luVertex].mv2Tex0UV;
            }
    D3DDevice_EndVertices(lpDevice);
}

template void ImRenderer<BasicColouredTexturedVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<BasicColouredTexturedVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template bool ImRenderer<BasicColouredTexturedVertex>::SetProgram(s8);
template void ImRenderer<BasicColouredTexturedVertex>::EndRendering();
template void ImRenderer<BasicColouredTexturedVertex>::BeginRendering();
template void ImRenderer<BasicColouredTexturedVertex>::Render(
    renderengine::PrimitiveType, const BasicColouredTexturedVertex*, u32);

template <typename V>
void Im3dBase<V>::SetTransform(Matrix44 lTransform)
{
    mCurrentTransform = lTransform;
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(maWorldViewProjStateHandle[this->mi8CurrentProgram], lIterator);
    *lIterator.m_dest++ = lTransform.xAxis;
    *lIterator.m_dest++ = lTransform.yAxis;
    *lIterator.m_dest++ = lTransform.zAxis;
    *lIterator.m_dest = lTransform.wAxis;
}

template <typename V>
void Im3dBase<V>::SetTransform(Matrix44 lModelToWorld, Matrix44 lViewProjection)
{
    Matrix44 lTransform;
    for (u32 luRow = 0; luRow < 4; ++luRow)
    {
        const __vector4 lRow = __lvx(&lModelToWorld, luRow * 16);
        __vector4 lResult = __vmulfp(__lvx(&lViewProjection, 0), __vspltw(lRow, 0));
        lResult = __vmaddfp(__lvx(&lViewProjection, 16), __vspltw(lRow, 1), lResult);
        lResult = __vmaddfp(__lvx(&lViewProjection, 32), __vspltw(lRow, 2), lResult);
        lResult = __vmaddfp(__lvx(&lViewProjection, 48), __vspltw(lRow, 3), lResult);
        __stvx(lResult, &lTransform, luRow * 16);
    }
    SetTransform(lTransform);
}
template void Im3dBase<BasicColouredTexturedVertex>::SetTransform(Matrix44);
template void Im3dBase<BasicColouredTexturedVertex>::SetTransform(Matrix44, Matrix44);

template void Im3dBase<BasicColouredVertex>::SetTransform(Matrix44);
template void Im3dBase<BasicColouredVertex>::SetTransform(Matrix44, Matrix44);

void Im3d::Construct(rw::IResourceAllocator* lpAllocator)
{
    mCurrentTransform.SetIdentity();
    const void* const lapVertexProgramBinary[2] = { gaIm3dDefaultVertexProgram, gaIm3dMaskVertexProgram };
    const u32 lauVertexProgramSize[2] = { sizeof(gaIm3dDefaultVertexProgram), sizeof(gaIm3dMaskVertexProgram) };
    const void* const lapPixelProgramBinary[2] = { gaIm3dDefaultPixelProgram, gaIm3dMaskPixelProgram };
    const u32 lauPixelProgramSize[2] = { sizeof(gaIm3dDefaultPixelProgram), sizeof(gaIm3dMaskPixelProgram) };
    ImRenderer<BasicColouredTexturedVertex>::Construct(lpAllocator,
        lapVertexProgramBinary, lauVertexProgramSize, lapPixelProgramBinary, lauPixelProgramSize, 2);
    for (s8 li8Program = 0; li8Program < 2; ++li8Program)
    {
        CGS_ASSERT(mapVertexProgramBuffer[li8Program] != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
        renderengine::ProgramBuffer::GetVariableHandleByName(
            reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[li8Program]),
            reinterpret_cast<const u8*>("worldViewProj"), &maWorldViewProjStateHandle[li8Program]);
    }
    CGS_ASSERT(mapVertexProgramBuffer[1] != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
    if (!renderengine::ProgramBuffer::GetVariableHandleByName(
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[1]),
        reinterpret_cast<const u8*>("gvMaskUseFlags"), &maMask[0].mPixelMaskUseHandle))
    {
        CGS_ASSERT(mapPixelProgramBuffer[1] != 0, "mapPixelProgramBuffer[ li8Program ] != NULL");
        const bool lbFound = renderengine::ProgramBuffer::GetVariableHandleByName(
            reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[1]),
            reinterpret_cast<const u8*>("gvMaskUseFlags"), &maMask[0].mPixelMaskUseHandle) != 0;
        CGS_ASSERT(lbFound, "GetPixelProgram( leMaskShader )->GetVariableHandleByName(\"gvMaskUseFlags\", maMask[ liShader ].mPixelMaskUseHandle)");
    }
    maMask[0].mvPixelMask.SetZero();
    const char* const lapFormats[3] = { "gvMask%cPositionMinMax", "gvMask%cUVStartEnd", "gvMask%cUVDifference" };
    const char* const lapFailures[3] = {
        "GetPixelProgram( leMaskShader )->GetVariableHandleByName(lacVarName, maMask[ liShader ].maPixelMaskHandles[ i ].mPixelMaskPositionMinMax)",
        "GetPixelProgram( leMaskShader )->GetVariableHandleByName(lacVarName, maMask[ liShader ].maPixelMaskHandles[ i ].mPixelMaskUVStartEnd)",
        "GetPixelProgram( leMaskShader )->GetVariableHandleByName(lacVarName, maMask[ liShader ].maPixelMaskHandles[ i ].mPixelMaskUVDifference)" };
    for (u32 luLayer = 0; luLayer < 2; ++luLayer)
    {
        Im3dMask::Im3dMaskLayer& lrLayer = maMask[0].maPixelMaskHandles[luLayer];
        renderengine::ProgramVariableHandle* const lapHandles[3] = {
            &lrLayer.mPixelMaskPositionMinMax, &lrLayer.mPixelMaskUVStartEnd, &lrLayer.mPixelMaskUVDifference };
        for (u32 luVariable = 0; luVariable < 3; ++luVariable)
        {
            char lacName[256];
            CgsCore::SPrintf(lacName, 256, lapFormats[luVariable], 'A' + luLayer);
            CGS_ASSERT(mapVertexProgramBuffer[1] != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
            if (!renderengine::ProgramBuffer::GetVariableHandleByName(
                reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[1]),
                reinterpret_cast<const u8*>(lacName), lapHandles[luVariable]))
            {
                CGS_ASSERT(mapPixelProgramBuffer[1] != 0, "mapPixelProgramBuffer[ li8Program ] != NULL");
                const bool lbFound = renderengine::ProgramBuffer::GetVariableHandleByName(
                    reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[1]),
                    reinterpret_cast<const u8*>(lacName), lapHandles[luVariable]) != 0;
                CGS_ASSERT(lbFound, lapFailures[luVariable]);
            }
        }
        lrLayer.mvPixelMaskPositionMinMax.SetZero();
        lrLayer.mvPixelMaskUVStartEnd.SetZero();
        lrLayer.mvPixelMaskUVDifference.SetZero();
    }
    mu32NumMasks = 0;
}

void Im3d::SaveMaskShaderConstants(const BasicColouredTexturedVertex* lpVertices,
    renderengine::TextureState* lpTextureState, renderengine::Texture* lpTexture)
{
    Im3dMask& lrMask = maMask[0];
    Im3dMask::Im3dMaskLayer& lrLayer = lrMask.maPixelMaskHandles[mu32NumMasks];
    const Vector4 lPosition = { lpVertices[0].mv3Pos.x, lpVertices[0].mv3Pos.y,
                               lpVertices[1].mv3Pos.x, lpVertices[1].mv3Pos.y };
    lrLayer.mvPixelMaskPositionMinMax = lPosition;
    const Vector4 lUV = { lpVertices[0].mv2Tex0UV.x, lpVertices[0].mv2Tex0UV.y,
                         lpVertices[1].mv2Tex0UV.x, lpVertices[1].mv2Tex0UV.y };
    lrLayer.mvPixelMaskUVStartEnd = lUV;
    const Vector4 lDifference = { 1.0f / (lpVertices[1].mv3Pos.x - lpVertices[0].mv3Pos.x),
                                 1.0f / (lpVertices[1].mv3Pos.y - lpVertices[0].mv3Pos.y), 0.0f, 0.0f };
    lrLayer.mvPixelMaskUVDifference = lDifference;
    lrLayer.mpMaskTextureState = lpTextureState;
    lrLayer.mpMaskTexture = lpTexture;
    for (u32 luLane = 0; luLane < mu32NumMasks + 1; ++luLane)
        reinterpret_cast<f32*>(&lrMask.mvPixelMask)[luLane] = 1.0f;
}

void Im3d::SetMaskPixelShaderState()
{
    CGS_ASSERT(mu32NumMasks > 0, "mu32NumMasks > 0");
    Im3dMask& lrMask = maMask[(mi8CurrentProgram - 1) / 2];
    Im3dMask::Im3dMaskLayer& lrLayer = lrMask.maPixelMaskHandles[mu32NumMasks - 1];
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(lrLayer.mPixelMaskPositionMinMax, lIterator);
    *lIterator.m_dest++ = lrLayer.mvPixelMaskPositionMinMax;
    renderengine::Device::BeginShaderStates(lrLayer.mPixelMaskUVStartEnd, lIterator);
    *lIterator.m_dest++ = lrLayer.mvPixelMaskUVStartEnd;
    renderengine::Device::BeginShaderStates(lrLayer.mPixelMaskUVDifference, lIterator);
    *lIterator.m_dest++ = lrLayer.mvPixelMaskUVDifference;
    renderengine::Device::BeginShaderStates(lrMask.mPixelMaskUseHandle, lIterator);
    *lIterator.m_dest = lrMask.mvPixelMask;
    if (lrLayer.mpMaskTextureState)
        ImRendererBase::SetState(lrLayer.mpMaskTextureState, mu32NumMasks);
    else if (lrLayer.mpMaskTexture)
        ImRendererBase::SetTexture(lrLayer.mpMaskTexture, mu32NumMasks);
}

void Im3d::PushMask(renderengine::TextureState* lpTextureState, renderengine::Texture* lpTexture,
                     const BasicColouredTexturedVertex* lpVertices)
{
    CGS_ASSERT(mu32NumMasks < KU_MAX_MASK_COUNT, "mu32NumMasks < V_IM3D_MAX_MASK_COUNT");
    if (mu32NumMasks == 0 && ImRenderer<BasicColouredTexturedVertex>::SetProgram(static_cast<s8>(mi8CurrentProgram + 1)))
        SetTransform(mCurrentTransform);
    SaveMaskShaderConstants(lpVertices, lpTextureState, lpTexture);
    ++mu32NumMasks;
    SetMaskPixelShaderState();
}

} // namespace CgsGraphics

namespace CgsGraphics
{
    bool Im3d::SetProgram(s8 li8Program)
    {
        s8 li8Slot = static_cast<s8>(2 * li8Program);
        const bool lbMasked = li8Program < 1 && mu32NumMasks != 0;
        if (lbMasked) li8Slot = static_cast<s8>(li8Slot + 1);
        const bool lbChanged = ImRenderer<BasicColouredTexturedVertex>::SetProgram(li8Slot);
        if (lbChanged)
        {
            SetTransform(mCurrentTransform);
            if (lbMasked) SetMaskPixelShaderState();
        }
        return lbChanged;
    }
}
