
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredVertex.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

namespace CgsGraphics
{
namespace
{

    const s32 KI_POSITION_ELEMENT_WORD = 0x2C23A5;  // element[0] (position stream)
    const s32 KI_COLOUR_ELEMENT_WORD   = 0x14C86;   // element[1] (packed colour stream)

    const u32 KU_VERTEX_STRIDE      = 12u;
    const u32 KU_FENCE_BYTE_THRESHOLD = 0x80000u;
}
} // namespace CgsGraphics

namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
    void DeviceSetVertexDescriptor(void* lpVertexDescriptor);
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
    lParameters.maElements[0].miOffset      = KI_POSITION_ELEMENT_WORD;
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;
    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 8;
    lParameters.maElements[1].miOffset      = KI_COLOUR_ELEMENT_WORD;
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 0xA;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 4;

    u8 lauDescriptor[48] = {};
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
void Im2dBase<V>::SetTransform(const Im2dTransform& lTransform)
{
    mCurrentTransform = lTransform;
    renderengine::FloatShaderStateIterator lIterator;
    renderengine::Device::BeginShaderStates(maHandleOffsetXYZ[this->mi8CurrentProgram], lIterator);
    *lIterator.m_dest++ = lTransform.mOriginXYZ;
    renderengine::Device::BeginShaderStates(maHandleRightUp[this->mi8CurrentProgram], lIterator);
    *lIterator.m_dest++ = lTransform.mRightUp;
    renderengine::Device::BeginShaderStates(maHandleColourShift[this->mi8CurrentProgram], lIterator);
    *lIterator.m_dest++ = lTransform.mColourShift;
    renderengine::Device::BeginShaderStates(maHandleColourScale[this->mi8CurrentProgram], lIterator);
    *lIterator.m_dest = lTransform.mColourScale;
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

    if ((KU_VERTEX_STRIDE * luCount) > KU_FENCE_BYTE_THRESHOLD)
    {
        D3DDevice_InsertFence(lpDevice);
    }

    void* lpRing = D3DDevice_BeginVertices(lpDevice, leType, luCount, KU_VERTEX_STRIDE);

    u32* lpDst = reinterpret_cast<u32*>(lpRing);
    {
        const u8* lpSrc = reinterpret_cast<const u8*>(lpVertices);
        for (u32 luVertex = 0; luVertex < luCount; ++luVertex)
        {
            const u32* lpVertexWords = reinterpret_cast<const u32*>(lpSrc + luVertex * KU_VERTEX_STRIDE);
            lpDst[0] = lpVertexWords[0];   // mv2Pos.x (float bits)
            lpDst[1] = lpVertexWords[1];   // mv2Pos.y (float bits)
            lpDst[2] = lpVertexWords[2];   // mv4Colour (packed dword)
            lpDst += 3;
        }
    }

    D3DDevice_EndVertices(lpDevice);
}

template void ImRenderer<Basic2dColouredVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<Basic2dColouredVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<Basic2dColouredVertex>::BeginRendering();
template bool ImRenderer<Basic2dColouredVertex>::SetProgram(s8);
template void Im2dBase<Basic2dColouredVertex>::SetTransform(const Im2dTransform&);
template void ImRenderer<Basic2dColouredVertex>::Render(renderengine::PrimitiveType, const Basic2dColouredVertex*, u32);

} // namespace CgsGraphics
