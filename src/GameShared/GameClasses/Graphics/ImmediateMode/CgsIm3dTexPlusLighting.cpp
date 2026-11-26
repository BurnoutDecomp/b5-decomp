
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsWorldTexturedVertex.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

static const char KAC_IM_RENDERER_ASSERT_FILE[] =
    "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsImRenderer.h";

namespace CgsGraphics
{
namespace
{

    const u32 KU_POSITION_ELEMENT_WORD = 0x1A23A6u; // element 0 (pos + transform index) @ off 0
    const u32 KU_NORMAL_ELEMENT_WORD   = 0x2A23B9u; // element 1 (normal)                @ off 16
    const u32 KU_TEXCOORD_ELEMENT_WORD = 0x2C23A5u; // element 2 (UVs)                   @ off 28

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

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRendererAddProgram.inl"

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
    lParameters.maElements[0].mu8UsageIndex = 1;
    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 16;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_NORMAL_ELEMENT_WORD);
    lParameters.maElements[1].mu8UsageIndex = 3;
    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = 28;
    lParameters.maElements[2].miOffset      = static_cast<s32>(KU_TEXCOORD_ELEMENT_WORD);
    lParameters.maElements[2].mu8UsageIndex = 6;

    const rw::BaseResourceDescriptors<5> lVertexDescriptor = renderengine::VertexDescriptor::GetResourceDescriptor(lParameters);
    rw::Resource lDescriptorResource;
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &lDescriptorResource, &lVertexDescriptor);
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
    CGS_ASSERT_AT(mapVertexProgramBuffer[0] != 0,
        "mapVertexProgramBuffer[ 0 ] != NULL", KAC_IM_RENDERER_ASSERT_FILE, 588);
    CGS_ASSERT_AT(mapPixelProgramBuffer[0] != 0,
        "mapPixelProgramBuffer[ 0 ] != NULL", KAC_IM_RENDERER_ASSERT_FILE, 589);
    CGS_ASSERT_AT(mgpActiveRenderer == 0,
        "mgpActiveRenderer == NULL", KAC_IM_RENDERER_ASSERT_FILE, 590);
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
    CGS_ASSERT_AT(mapVertexProgramBuffer[li8Program] != 0,
        "mapVertexProgramBuffer[ li8Program ] != NULL", KAC_IM_RENDERER_ASSERT_FILE, 526);
    CGS_ASSERT_AT(mapPixelProgramBuffer[li8Program] != 0,
        "mapPixelProgramBuffer[ li8Program ] != NULL", KAC_IM_RENDERER_ASSERT_FILE, 527);
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

template s8 ImRenderer<BrnGraphics::WorldTexturedVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<BrnGraphics::WorldTexturedVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template void ImRenderer<BrnGraphics::WorldTexturedVertex>::BeginRendering();
template bool ImRenderer<BrnGraphics::WorldTexturedVertex>::SetProgram(s8);

} // namespace CgsGraphics
