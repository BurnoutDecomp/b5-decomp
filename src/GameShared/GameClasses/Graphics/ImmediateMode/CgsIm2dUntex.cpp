
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

namespace
{
static const u32 kaIm2dUntexVertex[102] = {
    0x102A1101u, 0x00000120u, 0x00000078u, 0x00000000u, 0x00000024u, 0x00000000u, 0x000000E8u, 0x00000000u,
    0x00000000u, 0x000000C0u, 0x0000001Cu, 0x000000B4u, 0xFFFE0300u, 0x00000004u, 0x0000001Cu, 0x00000008u,
    0x000000ADu, 0x0000006Cu, 0x00020003u, 0x00010000u, 0x0000007Cu, 0x00000000u, 0x0000008Cu, 0x00020002u,
    0x00010000u, 0x0000007Cu, 0x00000000u, 0x00000099u, 0x00020000u, 0x00010000u, 0x0000007Cu, 0x00000000u,
    0x000000A4u, 0x00020001u, 0x00010000u, 0x0000007Cu, 0x00000000u, 0x67436F6Cu, 0x6F757253u, 0x63616C65u,
    0x00ABABABu, 0x00010003u, 0x00010004u, 0x00010000u, 0x00000000u, 0x67436F6Cu, 0x6F757253u, 0x68696674u,
    0x00674F66u, 0x66736574u, 0x58595A00u, 0x67526967u, 0x68745570u, 0x0076735Fu, 0x335F3000u, 0x322E302Eu,
    0x36353334u, 0x2E3100ABu, 0x00000000u, 0x00000078u, 0x00010000u, 0x00000000u, 0x00000000u, 0x00001021u,
    0x00000001u, 0x00000002u, 0x00000001u, 0x00000290u, 0x00000003u, 0x0000A007u, 0x0000F0A0u, 0x00001008u,
    0x10011003u, 0x00001200u, 0xC2000000u, 0x00003004u, 0x00001200u, 0xC4000000u, 0x10092007u, 0x00002200u,
    0x00000000u, 0x05F80000u, 0x00000E47u, 0x00000000u, 0x14C8C03Eu, 0x000000C6u, 0xC2000000u, 0xC801803Eu,
    0x00B26E6Cu, 0x91000100u, 0xC802803Eu, 0x00B2B3B1u, 0x91000100u, 0x05F80000u, 0x00000688u, 0x00000000u,
    0xC80F8000u, 0x00000000u, 0x8B000302u, 0x00000000u, 0x00000000u, 0x00000000u,
};
static const u32 kaIm2dUntexPixel[40] = {
    0x102A1100u, 0x0000007Cu, 0x00000024u, 0x00000000u, 0x00000024u, 0x00000000u, 0x00000058u, 0x00000000u,
    0x00000000u, 0x00000030u, 0x0000001Cu, 0x00000023u, 0xFFFF0300u, 0x00000000u, 0x00000000u, 0x00000008u,
    0x0000001Cu, 0x70735F33u, 0x5F300032u, 0x2E302E36u, 0x3533342Eu, 0x3100ABABu, 0x00000000u, 0x00000024u,
    0x10000000u, 0x00000000u, 0x00000000u, 0x00001021u, 0x00000001u, 0x00000001u, 0x0000F0A0u, 0x00000000u,
    0x1001C400u, 0x22000000u, 0xC80F8000u, 0x00000000u, 0xE2000000u, 0x00000000u, 0x00000000u, 0x00000000u,
};
}

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

    rw::ResourceDescriptor lVertexDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lVertexDescriptor, &lVertexParams);

    rw::Resource lVertexLayout = lpAllocator->DoAllocate(lVertexDescriptor, 0);
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

    rw::ResourceDescriptor lPixelDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lPixelDescriptor, &lPixelParams);

    rw::Resource lPixelLayout = lpAllocator->DoAllocate(lPixelDescriptor, 0);
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

// ARTIST827FC5C0: one recovered vertex/pixel package, then the four
// shader handles used by the 2D transform.
void Im2dUntex::Construct(rw::IResourceAllocator* allocator)
{
    mCurrentTransform = Im2dTransform::GetDefault();
    const void* vertices[1] = { kaIm2dUntexVertex };
    const void* pixels[1] = { kaIm2dUntexPixel };
    const u32 vertexSizes[1] = { sizeof(kaIm2dUntexVertex) };
    const u32 pixelSizes[1] = { sizeof(kaIm2dUntexPixel) };
    ImRenderer<Basic2dColouredVertex>::Construct(allocator, vertices, vertexSizes, pixels, pixelSizes, 1);
    const char* names[4] = { "gOffsetXYZ", "gRightUp", "gColourShift", "gColourScale" };
    renderengine::ProgramVariableHandle* handles[4] = { &maHandleOffsetXYZ[0], &maHandleRightUp[0],
        &maHandleColourShift[0], &maHandleColourScale[0] };
    for (u32 i = 0; i < 4; ++i)
    {
        if (!mapVertexProgramBuffer[0])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mapVertexProgramBuffer[ li8Program ] != NULL",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsImRenderer.h", 570);
            CgsDev::Assert::EndAssert();
        }
        renderengine::ProgramBuffer::GetVariableHandleByName(
            reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]),
            reinterpret_cast<const u8*>(names[i]), handles[i]);
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
