#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImNativePrograms.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"

namespace CgsGraphics
{
namespace
{

    const u32 KU_POSITION_ELEMENT_WORD = 0x2A23B9u;
    const u32 KU_COLOUR_ELEMENT_WORD   = 0x14C86u;
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

template void ImRenderer<BasicColouredVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<BasicColouredVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template bool ImRenderer<BasicColouredVertex>::SetProgram(s8);

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
void ImRenderer<V>::EndRendering()
{
    CGS_ASSERT(mgpActiveRenderer == static_cast<ImRendererBase*>(this), "mgpActiveRenderer == this");
    mgpActiveRenderer = 0;
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
    if (16u * luCount > 0x80000u)
        D3DDevice_InsertFence(lpDevice);
    struct PackedVertex { Vector3F mPosition; RGBA8 mColour; };
    CGS_STATIC_ASSERT((sizeof(PackedVertex) == 16), "ARTIST untextured GPU vertex stride");
    PackedVertex* const lpOutput = static_cast<PackedVertex*>(
        D3DDevice_BeginVertices(lpDevice, leType, luCount, sizeof(PackedVertex)));
        for (u32 luVertex = 0; luVertex < luCount; ++luVertex)
            {
                lpOutput[luVertex].mPosition.x = lpVertices[luVertex].mv3Pos.x;
                lpOutput[luVertex].mPosition.y = lpVertices[luVertex].mv3Pos.y;
                lpOutput[luVertex].mPosition.z = lpVertices[luVertex].mv3Pos.z;
                lpOutput[luVertex].mColour = lpVertices[luVertex].mv4Colour;
            }
    D3DDevice_EndVertices(lpDevice);
}

template void ImRenderer<BasicColouredVertex>::BeginRendering();
template void ImRenderer<BasicColouredVertex>::EndRendering();
template void ImRenderer<BasicColouredVertex>::Render(renderengine::PrimitiveType, const BasicColouredVertex*, u32);

void Im3dUntex::Construct(rw::IResourceAllocator* lpAllocator)
{
    static const f32 KAF_IDENTITY[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    for (u32 luByte = 0; luByte < sizeof(mCurrentTransform); ++luByte)
    {
        reinterpret_cast<u8*>(&mCurrentTransform)[luByte] = reinterpret_cast<const u8*>(KAF_IDENTITY)[luByte];
    }

    const void* const lapVertexProgramBinary[1] = { gaIm3dUntexVertexProgram };
    const u32 lauVertexProgramSize[1] = { sizeof(gaIm3dUntexVertexProgram) };
    const void* const lapPixelProgramBinary[1] = { gaIm3dUntexPixelProgram };
    const u32 lauPixelProgramSize[1] = { sizeof(gaIm3dUntexPixelProgram) };
    const s8    KI8_PROGRAMS_BUILT        = 1;
    ImRenderer<BasicColouredVertex>::Construct(
        lpAllocator,
        lapVertexProgramBinary, lauVertexProgramSize,
        lapPixelProgramBinary,  lauPixelProgramSize,
        KI8_PROGRAMS_BUILT);

    renderengine::ProgramBufferData* const lpVertexProgram =
        reinterpret_cast<renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]);
    CGS_ASSERT(lpVertexProgram != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
    renderengine::ProgramBuffer::GetVariableHandleByName(
        lpVertexProgram, reinterpret_cast<const u8*>("worldViewProj"),
        &maWorldViewProjStateHandle[0]);
}

} // namespace CgsGraphics
